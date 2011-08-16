"""Convert a bunch of perpetuum data files to serialized QVariant for PP consumption."""

import re
from PyQt4 import QtCore, QtGui
from perpetuum import *

EXCLUDE_CATEGORIES = [
    'cf_npc', 'cf_documents', 'cf_decor', 'cf_station_services', 'cf_structures',
    'cf_groups', 'cf_mission_items', 'cf_items', 'cf_dynamic_cprg'] # 'cf_dogtags'


def translation_tokens(data):
    """Collect all strings recursively from a given QVatiantMap. (Map keys are not included.)"""

    keys = []
    def _collect(data):
        if data.type() == QtCore.QVariant.Map:
            for value in data.toMap().values():
                _collect(value)
        elif data.type() == QtCore.QVariant.List:
            for value in data.toList():
                _collect(value)
        elif data.type() == QtCore.QVariant.String:
            keys.append(str(data.toString()))
    _collect(data)
    return set(keys)


def find_definitions(data, cat_names):
    """Convert a list of category names to a list of definiton IDs"""

    cat_flags = []
    for category in data['categoryFlags'].itervalues():
        if category['name'] in cat_names:
            cat_flags.append(category['value'])

    result = []
    for definition in data['getEntityDefaults'].itervalues():
        for flag in cat_flags:
            if (definition['categoryflags'] & flag) == flag:
                result.append(definition['definition'])
                break

    return result


def build_variant(exclude_definitions=[]):
    """Builder for genxy_parse that produces QVariants."""

    exclude_definitions = set(exclude_definitions)

    def _build(events, top_level=True):
        d = OrderedDict()
        prefix = None
        for type, value in events:
            if type == '[':
                value = _build(events, False)
                if value is not None:
                    d[key] = value
            elif type == ']':
                break
            elif type in '#|':
                key = value
                if prefix is None and key.endswith('0'):
                    prefix = key[:-1]
            else:
                if type == '$':
                    if key == 'options' and value.strip().startswith('#'):
                        value = genxy_parse(value, lambda events: _build(events, False))
                    else:
                        value = value.decode('utf-8')
                elif type == '2':
                    value = QtCore.QByteArray(value)
                elif type in ('p', '3', 'r'):
                    continue
                elif type == 'c':
                    # BGRA to RGBA
                    value = value & 0xff000000 | (value << 16) & 0xff0000 | value & 0xff00 | (value >> 16) & 0xff
                    value = QtGui.QColor.fromRgba(value)
                elif type == 'd':
                    value = QtCore.QDateTime(QtCore.QDate(*value[:3]), QtCore.QTime(*value[3:]), QtCore.Qt.UTC)
                elif type in ('4', '6', 'N'):
                    value = QtCore.QVariant.fromList(value)
                elif type == '5':
                    value = QtCore.QVariant.fromList([item.decode('utf-8') for item in value])
                d[key] = value

        if d.get('definition') in exclude_definitions:
            return None

        if top_level and len(d) == 1: # flatten single-element root
            d = d.values()[0]

        if isinstance(d, OrderedDict):
            #if all(key.isdigit() for key in d) or prefix and all(key.startswith(prefix) for key in d):
            #    d = QtCore.QVariant.fromList(d.values())
            #else:
            d = QtCore.QVariant.fromMap(d)
        return d

    return _build


def qvariant_dump(filename, variant):
    """Serialize QVariant structure to a file."""

    file = QtCore.QFile(filename)
    if file.open(QtCore.QIODevice.WriteOnly | QtCore.QIODevice.Truncate):
        stream = QtCore.QDataStream(file)
        stream.setVersion(QtCore.QDataStream.Qt_4_7)
        stream.setFloatingPointPrecision(QtCore.QDataStream.SinglePrecision)
        stream.setByteOrder(QtCore.QDataStream.LittleEndian)
        stream << variant
        file.close()
    else:
        raise Exception('Failed to open ' + filename)


def main(dest_dir='compiled'):
    try:
        os.makedirs(dest_dir)
    except OSError:
        pass

    print 'Parsing ...',
    data = genxy_consolidate('fragments')
    translations = genxy_consolidate('translations')
    excluded_defs = find_definitions(data, EXCLUDE_CATEGORIES)
    print 'OK'

    print 'Game data ...',
    variant_data = genxy_consolidate('fragments', build_variant(excluded_defs))
    variant_data = QtCore.QVariant.fromMap(variant_data)
    qvariant_dump(os.path.join(dest_dir, 'game.dat'), variant_data)
    print 'OK'

    translation_keys = translation_tokens(variant_data)

    for lang, translation in translations.items():
        print 'Translation', lang, '...',
        translation_keys = translation_keys.intersection(translation.iterkeys())
        translation = dict((key, translation[key].decode('utf-8')) for key in translation_keys)

        qvariant_dump(os.path.join(dest_dir, 'lang_%s.dat' % lang),
                      QtCore.QVariant.fromMap(translation))
        print 'OK'

    try:
        import yaml
        import yaml_use_ordered_dict
    except ImportError:
        pass
    else:
        print 'YAML dump ...',
        with open(os.path.join(dest_dir, 'game.yaml'), 'wb') as f:
            yaml.dump(data, f)
        print 'OK'


if __name__ == '__main__':
    import sys
    main(*sys.argv[1:])
