# coding: utf-8

import sys
import os
import re
import yaml
import yaml_use_ordered_dict
from collections import defaultdict, OrderedDict, Mapping
import perpetuum
import qvariant


class Translation(dict):

	def __init__(self, filename='re/dictionary.txt'):
		with open(filename, 'rb') as f:
			data = f.read()
		data = data.replace('module_module', 'module')
		data = data.replace('longrange_standard', 'standard_longrange')
		data = data.replace('_armor_repair_unit=', '_armor_repair_modifier_unit=')
		data = data.replace('electornics', 'electronics')
		data = data.replace('–', '-')
		self.update(perpetuum.genxy_parse(data))

		self['energy_vampired_amount_modifier_unit'] = '%'
		if '_ru' in filename:
			self['entityinfo_header_meta'] = 'Мета'
			self['entityinfo_metalevel'] = 'Мета-уровень'
		else:
			self['entityinfo_header_meta'] = 'Meta'
			self['entityinfo_metalevel'] = 'Production level'

		inv_data = defaultdict(list)
		for key, value in self.iteritems():
			inv_data[value].append(key)

		self._inv_data = dict(inv_data)

	def keys(self, value):
		return self._inv_data[value]


TRANS = Translation()
TRANS_LANGS = ('ru', 'hu', 'de', 'it')


def uncomment(text):
	return re.sub(r'(?m)\s*#.*$', '', text)


def compile_game_data(dir='yaml', out_dir='compiled'):

	try:
		os.makedirs(out_dir)
	except OSError:
		pass

	def parse_tree(tree, root_name):
		level_names = {-1: root_name}
		groups = defaultdict(lambda: defaultdict(list))

		for line in tree.splitlines():
			level, name = re.match(r'^(\t*)(.+)$', line).groups()
			level = len(level)

			level_names[level] = name
			group = groups[level_names[level - 1]]

			if name.startswith('extcat_') or name.startswith('cf_') or name.startswith('entityinfo_header'):
				groups[name] # force creation for empty groups
				group['groups'].append(name)
			else:
				group['objects'].append(name)

		return dict((k, dict(v)) for k, v in groups.iteritems())

	def collect_keys(data):
		keys = []
		def _collect(data):
			if isinstance(data, Mapping):
				keys.extend(data.keys())
				for value in data.values():
					_collect(value)
			elif isinstance(data, list):
				for value in data:
					_collect(value)
			elif isinstance(data, basestring):
				keys.append(data)
		_collect(data)

		return frozenset(keys + [k + '_desc' for k in keys] + [k + '_unit' for k in keys])

	def translation_split_data(data):
		split_data = defaultdict(dict)

		for ext_id, extension in data['extensions'].iteritems():
			ext_desc_id = ext_id + '_desc'
			split_data[ext_desc_id] = {ext_desc_id: {'BONUS': extension['modifier_value']}}

		for item_id, item in data['items'].iteritems():
			if not 'desc' in item:
				continue

			item_desc_id = item['desc']
			item_desc = TRANS[item_desc_id]

			item_splits = {}
			for param_id in re.findall(r'\{%([\w\.]+)%\}', item_desc):
				item_splits[param_id] = item['parameters'][param_id]
			if item_splits:
				split_data[item['desc']][item_id + '_desc'] = item_splits

		return dict(split_data)

	def prepare_trans(d, data, split_data, data_keys):
		trans = dict(TRANS)
		trans.update(d)
		trans = dict((k, perpetuum.wiki_to_html(v).decode('utf-8')) for k, v in trans.iteritems() if k in data_keys)

		for src_id, splits in split_data.iteritems():
			for dest_id, values in splits.iteritems():
				def sub_param_value(m):
					param_id = m.group(1)
					if param_id.startswith('BONUS'):
						param_id = 'BONUS'
					else:
						unit = trans.get(param_id + '_unit')
						if unit:
							return '%s %s' % (str(values[param_id]), unit)
					return str(values[param_id])
				trans[dest_id] = re.sub(r'\{%([\w\.]+)%\}', sub_param_value, trans[src_id])

		for attr_id in data['parameters']:
			if attr_id.endswith('_modifier') and not (attr_id + '_unit') in trans:
				trans[attr_id + '_unit'] = u'%'

		return trans

	def write_datafile(name, data):
		with open(os.path.join(out_dir, name + '.yaml'), 'wb') as f:
			f.write(yaml.dump(data, indent=4, default_flow_style=False))

		with open(os.path.join(out_dir, name + '.dat'), 'wb') as f:
			f.write(qvariant.dumps(data))

	data = {'groups': OrderedDict()}
	for filename in os.listdir(dir):
		key = os.path.splitext(filename)[0]
		with open(os.path.join(dir, filename)) as f:
			value = f.read()

		if filename.endswith('.yaml'):
			data[key] = yaml.load(value)
		elif filename.endswith('.tree'):
			data['groups'].update(parse_tree(uncomment(value), key))

	split_data = translation_split_data(data)
	data_keys = collect_keys(data)

	write_datafile('game', data)

	write_datafile('lang_en', prepare_trans(TRANS, data, split_data, data_keys))
	for lang in TRANS_LANGS:
		write_datafile('lang_' + lang, prepare_trans(Translation('re/dictionary_%s.txt' % lang), data, split_data, data_keys))


if __name__ == '__main__':
	import sys
	compile_game_data(*sys.argv[1:])
