"""Publish INI declarations and separate native author-resource files.

No source PFB snapshots, consumer lists, provenance candidates, or execution
strategy switches are part of the published format.
"""
from __future__ import annotations

import configparser
import io
from pathlib import Path
import re
import struct

from .resource_file import (MESH, MATERIAL, PIXELS, digest, resource_path,
                            encode_strings, encode_resource, write_resource, identifier)
from .type_tree import require
from .texture_fields import texture_blocks


def visibility(bindings):
    terms = []
    for binding in bindings:
        values = binding["visible"]
        if not values:
            terms.append("false")
        else:
            choices = " || ".join(f'{binding["variable"]} == {value}' for value in values)
            terms.append("(" + choices + ")" if len(values) > 1 else choices)
    return " && ".join(terms) if terms else "true"


def ini_document(root):
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), comment_prefixes=(";", "#"), strict=True)
    ini.optionxform = str
    ini.read(Path(root) / 'mod.ini', encoding='utf-8-sig')
    return ini


def save_ini(root, ini):
    text = io.StringIO()
    ini.write(text, space_around_delimiters=False)
    (Path(root) / 'mod.ini').write_text('; EFF resource-input Mod format 2\n' + text.getvalue(), encoding='utf-8')


def texture_section(texture):
    section = identifier(texture.get('section') or 'Texture' + texture['sourceAsset'])
    require(section.startswith('Texture'), 'texture section prefix')
    return section


def texture_output_paths(root, ini, textures, materials=()):
    """Plan filenames before writes so partial staging can detach hardlinks."""
    updated = {texture_section(t) for t in textures}
    retained = {s: ini[s]['path'] for s in ini.sections() if s.startswith('Texture') and s not in updated}
    if materials:
        # Match update_directory's removal of superseded bindings. A legacy
        # section no longer used after this update must not reserve its file.
        replaced = {identifier(m['id']) for m in materials}
        used = {v for s in ini.sections() if s.startswith('Material') and s not in replaced
                for k, v in ini[s].items() if k.startswith('texture.')}
        ids = {t['id']: texture_section(t) for t in textures}
        used.update(ids[edit['texture']] for m in materials for edit in m['textureEdits'])
        retained = {s: path for s, path in retained.items() if s in used}
    assigned, paths, data_cache = {}, {}, {}

    def data(texture):
        key = texture['id']
        if key not in data_cache:
            data_cache[key] = encode_resource(PIXELS, '00' * 16, texture_blocks(texture))
        return data_cache[key]

    def conflicts(relative, texture):
        return ((relative in assigned and data(assigned[relative]) != data(texture)) or
                (relative in retained.values() and resource_path(root, relative).read_bytes() != data(texture)))

    for texture in sorted(textures, key=texture_section):
        tid = texture_section(texture)
        label = identifier(texture['pixelLabel'])
        relative = 'textures/' + label + '.tex'
        qualified = 'textures/' + label + '_' + digest(tid.encode('utf-8'))[:12].lower() + '.tex'
        if texture.get('section') and tid in ini and ini[tid]['path'] == qualified:
            relative = qualified
        if conflicts(relative, texture) and texture.get('section'):
            relative = qualified
        require(not conflicts(relative, texture), 'different images use the same TEX filename: ' + relative +
                '; rename the source image or export all declarations sharing it')
        assigned[relative] = texture
        paths[texture['id']] = relative
    return paths


def texture_sections(root, ini, textures, paths=None):
    paths = texture_output_paths(root, ini, textures) if paths is None else paths
    ids, sections = {}, set()
    for texture in textures:
        tid = texture_section(texture)
        require(texture['id'] not in ids, 'duplicate texture ID')
        require(tid not in sections, 'duplicate texture section: ' + tid)
        sections.add(tid)
        ids[texture['id']] = tid
        data = encode_resource(PIXELS, '00' * 16, texture_blocks(texture))
        relative = paths[texture['id']]
        path = resource_path(root, relative)
        path.parent.mkdir(parents=True, exist_ok=True)
        texture['_written'] = not (texture.get('retained') and path.is_file() and path.read_bytes() == data)
        if texture['_written']:
            path.write_bytes(data)
        values = {'path': relative}
        settings = {'linear': ('true' if texture['colorSpace'] == 0 else 'false', 'false'),
                    'filter': (str(texture['filter']), '1'), 'wrap': (str(texture['wrap']), '0'),
                    'aniso': (str(texture['aniso']), '1'), 'mip_bias': (str(texture['mipBias']), '0.0')}
        for key, (value, default) in settings.items():
            if value != default:
                values[key] = value
        ini[tid] = values
    return ids


def update_directory(root, *, parts=(), materials=(), textures=(), texture_only=False, resource_cache=None):
    """Update selected resource kinds in an already complete staging directory."""
    root = Path(root)
    ini = ini_document(root)
    before = {s: dict(ini[s]) for s in ini.sections()}
    if 'key_switch_enabled' not in ini['Mod']:
        ini['Mod']['key_switch_enabled'] = '1'
    old_paths = {ini[s]['path'] for s in ini.sections() if s.startswith(('Mesh', 'Material', 'Texture'))}
    texture_paths = texture_output_paths(root, ini, textures, materials)
    texture_ids = {t['id']: texture_section(t) for t in textures}
    for material in materials:
        mid = identifier(material['id'])
        require(mid in ini, 'partial material export requires an existing material: ' + mid)
        relative = ini[mid]['path']
        path = resource_path(root, relative)
        if not texture_only:
            path.write_bytes(encode_resource(MATERIAL, material['nativeTypeHash'], material['fieldData']))
        else:
            require((ini[mid]['source.path'], ini[mid]['source.asset']) ==
                    (material['sourcePath'], material['sourceAsset']),
                    'material source changed; export materials and textures')
        ini[mid] = {'path': relative, 'source.path': material['sourcePath'], 'source.asset': material['sourceAsset']}
        for edit in material['textureEdits']:
            require(edit['texture'] in texture_ids, 'missing material texture')
            ini[mid]['texture.' + edit['property']] = texture_ids[edit['texture']]
    if materials:
        used_textures = {v for s in ini.sections() if s.startswith('Material') for k, v in ini[s].items() if k.startswith('texture.')}
        for s in list(ini.sections()):
            if s.startswith('Texture') and s not in used_textures:
                ini.remove_section(s)
    texture_sections(root, ini, textures, texture_paths)
    for part in parts:
        mesh_id = identifier(part['object'])
        require(mesh_id in ini, 'new or renamed Mesh requires a full export: ' + mesh_id)
        for section in ini.sections():
            if section.startswith('Part') and ini[section].get('mesh') == mesh_id:
                require(ini[section]['material'] == part['material'],
                        'changing Mesh material slots requires a full export: ' + mesh_id)
                bound_shapes = {key.removeprefix('shape.') for key in ini[section] if key.startswith('shape.')}
                require(bound_shapes <= set(part['shapeNames']),
                        'removing a controlled shape requires a full export: ' + mesh_id)
        relative = ini[mesh_id]['path']
        blocks = dict(part['fieldData'])
        blocks['@geometry'] = struct.pack('<II', part['vertices'], part['indices'])
        blocks['@bones'] = encode_strings(part['bonePaths'])
        blocks['@shapes'] = encode_strings(part['shapeNames'])
        resource_path(root, relative).write_bytes(encode_resource(MESH, part['nativeTypeHash'], blocks))
    current_paths = {ini[s]['path'] for s in ini.sections() if s.startswith(('Mesh', 'Material', 'Texture'))}
    for relative in old_paths - current_paths:
        resource_path(root, relative).unlink()
    if before != {s: dict(ini[s]) for s in ini.sections()}:
        save_ini(root, ini)
    from .ini_reader import read_package
    return read_package(root, resource_cache)


def export_directory(root, *, mod_id, parts, materials, textures, rules, switches, resource_cache=None,
                     key_switch_enabled=True):
    root = Path(root)
    require(root.is_dir() and not any(root.iterdir()), "publication output must be empty")
    identifier(mod_id)
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), comment_prefixes=(";", "#"), strict=True)
    ini.optionxform = str

    def section(name, values):
        identifier(name)
        require(name not in ini, "duplicate published section: " + name)
        ini.add_section(name)
        for key, value in values.items():
            text = str(value)
            require("\n" not in text and "\r" not in text, "multiline INI value")
            ini[name][key] = text

    from .ini_reader import key_switch_enabled as parse_key_switch_enabled
    section("Mod", dict(format=2, id=mod_id, key_switch_enabled=int(parse_key_switch_enabled(key_switch_enabled))))
    constants = {}
    for switch in switches:
        require(switch["variable"] not in constants, "duplicate state declaration")
        constants[switch["variable"]] = switch["default"]
    shapes, shape_variables = [], {}
    for part in parts:
        for control in part["shapeControls"]:
            if not control["enabled"]:
                continue
            token = re.sub(r"[^A-Za-z0-9_]", "_", part["object"])
            variable = "$shape_" + token + "_" + identifier(control["identity"])
            require(variable not in constants, "duplicate shape state")
            constants[variable] = control["default"]
            shape_variables[(part["partId"], control["shape"])] = variable
            shapes.append((variable, control))
    section("Constants", {"persist " + name: value for name, value in constants.items()})
    for index, switch in enumerate(switches, 1):
        section("KeySwitch" + str(index), {"key": switch["key"], "type": "cycle",
                switch["variable"]: ",".join(str(i) for i in range(switch["stateCount"]))})
    for index, (variable, control) in enumerate(shapes, 1):
        section("ShapeControl" + str(index), dict(variable=variable, label=control["label"],
                min=control["minimum"], max=control["maximum"]))
        for direction, key, value in (("Increase", "hotkey_increase", control["maximum"]),
                                      ("Decrease", "hotkey_decrease", control["minimum"])):
            if control[key]:
                section("KeyShape" + str(index) + direction, {"key": control[key], "type": "hold",
                        "speed": control["hotkey_speed"], variable: value})

    texture_ids = texture_sections(root, ini, textures)

    for material in materials:
        mid = identifier(material["id"])
        require(mid.startswith("Material"), "material section prefix")
        relative = "materials/" + mid + ".mat"
        write_resource(root, relative, MATERIAL, material["nativeTypeHash"], material["fieldData"])
        values = {"path": relative, "source.path": material["sourcePath"], "source.asset": material["sourceAsset"]}
        for edit in material["textureEdits"]:
            require(edit["texture"] in texture_ids, "missing material texture")
            key = "texture." + edit["property"]
            require(key not in values, "duplicate material texture property")
            values[key] = texture_ids[edit["texture"]]
        section(mid, values)

    part_ids = {}
    for part in parts:
        mesh_id = identifier(part["object"])
        require(mesh_id.startswith("Mesh"), "mesh section prefix")
        pid = identifier("Part" + mesh_id.removeprefix("Mesh"))
        require(part["partId"] not in part_ids, "duplicate part ID")
        part_ids[part["partId"]] = pid
        relative = "meshes/" + mesh_id + ".mesh"
        blocks = dict(part["fieldData"])
        blocks["@geometry"] = struct.pack("<II", part["vertices"], part["indices"])
        blocks["@bones"] = encode_strings(part["bonePaths"])
        blocks["@shapes"] = encode_strings(part["shapeNames"])
        write_resource(root, relative, MESH, part["nativeTypeHash"], blocks)
        section(mesh_id, dict(path=relative))
        values = dict(mesh=mesh_id, material=part["material"], visible=visibility(part["visibilityBindings"]))
        for shape in part["shapeNames"]:
            variable = shape_variables.get((part["partId"], shape))
            if variable:
                values["shape." + shape] = variable
        section(pid, values)

    for rule in rules:
        values = {"target.path": rule["sourcePath"], "target.asset": rule["sourceAsset"]}
        if rule["hidden"]:
            require(not rule["parts"], "hidden rule contains geometry")
            values["handling"] = "skip"
        else:
            values["parts"] = ",".join(part_ids[pid] for pid in rule["parts"])
        levels = set()
        for target in rule["targets"]:
            level = target["level"]
            require(level >= 0 and level not in levels, "duplicate/invalid LOD target")
            levels.add(level)
            if level == 0:
                require((target["sourcePath"], target["sourceAsset"]) == (rule["sourcePath"], rule["sourceAsset"]),
                        "LOD0 differs from rule target")
            else:
                values[f"lod.{level}.path"] = target["sourcePath"]
                values[f"lod.{level}.asset"] = target["sourceAsset"]
        require(0 in levels, "LOD0 target is required")
        section("Replace" + rule["sourceAsset"], values)

    save_ini(root, ini)
    # The reader is also the publication gate. There is no legacy JSON fallback.
    from .ini_reader import read_package
    return read_package(root, resource_cache)
