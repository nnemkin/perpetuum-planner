"""Convert a bunch of perpetuum data files to serialized QVariant for PP consumption."""

import re
import collections
import json
from perpetuum import *


EXCLUDE_CATEGORIES = [
    'cf_npc', 'cf_documents', 'cf_decor', 'cf_mission_items', 'cf_items', 'cf_dynamic_cprg']


def find_definitions(data, cat_names):
    """Convert a list of category names to a list of definiton IDs"""

    cat_flags = []
    for category in data['categoryFlags'].itervalues():
        if category['name'] in cat_names:
            cat_flags.append(category['value'])

    result = []
    for definition in data['getEntityDefaults'].itervalues():
        for flag in cat_flags:
            if (definition['categoryflags'] & flag) == flag and definition['definitionname'] != 'def_ice':
                result.append(definition['definition'])
                break

    return result


def main(dest_dir='compiled'):
    try:
        os.makedirs(dest_dir)
    except OSError:
        pass

    print 'Parsing ...',
    data = genxy_consolidate('fragments')
    data.update(genxy_consolidate('versioned'))
    translations = genxy_consolidate('translations')
    excluded_defs = find_definitions(data, EXCLUDE_CATEGORIES)
    print 'OK'

    def write_json(data, filename):
        filename = os.path.join(dest_dir, filename)
        # NB: with mixed str/unicode input, we can't dump to the file directly
        data = json.dumps(data, encoding='utf8', separators=(',', ':'),
                          allow_nan=False, ensure_ascii=False)
        with open(filename, 'wb') as f:
            f.write(data.encode('utf-8'))

    print 'Serializing ...',
    write_json(data, 'game.json')
    print 'OK'

    # collect all potential translation strings
    translation_keys = set(translation_tokens(data))

    for lang, translation in translations.items():
        print 'Translation', lang, '...',

        # remove unused translation strings
        translation = dict((key, translation[key].decode('utf-8')) for key in translation
                                if translation_keys.intersection([key, key.lower(), key.replace('_unit', '')]))

        write_json(translation, 'lang_%s.json' % lang)
        print 'OK'


if __name__ == '__main__':
    import sys
    main(*sys.argv[1:])
