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

    json_settings = {'separators': (',', ':'), 'allow_nan': False}

    print 'Serializing ...',
    with open(os.path.join(dest_dir, 'game.json'), 'wb') as f:
        json.dump(data, f, **json_settings)

    print 'OK'

    translation_keys = set(translation_tokens(data))

    for lang, translation in translations.items():
        print 'Translation', lang, '...',

        translation = dict((key, translation[key].decode('utf-8')) for key in translation
                                if translation_keys.intersection([key, key.lower(), key.replace('_unit', '')]))

        with open(os.path.join(dest_dir, 'lang_%s.json' % lang), 'wb') as f:
            json.dump(translation, f, **json_settings)
        print 'OK'


if __name__ == '__main__':
    import sys
    main(*sys.argv[1:])
