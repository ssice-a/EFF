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


def part_resource_id(part_id):
    part_id = identifier(part_id)
    return identifier("Part" + part_id.removeprefix("Mesh"))


def ini_document(root):
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), comment_prefixes=(";", "#"), strict=True)
    ini.optionxform = str
    ini.read(Path(root) / 'mod.ini', encoding='utf-8-sig')
    return ini


def save_ini(root, ini):
    (Path(root) / 'mod.ini').write_bytes(serialize_ini(ini))


def serialize_ini(ini):
    text = io.StringIO()
    ini.write(text, space_around_delimiters=False)
    return ('; EFF resource-input Mod format 2\n' + text.getvalue()).encode('utf-8')


def texture_section(texture):
    section = identifier(texture.get('section') or 'Texture' + texture['sourceAsset'])
    require(section.startswith('Texture'), 'texture section prefix')
    return section


def _texture_payload(texture, cache):
    # Bindings often share the very same immutable mip buffer. Validate each
    # declaration, but encode/checksum that buffer only once in this write.
    blocks = texture_blocks(texture)
    key = (id(blocks['pixels']), blocks['@texture'])
    if key not in cache:
        cache[key] = encode_resource(PIXELS, '00' * 16, blocks)
    return cache[key]


def texture_output_paths(root, ini, textures, materials=(), _payloads=None):
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
    assigned, paths = {}, {}
    data_cache = {} if _payloads is None else _payloads

    def data(texture):
        return _texture_payload(texture, data_cache)

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


def texture_sections(root, ini, textures, paths=None, memory_resources=None):
    payloads = {}
    paths = texture_output_paths(root, ini, textures, _payloads=payloads) if paths is None else paths
    ids, sections = {}, set()
    written = set()
    for texture in textures:
        tid = texture_section(texture)
        require(texture['id'] not in ids, 'duplicate texture ID')
        require(tid not in sections, 'duplicate texture section: ' + tid)
        sections.add(tid)
        ids[texture['id']] = tid
        data = _texture_payload(texture, payloads)
        relative = paths[texture['id']]
        path = resource_path(root, relative)
        texture['_written'] = relative not in written and (memory_resources is not None or
            not (texture.get('retained') and path.is_file() and path.read_bytes() == data))
        if texture['_written']:
            if memory_resources is None:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            else:
                memory_resources[relative] = data
        written.add(relative)
        values = {'path': relative}
        settings = {'linear': ('true' if texture['colorSpace'] == 0 else 'false', 'false'),
                    'filter': (str(texture['filter']), '1'), 'wrap': (str(texture['wrap']), '0'),
                    'aniso': (str(texture['aniso']), '1'), 'mip_bias': (str(texture['mipBias']), '0.0')}
        for key, (value, default) in settings.items():
            if value != default:
                values[key] = value
        ini[tid] = values
    return ids


def update_directory(root, *, parts=(), materials=(), textures=(), texture_only=False, resource_cache=None, material_clips=None, effects=None):
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
        existing_parts = [ini[section] for section in ini.sections()
                          if section.startswith('Part') and ini[section].get('mesh') == mesh_id]
        require(existing_parts, 'Mesh has no existing Part declaration: ' + mesh_id)
        require(part['material'] in {row['material'] for row in existing_parts},
                'changing Mesh material slots requires a full export: ' + mesh_id)
        for row in existing_parts:
            bound_shapes = {key.removeprefix('shape.') for key in row if key.startswith('shape.')}
            require(bound_shapes <= set(part['shapeNames']),
                    'removing a controlled shape requires a full export: ' + mesh_id)
        relative = ini[mesh_id]['path']
        blocks = dict(part['fieldData'])
        blocks['@geometry'] = struct.pack('<II', part['vertices'], part['indices'])
        blocks['@bones'] = encode_strings(part['bonePaths'])
        blocks['@shapes'] = encode_strings(part['shapeNames'])
        resource_path(root, relative).write_bytes(encode_resource(MESH, part['nativeTypeHash'], blocks))
    if parts:
        apply_animations(root, ini, parts, partial=True)
    current_paths = {ini[s]['path'] for s in ini.sections() if s.startswith(('Mesh', 'Material', 'Texture'))}
    for relative in old_paths - current_paths:
        resource_path(root, relative).unlink()
    if before != {s: dict(ini[s]) for s in ini.sections()}:
        save_ini(root, ini)
    if material_clips is not None or effects is not None:
        from .runtime_controls import apply
        apply(root,ini,material_clips,effects,material_ids={m['id'] for m in materials})
    from .ini_reader import read_package
    return read_package(root, resource_cache)


def apply_animations(root, ini, parts, partial=False, memory_resources=None):
    from . import shape_animation
    from .resource_file import read_resource
    selected_vars=set(); mapping={}; planned={}
    for part in parts:
        mesh_id=identifier(part['object'])
        pid=next((s for s in ini.sections() if s.startswith('Part') and ini[s].get('mesh')==mesh_id),None)
        if pid is None: continue
        selected_vars.update(v for k,v in ini[pid].items() if k.startswith('shape.'))
        for control in part.get('shapeControls',[]):
            if not control['enabled']:continue
            shape=control['shape'];key='shape.'+shape;variable=ini[pid].get(key)
            if variable is None:
                variable='$shape_'+re.sub(r'[^A-Za-z0-9_]','_',part['object'])+'_'+identifier(control['identity'])
                ini['Constants']['persist '+variable]=str(control['default']);ini[pid][key]=variable
                section='ShapeControl'+identifier(control['identity'])+'_'+mesh_id
                ini[section]=dict(variable=variable,label=control['label'],min=str(control['minimum']),max=str(control['maximum']))
            mapping[(part['partId'],shape)]=variable;selected_vars.add(variable)
        for clip in part.get('shapeAnimations',[]):
            if clip['id'] not in planned: planned[clip['id']]=(dict(clip),{})
            previous,tracks=planned[clip['id']]
            require(all(previous[k]==clip[k] for k in ('label','key','duration','speed','loop','autoplay')),'copied animation ID has conflicting settings; recreate the playback group')
            for shape,samples in clip['channels'].items():
                variable=mapping.get((part['partId'],shape));require(variable is not None,'animated shape lacks a control binding: '+shape)
                tracks[variable]=samples
    if partial:
        for section in list(ini.sections()):
            if not section.startswith('ShapeAnimation'):continue
            old=shape_animation.decode(read_resource(resource_path(root,ini[section]['path']),shape_animation.KIND))
            if set(old['tracks'])&selected_vars:
                require(set(old['tracks'])<=selected_vars,'partial Mesh export must include all parts in the playback group')
                for constant in list(ini['Constants']):
                    if constant.removeprefix('persist ')==old['speedVariable']:del ini['Constants'][constant]
                path=resource_path(root,ini[section]['path']);ini.remove_section(section);path.unlink()
    for cid,(clip,tracks) in planned.items():
        variable='$anim_speed_'+identifier(cid);clip['speedVariable']=variable
        ini['Constants']['persist '+variable]=str(clip['speed'])
        relative='resources/shape_animation_'+cid+'.bin';path=resource_path(root,relative)
        data=shape_animation.encode(ini['Mod']['id'],clip,list(tracks.items()))
        if memory_resources is None:
            path.parent.mkdir(parents=True,exist_ok=True)
            # Private atomic replacement also preserves any previous hard-linked export.
            temp=path.with_name(path.name+'.tmp');temp.write_bytes(data);temp.replace(path)
        else:
            memory_resources[relative]=data
        ini['ShapeAnimation'+cid]={'path':relative}


def export_directory(root, *, mod_id, parts, materials, textures, rules, switches, resource_cache=None,
                     key_switch_enabled=True, material_clips=None, effects=None,
                     memory_only=False):
    root = Path(root)
    require(root.is_dir() and (memory_only or not any(root.iterdir())), "publication output must be empty")
    memory_resources = {} if memory_only else None
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
    shapes, shape_variables, shape_bindings = [], {}, {}
    for part in parts:
        for control in part["shapeControls"]:
            if not control["enabled"]:
                continue
            token = re.sub(r"[^A-Za-z0-9_]", "_", part["object"])
            binding = (token, identifier(control["identity"]))
            variable = shape_bindings.get(binding)
            if variable is None:
                variable = "$shape_" + token + "_" + binding[1]
                require(variable not in constants, "duplicate shape state")
                constants[variable] = control["default"]
                shape_bindings[binding] = variable
                shapes.append((variable, control))
            shape_variables[(part["partId"], control["shape"])] = variable
    section("Constants", {"persist " + name: value for name, value in constants.items()})
    for index, switch in enumerate(switches, 1):
        declaration = {"key": switch["key"], "type": "cycle",
                       switch["variable"]: ",".join(str(i) for i in range(switch["stateCount"]))}
        if switch.get("label"):
            declaration["label"] = switch["label"]
        section("KeySwitch" + str(index), declaration)
    for index, (variable, control) in enumerate(shapes, 1):
        section("ShapeControl" + str(index), dict(variable=variable, label=control["label"],
                min=control["minimum"], max=control["maximum"]))
        for direction, key, value in (("Increase", "hotkey_increase", control["maximum"]),
                                      ("Decrease", "hotkey_decrease", control["minimum"])):
            if control[key]:
                section("KeyShape" + str(index) + direction, {"key": control[key], "type": "hold",
                        "speed": control["hotkey_speed"], variable: value})

    texture_ids = texture_sections(root, ini, textures, memory_resources=memory_resources)

    for material in materials:
        mid = identifier(material["id"])
        require(mid.startswith("Material"), "material section prefix")
        relative = "materials/" + mid + ".mat"
        if memory_only:
            memory_resources[relative] = encode_resource(MATERIAL, material["nativeTypeHash"], material["fieldData"])
        else:
            write_resource(root, relative, MATERIAL, material["nativeTypeHash"], material["fieldData"])
        values = {"path": relative, "source.path": material["sourcePath"], "source.asset": material["sourceAsset"]}
        for edit in material["textureEdits"]:
            require(edit["texture"] in texture_ids, "missing material texture")
            key = "texture." + edit["property"]
            require(key not in values, "duplicate material texture property")
            values[key] = texture_ids[edit["texture"]]
        section(mid, values)

    part_ids = {}
    written_meshes = {}
    for part in parts:
        mesh_id = identifier(part["object"])
        require(mesh_id.startswith("Mesh"), "mesh section prefix")
        pid = part_resource_id(part["partId"])
        require(part["partId"] not in part_ids, "duplicate part ID")
        part_ids[part["partId"]] = pid
        relative = "meshes/" + mesh_id + ".mesh"
        blocks = dict(part["fieldData"])
        blocks["@geometry"] = struct.pack("<II", part["vertices"], part["indices"])
        blocks["@bones"] = encode_strings(part["bonePaths"])
        blocks["@shapes"] = encode_strings(part["shapeNames"])
        previous = written_meshes.get(mesh_id)
        if previous is None:
            if memory_only:
                memory_resources[relative] = encode_resource(MESH, part["nativeTypeHash"], blocks)
            else:
                write_resource(root, relative, MESH, part["nativeTypeHash"], blocks)
            section(mesh_id, dict(path=relative))
            written_meshes[mesh_id] = (part["nativeTypeHash"], blocks)
        else:
            require(previous == (part["nativeTypeHash"], blocks),
                    "parts sharing a Mesh must have identical geometry")
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

    apply_animations(root, ini, parts, memory_resources=memory_resources)
    if material_clips is not None or effects is not None:
        from .runtime_controls import apply
        apply(root,ini,material_clips,effects,memory_resources=memory_resources)
    if memory_only:
        return dict(_memory_ini=serialize_ini(ini), _memory_resources=memory_resources,
                    _ini=ini, rules=[dict(id='Replace' + rule['sourceAsset'],
                                         sourcePath=rule['sourcePath'],
                                         sourceAsset=rule['sourceAsset'],
                                         hidden=rule['hidden'],
                                         parts=[part_ids[pid] for pid in rule['parts']],
                                         targets=[dict(sourcePath=target['sourcePath'],
                                                       sourceAsset=target['sourceAsset'],
                                                       level=target['level'])
                                                  for target in rule['targets']])
                                     for rule in rules])
    save_ini(root, ini)
    # The reader is also the publication gate. There is no legacy JSON fallback.
    from .ini_reader import read_package
    return read_package(root, resource_cache)
