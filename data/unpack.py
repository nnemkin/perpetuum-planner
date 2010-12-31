#!/usr/bin/env python

import os
import struct
import array
import argparse


class DataFile(object):

	def __init__(self, filename):
		self.filename = filename

		with open(filename, 'rb') as f:
			magic, header_offset, header_length = struct.unpack('8sii', f.read(16))
			if magic != 'GXYDATA\x1a':
				raise Exception('Invalid data file (wrong magic)', magic)

		header = self._get_record(header_offset, header_length)
		self._records = self._parse_header(header)

	def _decode(self, data):
		ret = array.array('B', data)

		for i in xrange(len(data)):
			ret[i] = 0xff & (ret[i] ^ ((i + 1) * (i ^ 0xCA) - 84))

		return ret.tostring()

	def _get_record(self, offset, length):
		with open(self.filename, 'rb') as f:
			f.seek(offset)
			data = f.read(length)
		return self._decode(data)

	def _parse_header(self, header):
		'''
		header record format:
			int numRecords
			for each record:
				char[nameLen] nameSZ, 0
				int offset
				int length
				int unkLength
		'''

		records = {}
		num_records, = struct.unpack('i', header[:4])
		pos = 4
		for i in xrange(num_records):
			name_end = header.find('\0', pos)
			name = header[pos:name_end]
			pos = name_end + 1
			offset, length, unkLength = struct.unpack('iii', header[pos:pos+12])
			pos += 12
			# f1Length = min(13, unkLength)
			# f1 = header[pos:pos+f1Length]
			pos += unkLength

			records[name] = (offset, length)

		return records

	def dump_record(self, name, dest_dir='.'):
		offset, length = self._records[name]
		print '%08x: %s (%.2f KB)' % (offset, name, length / 1024.)

		data = self._get_record(offset, length)

		ext_map = {'\x89PNG': 'png', 'DDS ': 'dds', 'A3MF': 'a3m', '#': 'txt', 'Extended Module': 'xm', 'RIFF': 'wav'}

		for magic, ext in ext_map.items():
			if data.startswith(magic):
				break
		else:
			ext = 'bin'

		rec_filename = os.path.join(dest_dir, '%s.%s' % (name, ext))
		with open(rec_filename, 'wb') as f:
			f.write(data)

	def dump_records(self, dest_dir='.'):
		for name in self._records:
			self.dump_record(name, dest_dir)


def main():
	parser = argparse.ArgumentParser(description='Dump content from Perpetuum.gbf.')
	parser.add_argument('dest_dir', default='.', help='destination directory (default: current diectory)')
	parser.add_argument('-s', '--single', metavar='NAME', help='name of the record to dump (default: dump all)')
	parser.add_argument('-f', '--file', dest='filename', metavar='PATH', required=True, help='path to Perpetuum.gbf')

	args = parser.parse_args()

	data_file = DataFile(args.filename)

	if args.dest_dir != '.':
		try:
			os.makedirs(args.dest_dir)
		except OSError:
			pass

	if args.single:
		data_file.dump_record(args.single, args.dest_dir)
	else:
		data_file.dump_records(args.dest_dir)


if __name__ == '__main__':
	main()
