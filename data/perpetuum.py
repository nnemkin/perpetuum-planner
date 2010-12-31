
import re
import operator
from collections import defaultdict


def genxy_parse(data):

	def _tokenize(data):
		token_re = re.compile(r'(?P<LBRACE>\[)|(?P<RBRACE>\])|(?P<KEY>[\|#][^=]+=)|(?P<NUMBER>n\d+)|(?P<HEX>[Li][\da-fA-F]+)|(?P<STRING>\$[^\r\n\|\]]+)|(?P<WS>[\s\r\n]+)')

		pos = 0
		while pos < len(data):
			m = token_re.match(data, pos)
			if m:
				if m.lastgroup != 'WS':
					yield m.lastgroup, m.group(m.lastgroup)
				pos = m.end()
			else:
				raise Exception('Unrecognized token', data[pos:pos+32])

	def _parse(tokens):
		for tid, token in tokens:
			if tid == 'KEY':
				return token[1:-1], _parse(tokens)
			elif tid == 'LBRACE':
				result = {}
				while True:
					key_value = _parse(tokens)
					if key_value is None:
						break
					result[key_value[0]] = key_value[1]
				return result
			elif tid == 'RBRACE':
				return None
			elif tid == 'NUMBER':
				return int(token[1:])
			elif tid == 'HEX':
				return int(token[1:], 16)
			elif tid == 'STRING':
				return re.sub(r'\\([\da-fA-F]{2})', lambda m: chr(int(m.group(1), 16)), token[1:])
			else:
				raise Exception('Unknown token', tid, token)

	return _parse(_tokenize(data))[1]


def wiki_to_html(wiki):
	wiki = re.sub(r"'''(.+?)'''", r'<b>\1</b>', wiki)
	wiki = re.sub(r"''(.+?)''", r'<i>\1</i>', wiki)
	wiki = re.sub(r"\[\[Help:(.+?)\|(.+?)\]\]", r'<a href="http://www.perpetuum-online.com/Help:\1">\2</a>', wiki)
	wiki = re.sub(r"\[color=(.+?)\](.+?)\[/color\]", r'<font color="\1">\2</font>', wiki)
	wiki = re.sub(r"\r?\n\r?\n", r'</p><p>', wiki)
	if '<p>' in wiki:
		wiki = '<p>%s</p>' % wiki
	return wiki
