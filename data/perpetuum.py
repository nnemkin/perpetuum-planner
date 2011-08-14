"""Utitilies for working with Perpetuum data formats"""

import os
import re
import struct
import array
import fnmatch
from collections import OrderedDict, namedtuple, Mapping


PERPETUUM_LANGUAGES = ['en', 'hu', 'de', 'ru', 'it', 'fr', 'sk']


def genxy_parse(data, builder=None):
    """Parse Perpetuum's JSON-like format and return a top-level mapping.
       `builder` argument allows to customise value construction.
       By default, simple python objects are constructed."""

    def _tokenize(data, pos=0):
        token_re = re.compile(r'''(?xs)
            \[|              # dict start
            \]|              # dict end
            [\|#][^=]+=|     # key
            \$[^\r\n\|\]#]*| # string
            2[\da-fA-F]+|    # hex blob
            f-?\d*(\.\d+)?|  # float
            N-?\d+(\s*,\s*-?\d+)*| # int array
            n-?\d+|                # int
            [dpr3]-?[\da-fA-F]+(?:\.-?[\da-fA-F]+)*|   # hex int tuple
            [64]-?[\da-fA-F]+(?:s*,\s*-?[\da-fA-F]+)*| # hex int list
            5[^\r\n\|\]#]*(?:s*,\s*[^\r\n\|\]#]*)*|    # string list
            [Lic]-?[\da-fA-F]+|  # hex int
            t[\da-fA-F]{8}|      # float in hex int
            (?P<WS>)[\s\r\n]+    # whitespace''')

        while pos < len(data):
            m = token_re.match(data, pos)
            if m:
                if m.lastgroup != 'WS':
                    yield m.group()
                pos = m.end()
            else:
                raise Exception('Unrecognized token', data[pos:pos+32])

    def _parse(tokens):
        for token in tokens:
            type, value = token[0], token[1:]

            if type in '[]|#':
                yield type, value[:-1]
            else:
                if type == '$':
                    value = re.sub(r'\\([\da-fA-F]{2})', lambda m: chr(int(m.group(1), 16)), value)
                elif type == '2':
                    value = value.decode('hex')
                elif type == 'f':
                    value = float(value)
                elif type == 'n':
                    value = int(value)
                elif type == 'N':
                    value = map(int, value.split(','))
                elif type in 'Lic':
                    value = int(value, 16)
                elif type == 't':
                    value = struct.unpack('f', value.decode('hex'))[0]
                elif type == '5':
                    value = [s.strip() for s in value.split(',')]
                elif type in '64':
                    value = [int(tok, 16) for tok in value.split(',')]
                elif type in 'dpr3':
                    value = tuple(int(tok, 16) for tok in value.split('.'))
                else:
                    raise Exception('Unexpected token', type, token)

                yield type, value

    def _build_python(events, top_level=True):
        d = OrderedDict()
        for type, value in events:
            if type == '[':
                d[key] = _build_python(events, False)
            elif type == ']':
                break
            elif type in '#|':
                key = value
            else:
                if key == 'options' and type == '$' and value.strip().startswith('#'):
                    value = _build_python(_parse(_tokenize(value)), False)
                d[key] = value
        if top_level and len(d) == 1: # flatten single-element root
            d = d.values()[0]
        return d

    def _build_xml(events, key='message', raw=False):
        from lxml import etree

        root = el = etree.Element(key)
        for type, value in events:
            if type == '[':
                el = etree.SubElement(el, key)
            elif type == ']':
                el = el.getparent()
            elif type in '|#':
                key = value
            else:
                if type in 'dpr364': # tuple/array
                    value = ','.join(map(str, value))
                elif type == '2':
                    value = value.encode('base64')
                else:
                    value = str(value)

                if key == 'options' and type == '$' and value.strip().startswith('#'):
                    el.append(_build_xml(_parse(_tokenize(value)), key, True))
                else:
                    etree.SubElement(el, key).text = value.decode('utf-8')

        if raw:
            return root
        if len(root) == 1: # flatten single-element root
            root = root[0]
        return root

    if builder is None:
        builder = _build_python
    elif builder in ('xml', 'XML'):
        builder = _build_xml

    try:
        data_start = data.index('#')
    except ValueError:
        return None
    else:
        return builder(_parse(_tokenize(data, data_start)))


def genxy_consolidate(path, builder=None):
    """Parse files in a given directory and return filename->contents dictionary.
       Extensions are stripped from filenames."""
    result = {}
    for filename in os.listdir(path):
        key = os.path.splitext(filename)[0]
        with open(os.path.join(path, filename), 'rb') as f:
            value = genxy_parse(f.read(), builder)
        result[key] = value
    return result


def wiki_to_html(wiki):
    """Convert Perpetuum wiki text fragment to HTML."""

    wiki = re.sub(r"'''(.+?)'''", r'<b>\1</b>', wiki)
    wiki = re.sub(r"''(.+?)''", r'<i>\1</i>', wiki)
    wiki = re.sub(r"\[\[Help:(.+?)\|(.+?)\]\]", r'<a href="http://www.perpetuum-online.com/Help:\1">\2</a>', wiki)
    wiki = re.sub(r"\[color=(.+?)\](.+?)\[/color\]", r'<font color="\1">\2</font>', wiki)
    wiki = re.sub(r"\r?\n\r?\n", r'</p><p>', wiki)
    if '<p>' in wiki:
        wiki = '<p>%s</p>' % wiki
    return wiki


def translation_tokens(data):
    keys = []
    def _collect(data):
        if isinstance(data, Mapping):
            # keys.extend(data.keys())
            for value in data.itervalues():
                _collect(value)
        elif isinstance(data, list):
            for value in data:
                _collect(value)
        elif isinstance(data, basestring):
            keys.append(data)
    _collect(data)
    return set(keys)


class DataFile(object):
    """GBF file reader"""

    def __init__(self, filename):
        self.filename = filename

        with open(filename, 'rb') as f:
            magic, header_offset, header_length = struct.unpack('8sii', f.read(16))
            if magic != 'GXYDATA\x1a':
                raise Exception('Invalid data file (wrong magic)', magic)

        header = self._get_record(header_offset, header_length)
        self._records = self._parse_header(header)

    def _decode(self, data):
        try:
            import numpy as np

            i = np.arange(len(data), dtype=np.byte)
            buf = np.frombuffer(data, np.byte) ^ ((i + 1) * (i ^ -54) - 84)
            return buf.tostring()
        except ImportError:
            buf = array.array('B', data)
            for i in xrange(len(data)):
                buf[i] = 0xff & (buf[i] ^ ((i + 1) * (i ^ 0xca) - 84))
            return buf.tostring()

    def _get_record(self, offset, length):
        with open(self.filename, 'rb') as f:
            f.seek(offset)
            data = f.read(length)
        return self._decode(data)

    def _parse_header(self, header):
        """
        header record format:
            int numRecords
            for each record:
                char[nameLen] nameSZ, 0
                int offset
                int length
                int unkLength
        """

        records = {}
        num_records = struct.unpack_from('i', header)[0]
        pos = 4
        for i in xrange(num_records):
            name_end = header.find('\0', pos)
            name = header[pos:name_end]
            pos = name_end + 1
            offset, length, unkLength = struct.unpack_from('iii', header, pos)
            pos += 12
            # f1Length = min(13, unkLength)
            # f1 = header[pos:pos+f1Length]
            pos += unkLength
            records[name] = (offset, length)
        return records

    PREFIX_MAP = {'\x89PNG': '.png',
                  'DDS ': '.dds',
                  'A3MF': '.a3m',
                  '#': '.txt',
                  '=': '.txt',
                  'Extended Module': '.xm',
                  'RIFF': '.wav',
                  'OggS': '.ogg'}

    def _guess_ext(self, name, data):
        for prefix, ext in self.PREFIX_MAP.iteritems():
            if data.startswith(prefix):
                return ext
        return '.bin'

    CATEGORY_MAP = OrderedDict([
        ('def*.png', 'icons'),
        ('icon*.png', 'icons'),
        ('entityIcon*.png', 'icons'),
        ('noIcon*.png', 'icons'),
        ('gfx_*.png', 'gfx'),
        ('*.a3m', 'models'),
        ('snd_*', 'sound'),
        ('altitude*', 'terrain'),
        ('terrain*', 'terrain'),
        ('altitude0*', 'terrain'),
        ('blocks0*', 'terrain'),
        ('control0*', 'terrain'),
        ('plants0*', 'terrain'),
        ('surface0*', 'terrain'),
        ('tactical*.png', 'tactical_icons'),
        ('font*', 'font'),
        ('textures_*.dds', 'textures'),
        ('corp*.png', 'corp_icons'),
        ('credits.txt', 'misc'),
        ('eula*.txt', 'misc'),
        ('*.txt', 'text_data')])

    def dump_record(self, name, dest_dir, sort=False):
        offset, length = self._records[name]
        print '%08x: %s (%.2f KB)' % (offset, name, length / 1024.)

        data = self._get_record(offset, length)
        name += self._guess_ext(name, data)

        if sort:
            for pattern, category in self.CATEGORY_MAP.iteritems():
                if fnmatch.fnmatch(name, pattern):
                    dest_dir = os.path.join(dest_dir, category)
                    try:
                        os.makedirs(dest_dir)
                    except OSError:
                        pass
                    break

        rec_filename = os.path.join(dest_dir, name)
        with open(rec_filename, 'wb') as f:
            f.write(data)

    def dump_records(self, patterns, dest_dir, sort=False):
        for name in self._records:
            if any(fnmatch.fnmatch(name, pattern) for pattern in patterns):
                self.dump_record(name, dest_dir, sort)


class SkinFile(object):
    """GenxySkin file reader"""

    Rect = namedtuple('Rect', 'image_id, id, x1, y1, x2, y2')

    def __init__(self, filename):
        """ construct definition:
        skin_file = Struct('skin',
            Magic('GXS2'),
            ULInt32('num_images'),
            Array(lambda ctx: ctx.num_images,
                Struct('images',
                    ULInt32('id'),
                    ULInt32('size'),
                    OnDemand(Bytes('data', lambda ctx: ctx.size))
                )
            ),
            ULInt32('num_rects'),
            Array(lambda ctx: ctx.num_rects,
                Struct('rects',
                    ULInt32('image_id'),
                    ULInt32('id'),
                    ULInt32('x1'),
                    ULInt32('y1'),
                    ULInt32('x2'),
                    ULInt32('y2'),
                )
            ),
            ULInt32('dict_size'),
            String('dict', lambda ctx: ctx.dict_size)
        )
        """

        with open(filename, 'rb') as f:
            if f.read(8) != 'GXS2':
                raise Exception('Invalid skin file (wrong magic)')

            self.images = {}
            num_images = struct.unpack('i', f.read(4))[0]
            for i in xrange(num_images):
                image_id, image_size = struct.unpack('ii', f.read(8))
                self.images[image_id] = (f.tell(), image_size)
                f.seek(image_size, os.SEEK_CUR)

            self.rects = []
            num_rects = struct.unpack('i', f.read(4))[0]
            for i in xrange(num_rects):
                rect = self.Rect(*struct.unpack('iiiiii', f.read(24)))
                self.rects.append(rect)

            dict_size = struct.unpack('i', f.read(4))[0]
            self._dict = f.read(dict_size)
