"""Convert a bunch of perpetuum data files to SQLite database."""

import collections
from perpetuum import *
import zlib
import sqlite3


def compressed_size(filename):
    with open(filename, 'rb') as f:
        data = f.read()
        return len(data), len(zlib.compress(data, 9))


def compile_sql(db_path='planner.db'):

    def t_extension_categories(data):
        '''category_id, name'''
        for cat in data['extensionCategoryList'].itervalues():
            yield cat['ID'], cat['name']

    def t_extensions(data):
        '''extension_id, category_id, name, description, rank, price, bonus, primary_attribute, secondary_attribute'''
        for ext in data['extensionGetAll'].itervalues():
            yield ext['extensionID'], ext['category'], ext['name'], ext['description'], ext['rank'], \
                  ext['price'], ext['bonus'], ext['learningAttributePrimary'], ext['learningAttributeSecondary']

    def t_extension_requirements(data):
        '''extension_id, req_extension_id, level'''
        for ext in data['extensionPrerequireList'].itervalues():
            yield ext['extensionID'], ext['requiredExtension'], ext['requiredLevel']

    def t_aggregate_categories(data):
        '''category_id, name'''
        for cat in data['_custom']['aggregateCategories'].itervalues():
            yield cat['ID'], cat['name']

    def t_categories(data):
        '''category_id, path, name, hidden, market'''
        for cat in data['categoryFlags'].itervalues():
            flags_bytes = struct.pack('<Q', cat['value']).rstrip('\x00')
            #parent_value = struct.unpack_from('<Q', flags_bytes[:-1] + '\x00'*8)[0]

            yield cat['value'], buffer(flags_bytes), cat['name'], cat['hidden'], \
                  cat['value'] in data['marketAvailableItems']['categoryflags']

    def t_aggregate_fields(data):
        '''aggregate_id, category_id, name, unit_name, formula, multiplier, offset, digits'''
        for field in data['getAggregateFields'].itervalues():
            yield field['ID'], field['category'], field['name'], field['measurementUnit'], field['formula'], \
                  field['measurementMultiplier'], field['measurementOffset'], field['digits']

    def t_definitions(data):
        '''definition_id, name, description, icon, quantity, attribute_flags, category_id,
            hidden, purchasable, market, volume, repacked_volume, mass'''
        for d in data['getEntityDefaults'].itervalues():
            dp = data['definitionProperties'].get(str(d['definition']))
            if dp and 'copyfrom' in dp:
                dp = data['definitionProperties'][str(dp['copyfrom'])]
            yield d['definition'], d['definitionname'], d.get('descriptiontoken'), \
                  dp and dp.get('icon'), d['quantity'], d['attributeflags'], \
                  d['categoryflags'], d['hidden'], d['purchasable'],\
                  d['definition'] in data['marketAvailableItems']['definition'], \
                  d['volume'], d['repackedvolume'], d['mass']

    def t_definition_aggregates(data):
        '''definition_id, aggregate_id, value'''
        for d in data['getEntityDefaults'].itervalues():
            for agg in d.get('accumulator', {}).itervalues():
                yield d['definition'], agg['ID'], agg['value']

    def t_definition_enabler_extensions(data):
        '''definition_id, extension_id, level'''
        for d in data['getEntityDefaults'].itervalues():
            for ext in d.get('enablerExtension', {}).itervalues():
                yield d['definition'], ext['extensionID'], ext['extensionLevel']

    def t_definition_extensions(data):
        '''definition_id, extension_id'''
        for d in data['getEntityDefaults'].itervalues():
            for ext_id in d.get('extension', []):
                yield d['definition'], ext_id

    def t_definition_components(data):
        '''definition_id, component_id, amount'''
        for d in data['productionComponentsList'].itervalues():
            for c in d['components'].itervalues():
                yield d['definition'], c['definition'], c['amount']

    def t_definition_bonuses(data):
        '''definition_id, aggregate_id, extension_id, bonus, effect_enhancer'''
        for d in data['getEntityDefaults'].itervalues():
            for b in d.get('bonus', {}).itervalues():
                yield d['definition'], b['aggregate'], b['extensionID'], b['bonus'], b['effectenhancer']

    def t_definition_slots(data):
        '''definition_id, slot, flags'''
        for d in data['getEntityDefaults'].itervalues():
            options = d.get('options')
            slotFlags = options.get('slotFlags') if isinstance(options, dict) else None
            for i, flags in enumerate(slotFlags or ()):
                yield d['definition'], i, flags

    def t_definition_research(data):
        '''definition_id, research_level, calibration_program_id'''
        for d in data['getResearchLevels'].itervalues():
            yield d['definition'], d['researchLevel'], d['calibrationProgram']

    CW_LEVELS = [(1, 'race'), (2, 'corporation'), (3, 'school'), (4, 'major'), (5, 'spark')]
    def create_cw_id(level, id):
        return level * 1000 + id

    def t_character_wizard(data):
        '''choice_id, base_choice_id, level, name, description, corporation_id'''
        for level, level_name in CW_LEVELS:
            for row in data['characterWizardData'][level_name].itervalues():
                for parent_level, parent_level_name in  CW_LEVELS:
                    parent_id = row.get(parent_level_name + 'ID')
                    if parent_id is not None:
                        parent_id = create_cw_id(level, parent_id)
                        break

                yield create_cw_id(level, row['ID']), parent_id, \
                      level, row['name'], row['description'], row.get('baseEID')

    def t_character_wizard_extensions(data):
        '''choice_id, extension_id, level'''
        for level, level_name in CW_LEVELS:
            for row in data['characterWizardData'][level_name].itervalues():
                for ext in row.get('extension', {}).itervalues():
                    yield create_cw_id(level, row['ID']), ext['extensionID'], ext['add']

    def t_character_wizard_attributes(data):
        '''choice_id, attribute_id, value'''
        for level, level_name in CW_LEVELS:
            for row in data['characterWizardData'][level_name].itervalues():
                for i, attr in enumerate('ABCDEF'):
                    yield create_cw_id(level, row['ID']), i+1, row['attribute' + attr]

    def t_attributes(data):
        '''attribute_id, name'''
        for attr in data['_custom']['attributes'].itervalues():
            yield attr['ID'], attr['name']

    def t_translations(data):
        '''name, en, hu, de, ru, it, fr, sk'''
        translations = genxy_consolidate('translations')
        names = translation_tokens(data) & set(translations['en'])
        for name in names:
            yield [name] + [translations[lang][name] for lang in PERPETUUM_LANGUAGES]

    if os.path.exists(db_path):
        os.unlink(db_path)

    conn = sqlite3.connect(db_path)
    conn.text_factory = str # UTF-8 IO
    with open('planner.sql', 'rb') as f:
        conn.executescript(f.read())

    data = genxy_consolidate('fragments')
    for name, func in locals().iteritems():
        if name.startswith('t_'):
            name = name[2:]
            print 'Table', name, '... ',
            num_fields = func.__doc__.count(',') + 1
            sql = 'INSERT INTO %s (%s) VALUES (%s)' % (name, func.__doc__, ','.join('?' * num_fields))
            conn.executemany(sql, func(data))
            print 'OK'

    conn.commit()
    conn.execute('ANALYZE main')
    conn.close()

    size, comp_size = compressed_size(db_path)

    print '%s ready, %.2f MB (compressed %.2f MB)' \
        % (os.path.basename(db_path), size / (1024. * 1024.), comp_size / (1024. * 1024.))


if __name__ == '__main__':
    import sys
    compile_sql(*sys.argv[1:])
