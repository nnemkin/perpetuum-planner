"""Collection of command line tools for working with Perpetuum data."""

import argparse
from collections import defaultdict
from perpetuum import *


def command_data(args):
    """GBF file operations"""

    data = DataFile(args.input)
    try:
        os.makedirs(args.output)
    except OSError:
        pass
    data.dump_records(args.names or ['*'], args.output, args.sort)


def command_skin(args):
    """Skin file operations"""

    from PyQt4 import QtCore, QtGui
    app = QtGui.QApplication([]) # needed to access fonts

    skin = SkinFile(args.input)

    for image in skin.images:
        with open('image_%04d.png' % image.id, 'wb') as f:
            f.write(image.skin.value)

        qimage = QtGui.QImage()
        qimage.loadFromData(image.data.value, 'PNG')

        for rect in skin.rects:
            if rect.image_id == image.id:
                qrect = QtCore.QRect(rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1)
                qimage.copy(qrect).save('part_%04d_%04d.png' % (rect.id, rect.image_id), 'PNG')

        painter = QtGui.QPainter(qimage)
        painter.setFont(QtGui.QFont('Arial Narrow', 8))

        for rect in skin.rects:
            if rect.image_id == image.id:
                qrect = QtCore.QRect(rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1)
                painter.setPen(QtCore.Qt.red)
                painter.drawRect(qrect)
                painter.setPen(QtCore.Qt.green)
                painter.drawText(qrect, QtCore.Qt.AlignTop, unicode(rect.id))

        painter.end()
        qimage.save(QtCore.QFile('parts_%04d.png' % image.id), 'PNG')


def command_wire(args):
    """Wire dump operations"""

    with open(args.input, 'rb') as f:
        records = f.read().decode('utf-16').encode('utf-8').split('\x00')

        try:
            os.makedirs(args.output)
        except OSError:
            pass

        use_counts = defaultdict(int)

        for record in records:
            if record.find('#') != -1:
                name = record[:record.index(':')]
                if args.format == 'xml':
                    from lxml import etree
                    record = etree.tostring(genxy_parse(record, 'xml'),
                                            xml_declaration=True,
                                            encoding='UTF-8',
                                            pretty_print=True)
                    ext = 'xml'
                elif args.format == 'yaml':
                    import yaml
                    import yaml_use_ordered_dict
                    record = yaml.dump(genxy_parse(record))
                    ext = 'yaml'
                else:
                    record = record[record.index('#'):].replace('#', '\r\n#').strip()
                    ext = 'txt'

                use_counts[name] += 1
                if use_counts[name] > 1:
                    name = '%s_%d' % (name, use_counts[name])

                out_filename = os.path.join(args.output, '%s.%s' % (name, ext))
                with open(out_filename, 'wb') as f:
                    f.write(record)


def main():
    parser = argparse.ArgumentParser(description='Perpetuum data utilities.')
    subparsers = parser.add_subparsers(help='Available commands')

    subparser = subparsers.add_parser('data', help='Dump content from Perpetuum.gbf.')
    subparser.add_argument('-i', '--input', metavar='PATH', required=True, help='Input file path')
    subparser.add_argument('-o', '--output', metavar='PATH', default='.', help='Output directory')
    subparser.add_argument('-n', '--name', dest='names', action='append', metavar='NAMES',
                           help='Record name to extract. Can be used multiple times. Glob patterns are supported.')
    subparser.add_argument('--sort', action='store_true', default=False,
                           help='Create subfolders and sort extracted files by category.')
    subparser.set_defaults(command=command_data)

    subparser = subparsers.add_parser('skin', help='Parse skin file.')
    subparser.add_argument('-i', '--input', metavar='PATH', required=True, help='Input file path')
    subparser.add_argument('-o', '--output', metavar='PATH', default='.', help='Output directory')
    subparser.set_defaults(command=command_skin)

    subparser = subparsers.add_parser('wire', help='Parse wire dump.')
    subparser.add_argument('-i', '--input', metavar='PATH', required=True, help='Input file path')
    subparser.add_argument('-o', '--output', metavar='PATH', default='.', help='Output directory')
    subparser.add_argument('-f', '--format', choices=['raw', 'xml', 'yaml'], default='raw', help='Output format')
    subparser.set_defaults(command=command_wire)

    args = parser.parse_args()
    args.command(args)


if __name__ == '__main__':
    main()
