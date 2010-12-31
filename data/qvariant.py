
import sys
import binascii
from collections import OrderedDict, Mapping
from PyQt4 import QtCore

def _tovariant(data):
	if isinstance(data, list):
		return QtCore.QVariant.fromList(map(_tovariant, data))
	elif isinstance(data, Mapping):
		return QtCore.QVariant.fromMap(dict((k.decode('utf-8'), _tovariant(v)) for k, v in data.iteritems()))
	elif isinstance(data, str):
		return data.decode('utf-8')
	else:
		return data

def dumps(data):
	buf = QtCore.QBuffer()
	buf.open(QtCore.QIODevice.WriteOnly)
	stream = QtCore.QDataStream(buf)
	stream.setVersion(QtCore.QDataStream.Qt_4_7)
	stream.setFloatingPointPrecision(QtCore.QDataStream.SinglePrecision)
	stream.setByteOrder(QtCore.QDataStream.LittleEndian)
	stream << _tovariant(data)
	buf.close()

	return buf.buffer().data()
