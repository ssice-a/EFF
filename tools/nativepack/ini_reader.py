"""Strict reader/linker for INI format 2. It does not load game objects."""
from __future__ import annotations

import configparser
from pathlib import Path
import re
import struct

from .delta_apply import GEOMETRY_FIELDS
from .resource_file import (MESH, MATERIAL, PIXELS, resource_path,
                            decode_resource, decode_strings, identifier, read_resource)
from .state_rules import Condition, VARIABLE, number
from .type_tree import require
from .texture_fields import texture_byte_size, texture_metadata


def read_package(root, resource_cache=None):
    root = Path(root).resolve()
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), comment_prefixes=(";", "#"), strict=True)
    ini.optionxform = str
    with (root / "mod.ini").open(encoding="utf-8-sig") as stream:
        ini.read_file(stream)
    require(not ini.defaults(), "DEFAULT inheritance is not supported")
    resources, allowed = {}, {"mod.ini"}

    def keys(name, required=(), optional=(), prefixes=()):
        section = ini[name]
        missing = set(required) - section.keys()
        require(not missing, "missing declaration in " + name + ": " + str(sorted(missing)))
        for key in required:
            require(section[key].strip(), "empty declaration in " + name + ": " + key)
        require(all(key in required or key in optional or any(key.startswith(p) for p in prefixes)
                    for key in section), "unknown declaration in " + name)
        return section

    def load(relative, kind):
        require(relative.split("/", 1)[0] == {MESH: "meshes", MATERIAL: "materials",
                PIXELS: "textures"}.get(kind, "resources"), "resource folder/kind mismatch")
        path = resource_path(root, relative)
        if relative not in resources:
            resources[relative] = read_resource(path, kind, resource_cache)
        require(not kind or resources[relative]["kind"] == kind, "resource kind alias mismatch")
        allowed.add(relative)
        return resources[relative]

    def descriptors(relative, resource):
        return [dict(row, file=relative) for row in resource["descriptors"] if not row["name"].startswith("@")]

    require("Mod" in ini and "Constants" in ini, "Mod/Constants sections are required")
    mod = keys("Mod", ("format", "id"), ("key_switch_enabled",))
    identifier(mod["id"])
    known = {"Mod", "Constants"}
    states, persistence = {}, set()
    for key, value in ini["Constants"].items():
        persistent = key.startswith("persist ")
        variable = key.removeprefix("persist ") if persistent else key
        require(VARIABLE.fullmatch(variable) and variable not in states, "invalid/duplicate state declaration")
        states[variable] = number(value)
        if persistent:
            persistence.add(variable)

    mesh_rows, materials, textures, parts, controls, bindings, rules = {}, [], [], [], [], [], []
    for name in ini.sections():
        if name in known:
            continue
        identifier(name)
        known.add(name)
        if name.startswith("Resource"):
            values = keys(name, ("path",))
            load(values['path'], 0)
        elif name.startswith("Mesh"):
            values = keys(name, ("path",))
            resource = load(values["path"], MESH)
            blocks = resource["blocks"]
            require(set(blocks) == set(GEOMETRY_FIELDS) | {"@geometry", "@bones", "@shapes"},
                    "incomplete/unknown mesh resource blocks")
            require(len(blocks["@geometry"]) == 8, "invalid geometry metadata")
            vertices, indices = struct.unpack("<II", blocks["@geometry"])
            require(vertices > 0 and indices > 0 and indices % 3 == 0, "invalid geometry counts")
            bones, shapes = decode_strings(blocks["@bones"]), decode_strings(blocks["@shapes"])
            require(len(bones) == len(set(bones)) and all(bones), "duplicate/empty bone binding paths")
            require(len(shapes) == len(set(shapes)) and all(shapes), "duplicate/empty shape channels")
            mesh_rows[name] = dict(object=name, vertices=vertices, indices=indices, bonePaths=bones,
                    shapeNames=shapes, nativeTypeHash=resource["nativeTypeHash"], nativeFields=descriptors(values["path"], resource))
        elif name.startswith("Material"):
            values = keys(name, ("path", "source.path", "source.asset"), prefixes=("texture.",))
            resource = load(values["path"], MATERIAL)
            require(all(not field.startswith("@") for field in resource["blocks"]), "invalid material block")
            materials.append(dict(id=name, sourcePath=values["source.path"], sourceAsset=values["source.asset"],
                    nativeTypeHash=resource["nativeTypeHash"], nativeFields=descriptors(values["path"], resource),
                    textureEdits=[dict(property=key.removeprefix("texture."), texture=value)
                                  for key, value in values.items() if key.startswith("texture.")]))
        elif name.startswith("Texture"):
            values = keys(name, ('path',), ('source.path', 'source.asset', 'width', 'height', 'format', 'mip_count',
                                           'linear', 'filter', 'wrap', 'aniso', 'mip_bias'))
            pixels = load(values["path"], PIXELS)
            require(pixels['nativeTypeHash'] == '00'*16, 'invalid pixel resource')
            spec = texture_metadata(pixels, values)
            integers = {}
            for field, default in (('filter', '1'), ('wrap', '0'), ('aniso', '1')):
                value = values.get(field, default)
                require(re.fullmatch(r'[0-9]+', value), 'invalid integer texture declaration: ' + field)
                integers[field] = int(value)
            require(values.get('linear', 'false') in ('true', 'false'), 'linear must be true or false')
            require(('source.path' in values) == ('source.asset' in values), 'incomplete legacy texture source')
            row = dict(next(d for d in pixels['descriptors'] if d['name'] == 'pixels'), file=values['path'])
            texture = dict(row, id=name, sourcePath=values.get('source.path', ''), sourceAsset=values.get('source.asset', ''),
                    **spec, colorSpace=0 if values.get('linear', 'false') == 'true' else 1,
                    filter=integers["filter"], wrap=integers["wrap"], aniso=integers["aniso"],
                    mipBias=number(values.get('mip_bias', '0')))
            require(len(pixels["blocks"]["pixels"]) == texture_byte_size(texture),
                    "pixel size does not match TEX metadata")
            textures.append(texture)
        elif name.startswith("Part"):
            values = keys(name, ("mesh", "material", "visible"), prefixes=("shape.",))
            condition = Condition(values["visible"])
            require(condition.variables <= states.keys(), "visibility reads undefined state in " + name)
            parts.append(dict(partId=name, mesh=values["mesh"], material=values["material"],
                    visible=condition, shapeBindings={key.removeprefix("shape."): value for key, value in values.items()
                                                      if key.startswith("shape.")}))
        elif name.startswith("ShapeControl"):
            values = keys(name, ("variable", "label", "min", "max"))
            variable = values["variable"]
            require(variable in states, "shape control uses undefined state")
            low, high = number(values["min"]), number(values["max"])
            require(low < high and low <= states[variable] <= high, "invalid shape range/default")
            controls.append(dict(variable=variable, label=values["label"], minimum=low, maximum=high))
        elif name.startswith("Key"):
            values = keys(name, ("key", "type"), ("speed",), ("$",))
            assignments = [(key, value) for key, value in values.items() if key.startswith("$")]
            require(len(assignments) == 1, "one key binding must update one state")
            variable, text = assignments[0]
            require(variable in states and VARIABLE.fullmatch(variable), "key writes undefined state")
            require(values["type"] in ("cycle", "hold", "press") and values["key"], "invalid key binding")
            numbers = [number(value.strip()) for value in text.split(",")]
            if values["type"] == "cycle":
                require(len(numbers) >= 2 and len(set(numbers)) == len(numbers) and states[variable] in numbers,
                        "invalid cycle states/default")
                require("speed" not in values, "cycle binding cannot contain speed")
            else:
                require(len(numbers) == 1, "press/hold binding requires one target")
            speed = number(values.get("speed", "1"))
            require(speed > 0 and (values["type"] == "hold" or "speed" not in values), "invalid key speed")
            bindings.append(dict(id=name, key=values["key"], type=values["type"], variable=variable, values=numbers, speed=speed))
        elif name.startswith("Replace"):
            values = keys(name, ("target.path", "target.asset"), ("parts", "handling"), ("lod.",))
            hidden = values.get("handling") == "skip"
            require(values.get("handling") in (None, "skip") and hidden == ("parts" not in values),
                    "rule must declare parts or handling=skip")
            part_ids = [] if hidden else [identifier(value.strip()) for value in values["parts"].split(",")]
            require(len(part_ids) == len(set(part_ids)), "duplicate replacement part")
            targets = [dict(level=0, sourcePath=values["target.path"], sourceAsset=values["target.asset"])]
            lods = {}
            for key, value in values.items():
                if not key.startswith("lod."):
                    continue
                match = re.fullmatch(r"lod\.([1-9][0-9]*)\.(path|asset)", key)
                require(match, "invalid LOD declaration")
                lods.setdefault(int(match[1]), {})[match[2]] = value
            for level, target in sorted(lods.items()):
                require(set(target) == {"path", "asset"}, "incomplete LOD identity")
                targets.append(dict(level=level, sourcePath=target["path"], sourceAsset=target["asset"]))
            rules.append(dict(id=name, sourcePath=values["target.path"], sourceAsset=values["target.asset"],
                    hidden=hidden, parts=part_ids, targets=targets))
        else:
            raise ValueError("unknown INI section: " + name)

    by_material, by_texture, by_part = ({m["id"]: m for m in materials}, {t["id"]: t for t in textures},
                                       {p["partId"]: p for p in parts})
    require(parts and rules, "Mod requires parts and replacement declarations")
    require(len({c["variable"] for c in controls}) == len(controls), "duplicate shape controls")
    shape_controls = {c["variable"]: c for c in controls}
    used_meshes, used_materials, used_textures, used_shapes, used_parts, target_ids = set(), set(), set(), set(), set(), set()
    for part in parts:
        require(part["mesh"] in mesh_rows and part["material"] in by_material, "unresolved part resource")
        part.update(mesh_rows[part["mesh"]])
        used_meshes.add(part["mesh"]); used_materials.add(part["material"])
        for shape, variable in part["shapeBindings"].items():
            require(shape in part["shapeNames"] and variable in shape_controls, "unresolved part shape binding")
            used_shapes.add(variable)
        part["initialVisible"] = part["visible"].evaluate(states)
    for material in materials:
        for edit in material["textureEdits"]:
            require(edit["property"] and edit["texture"] in by_texture, "unresolved material texture")
            used_textures.add(edit["texture"])
    for rule in rules:
        require(set(rule["parts"]) <= by_part.keys(), "unresolved replacement part")
        used_parts.update(rule["parts"])
        for target in rule["targets"]:
            pair = (target["sourcePath"], target["sourceAsset"])
            require(all(pair) and pair not in target_ids, "empty/duplicate replacement target identity")
            target_ids.add(pair)
    require(used_meshes == mesh_rows.keys() and used_materials == by_material.keys() and
            used_textures == by_texture.keys() and used_shapes == shape_controls.keys() and used_parts == by_part.keys(),
            "unreferenced resource/control declaration")
    for variable in states:
        cycles = [b for b in bindings if b["variable"] == variable and b["type"] == "cycle"]
        if cycles:
            domain = set(cycles[0]["values"])
            require(all(set(b["values"]) == domain for b in cycles), "keys sharing state must use the same cycle domain")
            require(all(set(b["values"]) <= domain for b in bindings if b["variable"] == variable),
                    "key can move shared state outside cycle domain")
        if variable in shape_controls:
            control = shape_controls[variable]
            require(all(control["minimum"] <= v <= control["maximum"] for b in bindings if b["variable"] == variable for v in b["values"]),
                    "shape key target outside control range")
    for generated in ("source-inputs.bin", "compiled.bin"):
        if (root / generated).is_file():
            allowed.add(generated)
    actual = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()
              and not re.fullmatch(r"state\.ini(?:\.tmp-[0-9]+)?", p.relative_to(root).as_posix())
              and p.suffix.lower() not in (".png", ".psd") and p.name.lower() != "eff_texture_convert.exe"}
    require(actual == allowed, "unreferenced/source files in Mod directory: " + str(sorted(actual ^ allowed)))
    return dict(format="EFF_NATIVE_MOD_DIRECTORY", version=2, modId=mod["id"], keySwitchEnabled=key_switch_enabled(mod.get("key_switch_enabled", "1")), parts=parts, materials=materials,
                textures=textures, rules=rules, states=states, persistentStates=persistence, keyBindings=bindings,
                shapeControls=controls, _resources=resources, _files=allowed)


def key_switch_enabled(value="1"):
    value = str(value).lower()
    require(value in ("0", "1", "false", "true"), "key_switch_enabled must be 0/1 or false/true")
    return value in ("1", "true")
