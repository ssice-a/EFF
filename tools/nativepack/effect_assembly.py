"""Effect assembly syntax; native layout stays inside the effect resource.

The expanded declarations are an in-memory compatibility view, never a second
manifest on disk. Old EffectAttachment sections keep their existing meaning.
"""
from .type_tree import require

LAYOUT = 'EFF_EFFECT_LAYOUT/1'


def write_layout(writer, native):
    writer.string(LAYOUT)
    writer.string(native['prefab'])
    writer.string(native['rootName'])
    writer.integer(native['root'])
    writer.integer(native['particles'])
    for name in ('gameResources', 'authorResources'):
        rows = native.get(name, ())
        writer.integer(len(rows))
        for row in rows:
            writer.string(row.get('relocationCab', ''))
            writer.integer(row.get('relocationId', 0))
            patches = row.get('relocations', ())
            writer.integer(len(patches))
            for patch in patches:
                for field in ('bundle', 'offset', 'kind'):
                    writer.integer(patch[field])


def read_layout(reader):
    require(reader.string() == LAYOUT, 'effect native layout version differs')
    layout = dict(prefab=reader.string(), rootName=reader.string(),
                  root=reader.integer(), particles=reader.integer())
    for name in ('gameResources', 'authorResources'):
        count = reader.integer()
        require(count <= 4096, 'too many effect resource bindings')
        rows = []
        for _ in range(count):
            cab, pid, count = reader.string(), reader.integer(), reader.integer()
            require(count <= 65536 and bool(cab) == bool(pid), 'invalid effect relocation')
            require(not cab or (len(cab) == 36 and cab.startswith('CAB-') and
                    all(c in '0123456789abcdefABCDEF' for c in cab[4:]) and count),
                    'invalid effect relocation marker')
            patches = [dict(bundle=reader.integer(), offset=reader.integer(),
                            kind=reader.integer()) for _ in range(count)]
            require(all(p['kind'] in (0, 1) for p in patches), 'invalid effect relocation kind')
            rows.append(dict(relocationCab=cab, relocationId=pid, relocations=patches))
        layout[name] = rows
    return layout


def binding_section(section):
    return section.startswith(('AttachEffect', 'ReplaceEffect'))


def expand(root, ini):
    """Resolve public relationships to the common runtime/editor declaration."""
    from . import author_format3 as fmt
    import configparser
    expanded = configparser.ConfigParser(interpolation=None, delimiters=('=',), strict=True)
    expanded.optionxform = str
    for section in ini.sections():
        if not binding_section(section):
            expanded[section] = dict(ini[section])
            continue
        row = dict(ini[section])
        resource = row.pop('effect', '')
        require(resource in ini and ini[resource].get('type') == 'effect',
                'effect relationship has no Effect resource: ' + section)
        declaration = dict(ini[resource])
        layout = fmt.read_effect_metadata(root, declaration['path']).get('layout')
        require(layout is not None, 'effect resource has no native layout: ' + resource)
        values = {k: v for k, v in declaration.items()
                  if k == 'bundles' or k.startswith(('game.', 'author.'))}
        # Author data is optional for runtime use, required only for editing.
        if declaration.get('author'):
            values['author'] = declaration['author']
        values.update(prefab=layout['prefab'], root_name=layout['rootName'],
                      root=str(layout['root']), particles=str(layout['particles']))
        values.setdefault('bundles', resource)
        for prefix, name in (('game.', 'gameResources'), ('author.', 'authorResources')):
            declared = {int(k.split('.')[1]) for k in declaration if k.startswith(prefix)}
            require(declared == set(range(len(layout[name]))),
                    'effect binding declarations/layout differ: ' + resource)
            for i, relocation in enumerate(layout[name]):
                p = prefix + str(i) + '.'
                if relocation['relocationCab']:
                    values[p + 'relocation_cab'] = relocation['relocationCab']
                    values[p + 'relocation_id'] = str(relocation['relocationId'])
                    for j, patch in enumerate(relocation['relocations']):
                        for field in ('bundle', 'offset', 'kind'):
                            values[p + 'patch.' + str(j) + '.' + field] = str(patch[field])
        require('mode' not in row and 'bundles' not in row,
                'effect relationships use AttachEffect/ReplaceEffect syntax: ' + section)
        if section.startswith('ReplaceEffect'):
            identity = section[len('ReplaceEffect'):]
            row['target.prefab'] = row.pop('target.path', '')
            row['mode'] = 'replace_game_prefab'
        else:
            identity = section[len('AttachEffect'):]
            row['mode'] = 'attach_mesh'
        require(identity, 'effect relationship name is empty')
        values.update(row)
        target = 'EffectAttachment' + identity
        require(target not in expanded, 'duplicate effect relationship: ' + target)
        expanded[target] = values
    return expanded
