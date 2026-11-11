"""Strict offline migration from native Format 2 directories to Format 3."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

from . import author_format3
from .author_format3 import read_format3_directory, resolve_templates, write_format3_directory
from .author_source import AuthorCore, AuthorSource
from .ini_reader import read_package
from .mesh import decode_native_mesh
from .type_tree import decode, encode, require


def _source_packages(workspace, core, fallback_workspace=None):
    roots = [(Path(workspace).resolve() / "assets/beyond/dynamicassets/gameplay/actors/postmodels/characters", 0)]
    if fallback_workspace is not None:
        fallback = Path(fallback_workspace).resolve() / "assets/beyond/dynamicassets/gameplay/actors/postmodels/characters"
        if fallback not in [root for root, _ in roots]:
            roots.append((fallback, 1))
    sources = []
    seen = set()
    for root, priority in roots:
        for path in sorted(root.glob("chr_*_postmodel.eff"), key=lambda item: item.name.casefold()):
            resolved = path.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            source = AuthorSource(resolved, core)
            source.migration_priority = priority
            sources.append(source)
    require(sources, "AnimeStudio EFFSOURCE packages are missing from the selected workspace")
    return sources


def _effect_author_files(effects, source_root, eiem_format):
    """Reuse an exact AnimeStudio effect source when the workspace has one."""
    if not effects:
        return {}
    import eiem_effect_document
    candidates = {}
    for path in Path(source_root).resolve().rglob("effect.json"):
        if ".eff-backups" in path.parts:
            continue
        try:
            state = eiem_effect_document.load_source(path.parent)
            prefab = state["catalog"].get("prefab")
        except (OSError, ValueError, KeyError, json.JSONDecodeError):
            continue
        if prefab:
            candidates.setdefault(prefab, []).append((path.parent, state))
    files = {}
    for effect in effects:
        prefab = effect.get("native", {}).get("prefab", "")
        matches = candidates.get(prefab, [])
        require(len(matches) == 1,
                "Format 2 Effect has no unique AnimeStudio source snapshot: " + prefab)
        source, workspace_state = matches[0]
        attachment = {key: value for key, value in effect.items() if key != "native"}
        files[author_format3.identifier(effect["id"]) + ".effeffect"] = (
            eiem_format.encode_format3_effect_source(source, workspace_state, attachment))
    return files


def _verify_effect_resource_payload(path, relative, native_data):
    """Ensure Format 3 wraps the legacy game Bundle without changing its bytes."""
    resource_id = "ResourceEffect_" + hashlib.sha256(relative.encode("utf-8")).digest()[:8].hex().upper()
    with Path(path).open("rb") as stream:
        header = stream.read(author_format3.HEADER.size)
        require(len(header) == author_format3.HEADER.size,
                "migrated Effect resource header is truncated: " + relative)
        magic, version, kind, metadata_bytes, payload_bytes, digest = author_format3.HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        require(magic == author_format3.MAGIC and version == 3 and kind == 5 and
                len(metadata) == metadata_bytes and hashlib.sha256(metadata).digest() == digest and
                payload_bytes == len(native_data),
                "migrated Effect resource envelope differs: " + relative)
        reader = author_format3._MetadataReader(metadata)
        require((reader.string(), reader.integer()) == ("EFF_AUTHOR_RESOURCE_METADATA/3", 5),
                "migrated Effect resource metadata structure differs: " + relative)
        actual_id, actual_file, native_hash = (reader.string() for _ in range(3))
        actual_bytes = reader.integer()
        reader.end()
        require(actual_id == resource_id and actual_file == relative and
                native_hash == hashlib.sha256(native_data).hexdigest().upper() and
                actual_bytes == len(native_data),
                "migrated Effect resource identity or digest differs: " + relative)
        payload = stream.read(payload_bytes)
    require(payload == native_data,
            "migrated Effect Bundle bytes changed during Format 3 wrapping: " + relative)


class _SourceCollection:
    def __init__(self, sources):
        self.sources = sources

    def resolve(self, kind, path, asset):
        return _unique_source(self.sources, kind, path, asset)[1]


def _prepare_migrated_mesh_fields(tree, fields):
    """Persist expanded native frame data while keeping its UV streams intact."""
    from .mesh import expand_shaped_frames
    if expand_shaped_frames(tree):
        fields["m_VertexData"] = encode(tree["m_VertexData"])


def _unique_source(sources, kind, path, asset):
    matches = [(source, source.find_all(kind, path, asset)) for source in sources]
    rows = [(source, row) for source, values in matches for row in values]
    require(rows, f"AnimeStudio source has 0 exact {kind} matches: {path} / {asset}")
    priority = min(getattr(source, "migration_priority", 0) for source, _ in rows)
    rows = [(source, row) for source, row in rows
            if getattr(source, "migration_priority", 0) == priority]
    if len(rows) > 1:
        first_source, first = rows[0]
        first_hash = first_source.rows[first["native"]["cab"]]["snapshot"]["sha256"]
        require(all(row["identity"] == first["identity"] and
                    row["native"] == first["native"] and
                    row.get("details") == first.get("details") and
                    source.rows[row["native"]["cab"]]["snapshot"]["sha256"] == first_hash
                    for source, row in rows[1:]),
                f"AnimeStudio source has conflicting exact {kind} matches: {path} / {asset}")
    return rows[0]


def _source_texture(sources, source_material, property_name):
    textures = [row for row in source_material.get("details", {}).get("textures", ())
                if row.get("property") == property_name]
    require(len(textures) == 1,
            "AnimeStudio Material does not have one exact texture slot: " + property_name)
    pointer = textures[0].get("pointer", {})
    identity = pointer.get("identity")
    require(identity and pointer.get("type") == "Texture2D" and pointer.get("resolved"),
            "AnimeStudio texture slot does not resolve to Texture2D: " + property_name)
    rows = [(source, source._resources_by_identity.get(identity)) for source in sources]
    rows = [(source, row) for source, row in rows if row is not None]
    require(rows, "AnimeStudio Texture2D identity is absent from the full source set: " + identity)
    priority = min(getattr(source, "migration_priority", 0) for source, _ in rows)
    rows = [(source, row) for source, row in rows
            if getattr(source, "migration_priority", 0) == priority]
    texture = rows[0][1]
    require(all(row.get("details") == texture.get("details") for _, row in rows),
            "AnimeStudio Texture2D identity has conflicting full-index metadata: " + identity)
    return rows[0][0], texture


def _srgb_property(property_name):
    name = property_name.casefold()
    return ("diffuse" in name or "emission" in name or
            name in {"_basemap", "_maintex", "_albedomap", "_basecolormap"})


def _array_map(value):
    result = {}
    for row in value.items():
        key = row["first"].data
        require(key not in result, "duplicate native Material property: " + key)
        result[key] = row["second"]
    return result


def _vector(value, fields):
    return [float(value[field].data) for field in fields]


def _number(value):
    require(isinstance(value, (int, float)), "native Material property is not numeric")
    return str(value)


def _material_operations(source, source_material, package, material):
    native = source.resolve("Material", material["sourcePath"], material["sourceAsset"])
    before = source.tree(native)
    after = copy.deepcopy(before)
    resources = package["_resources"]
    supported = {"m_SavedProperties", "m_CustomRenderQueue", "m_ValidKeywords",
                 "m_InvalidKeywords", "m_ShaderKeywords", "stringTagMap"}
    for field in material["nativeFields"]:
        name = field["name"]
        require(name in supported,
                "legacy Material field has no Format 3 operation mapping: " + name)
        encoded = resources[field["file"]]["blocks"][name]
        after.data[name] = decode(after[name].schema, encoded)
        require(encode(after[name]) == encoded,
                "legacy Material field failed its native schema roundtrip: " + name)

    edits = []
    if "m_SavedProperties" in after.data:
        previous = before["m_SavedProperties"]
        current = after["m_SavedProperties"]
        for field, kind in (("m_Floats", "float"), ("m_Ints", "int"),
                            ("m_Colors", "color"), ("m_TexEnvs", "texture")):
            if field not in previous.data or field not in current.data:
                continue
            old_values, new_values = _array_map(previous[field]), _array_map(current[field])
            removed = old_values.keys() - new_values.keys()
            edits.extend(("remove." + kind + "." + prop, "true")
                         for prop in sorted(removed))
            for prop, value in new_values.items():
                old = old_values.get(prop)
                if kind == "float":
                    if old is None or encode(old) != encode(value):
                        edits.append(("float." + prop, _number(value.data)))
                elif kind == "int":
                    if old is None or encode(old) != encode(value):
                        edits.append(("int." + prop, str(int(value.data))))
                elif kind == "color":
                    if old is None or encode(old) != encode(value):
                        edits.append(("color." + prop, ",".join(
                            _number(component) for component in _vector(value, ("r", "g", "b", "a")))))
                else:
                    require(old is not None,
                            "Format 3 cannot create a native texture slot: " + material["id"] + ":" + prop)
                    for member, operation in (("m_Scale", "texture_scale."),
                                              ("m_Offset", "texture_offset.")):
                        if encode(value[member]) != encode(old[member]):
                            edits.append((operation + prop, ",".join(
                                _number(component) for component in _vector(value[member], ("x", "y")))))

    def keyword_set(tree):
        if "m_ValidKeywords" in tree.data:
            return {value.data for value in tree["m_ValidKeywords"].items()}
        if "m_ShaderKeywords" in tree.data:
            return set(tree["m_ShaderKeywords"].data.split())
        return set()

    old_keywords, new_keywords = keyword_set(before), keyword_set(after)
    edits.extend(("keyword." + value, "true") for value in sorted(new_keywords - old_keywords))
    edits.extend(("keyword." + value, "false") for value in sorted(old_keywords - new_keywords))

    if "stringTagMap" in before.data and "stringTagMap" in after.data:
        old_tags = _array_map(before["stringTagMap"])
        new_tags = _array_map(after["stringTagMap"])
        removed = old_tags.keys() - new_tags.keys()
        edits.extend(("remove.tag." + key, "true") for key in sorted(removed))
        edits.extend(("tag." + key, str(value.data)) for key, value in new_tags.items()
                     if key not in old_tags or value.data != old_tags[key].data)

    if "m_CustomRenderQueue" in after.data:
        queue = after["m_CustomRenderQueue"].data
        require(queue >= -1, "legacy Material render queue is invalid")
        if queue != before["m_CustomRenderQueue"].data:
            edits.append(("render_queue", str(queue)))

    values = ["format=EFFMAT", "version=1", "overrides=true",
              "source=" + material["sourcePath"],
              "name=" + str(source_material.get("name") or material["sourceAsset"])]
    values.extend(key + "=" + value for key, value in edits)
    return values


def _visibility_expression(tree):
    if tree[0] == "literal":
        value = tree[1]
        if isinstance(value, bool):
            value = 1.0 if value else 0.0
        return ("number", float(value), "", None, None)
    if tree[0] == "var":
        return ("variable", 0.0, tree[1], None, None)
    if tree[0] == "!":
        return ("not", 0.0, "", _visibility_expression(tree[1]), None)
    operators = {"==": "equal", "!=": "not_equal", "<": "less",
                 "<=": "less_equal", ">": "greater", ">=": "greater_equal",
                 "&&": "and", "||": "or"}
    require(tree[0] in operators, "legacy visibility operator is unsupported")
    return (operators[tree[0]], 0.0, "", _visibility_expression(tree[1]),
            _visibility_expression(tree[2]))


def _source_texture_for_binding(sources, material, property_name, fallback_format=None):
    source, source_material = _unique_source(
        sources, "Material", material["sourcePath"], material["sourceAsset"])
    slots = [row for row in source_material.get("details", {}).get("textures", ())
             if row.get("property") == property_name]
    require(len(slots) == 1,
            "AnimeStudio Material does not have one exact texture slot: " + property_name)
    pointer = slots[0].get("pointer", {})
    if pointer.get("isNull") and not pointer.get("resolved"):
        require(fallback_format in ("RGBA32", "BC5", "BC7"),
                "null AnimeStudio texture slot has no supported legacy format: " + property_name)
        template = next(((candidate, row) for candidate in sources
                         for row in (candidate.first_of_type("Texture2D"),) if row), None)
        require(template is not None,
                "AnimeStudio source set has no Texture2D scaffold template")
        return template[0], template[1], fallback_format
    texture_source, texture = _source_texture(sources, source_material, property_name)
    format_name = texture.get("details", {}).get("format")
    require(format_name in ("RGBA32", "BC5", "BC7"),
            "AnimeStudio source Texture format is not supported: " + str(format_name))
    return texture_source, texture, format_name


def _prepare_textures(package, sources, texconv):
    textures_by_id = {row["id"]: row for row in package["textures"]}
    uses = {identity: [] for identity in textures_by_id}
    materials_by_id = {row["id"]: row for row in package["materials"]}
    texture_resources = package["_resources"]
    for material in package["materials"]:
        _, source_material = _unique_source(
            sources, "Material", material["sourcePath"], material["sourceAsset"])
        for edit in material["textureEdits"]:
            texture_id = edit["texture"]
            texture_source, source_texture, source_format = _source_texture_for_binding(
                sources, material, edit["property"], textures_by_id[texture_id]["format"])
            color_space = 1 if _srgb_property(edit["property"]) else 0
            old = textures_by_id[texture_id]
            require(old["mipCount"] in (1, max(old["width"], old["height"]).bit_length()),
                    "legacy Texture mip chain cannot be regenerated exactly: " + texture_id)
            uses[texture_id].append((material["id"], edit["property"], color_space,
                                    texture_source, source_texture, source_format))

    output_textures, remap = [], {}
    output_ids = set()
    shared_textures = {}
    for texture_id, rows in textures_by_id.items():
        require(uses[texture_id], "legacy Texture is not bound by a Material: " + texture_id)
        pixels = texture_resources[rows["file"]]["blocks"]["pixels"]
        variants = {}
        for material_id, property_name, color_space, texture_source, source_texture, source_format in uses[texture_id]:
            key = (color_space, source_texture["identity"], source_format)
            variants.setdefault(key, (texture_source, source_texture))
            remap[(material_id, property_name)] = key
        for index, (key, (texture_source, source_texture)) in enumerate(sorted(
                variants.items(), key=lambda row: row[0])):
            color_space, _source_identity, source_format = key
            suffix = "_srgb" if color_space else "_linear"
            output_id = rows["id"] + suffix
            width, height, mip_count = rows["width"], rows["height"], rows["mipCount"]
            if source_format == "RGBA32":
                target_format, format_id, encoded = "RGBA32", 4, pixels
            else:
                full_mips = max(width, height).bit_length()
                require(mip_count in (1, full_mips),
                        "legacy Texture mip chain cannot be regenerated exactly: " + rows["id"])
                from eiem_texture_compression import compress_rgba
                encoded_result = compress_rgba(
                    pixels[:width * height * 4], width, height,
                    mip_count > 1, source_format,
                    color_space, texconv, flip_y=False)
                require(encoded_result["mipCount"] == mip_count,
                        "compressed Texture mip count differs from AnimeStudio source")
                target_format = encoded_result["format"]
                format_id = encoded_result["formatId"]
                encoded = encoded_result["pixels"]
            signature = (
                width, height, mip_count, target_format, format_id, color_space,
                rows["filter"], rows["wrap"], rows["aniso"], rows["mipBias"], encoded,
            )
            shared_id = shared_textures.get(signature)
            if shared_id is None:
                require(output_id not in output_ids,
                        "migrated Texture identifier collision: " + output_id)
                output_ids.add(output_id)
                shared_textures[signature] = output_id
                output_textures.append(dict(
                    id=output_id, sourcePath=source_texture["logicalPath"],
                    sourceAsset=source_texture["name"], width=width, height=height,
                    mipCount=mip_count, format=target_format, formatId=format_id,
                    colorSpace=color_space, filter=rows["filter"], wrap=rows["wrap"],
                    aniso=rows["aniso"], mipBias=rows["mipBias"], pixels=encoded))
            else:
                output_id = shared_id
            for material_id, property_name, used_color_space, used_source, used_texture, used_format in uses[texture_id]:
                if (used_color_space, used_texture["identity"], used_format) == key:
                    remap[(material_id, property_name)] = output_id
    return output_textures, remap


def _prepare_controls(package, parts):
    controls = {row["variable"]: row for row in package["shapeControls"]}
    clips = package["shapeAnimations"]
    claimed_tracks = set()
    for part in parts:
        part["visibilityExpression"] = _visibility_expression(part["visible"].tree)
        bindings = part["shapeBindings"]
        part_controls = []
        for shape, variable in bindings.items():
            control = controls[variable]
            part_controls.append(dict(
                enabled=True, shape=shape, identity=variable, variable=variable,
                label=control["label"], minimum=control["minimum"],
                maximum=control["maximum"], default=package["states"][variable],
                hotkey_increase="", hotkey_decrease="", hotkey_speed=1.0))
        part["shapeControls"] = part_controls
        part["shapeAnimations"] = []
        bound_shapes = {variable: shape for shape, variable in bindings.items()}
        for clip in clips:
            channels = {bound_shapes[variable]: samples
                        for variable, samples in clip["tracks"].items()
                        if variable in bound_shapes}
            if channels:
                claimed_tracks.update((clip["id"], variable) for variable in clip["tracks"]
                                      if variable in bound_shapes)
                part["shapeAnimations"].append(dict(
                    id=clip["id"], label=clip["label"],
                    speedVariable=clip["speedVariable"], key=clip["key"],
                    duration=clip["duration"], speed=clip["speed"],
                    loop=clip["loop"], autoplay=clip["autoplay"], channels=channels))
    expected_tracks = {(clip["id"], variable) for clip in clips for variable in clip["tracks"]}
    require(claimed_tracks == expected_tracks,
            "legacy shape animation references an unbound or multiply ambiguous shape state")


def migrate_package(source_dir, destination, *, workspace, source_root=None,
                    author_core, author_assets, texconv, vendor=None):
    source_dir, destination = Path(source_dir).resolve(), Path(destination).resolve()
    require(source_dir != destination and source_dir not in destination.parents and
            destination not in source_dir.parents,
            "migration source and destination may not contain one another")
    require(not destination.exists(), "migration destination already exists: " + str(destination))
    if vendor:
        sys.path.insert(0, str(Path(vendor).resolve()))
    blender_tools = Path(__file__).resolve().parents[1] / "Blender"
    sys.path.insert(0, str(blender_tools))
    import eiem_format
    import eiem_native_export

    package = read_package(source_dir)
    require(package["format"] == "EFF_NATIVE_MOD_DIRECTORY" and package["version"] == 2,
            "migration input is not a validated Format 2 directory")
    require(not package["materialAnimations"],
            "legacy Material animations are outside the approved migration scope")
    core = AuthorCore(author_core)
    sources = _source_packages(source_root or workspace, core,
                               fallback_workspace=workspace if source_root else None)
    for source in sources:
        source.assert_current()

    textures, texture_remap = _prepare_textures(package, sources, texconv)
    materials = {}
    source_material_rows = {}
    for material in package["materials"]:
        source, source_material = _unique_source(
            sources, "Material", material["sourcePath"], material["sourceAsset"])
        source_material_rows[material["id"]] = source_material
        texture_edits = []
        for edit in material["textureEdits"]:
            texture_edits.append(dict(
                property=edit["property"],
                texture=texture_remap[(material["id"], edit["property"])]))
        author_values = _material_operations(source, source_material, package, material)
        author_values.extend("texture." + edit["property"] + "=" + edit["texture"]
                             for edit in texture_edits)
        materials[material["id"]] = dict(
            id=material["id"], sourcePath=material["sourcePath"],
            sourceAsset=material["sourceAsset"], textureEdits=texture_edits,
            authorData=("\n".join(author_values) + "\n").encode("utf-8"))

    parts = copy.deepcopy(package["parts"])
    for part in parts:
        fields = {}
        for field in part["nativeFields"]:
            data = package["_resources"][field["file"]]["blocks"][field["name"]]
            require(field["name"] not in fields, "duplicate legacy Mesh field")
            fields[field["name"]] = data
        require(set(fields) == {
            "m_SubMeshes", "m_Shapes", "m_BindPose", "m_BoneNameHashes",
            "m_BonesAABB", "m_VariableBoneCountWeights", "m_MeshCompression",
            "m_IndexFormat", "m_IndexBuffer", "m_VertexData", "m_CompressedMesh",
            "m_LocalAABB", "m_StreamData", "m_BonesPerVertex"},
            "legacy Mesh field set is incomplete or unknown")
        part["fieldData"] = fields
        part["targetPath"] = part["targetAsset"] = ""
        mesh_targets = [(rule, target) for rule in package["rules"]
                        if part["partId"] in rule["parts"]
                        for target in rule["targets"]]
        require(mesh_targets, "legacy Mesh Part has no replacement target")
        path, asset = mesh_targets[0][1]["sourcePath"], mesh_targets[0][1]["sourceAsset"]
        source, native = _unique_source(sources, "Mesh", path, asset)
        tree = copy.deepcopy(source.tree(native))
        for name, data in fields.items():
            tree.data[name] = decode(tree[name].schema, data)
            require(encode(tree[name]) == data,
                    "legacy Mesh field failed its native schema roundtrip: " + name)
        _prepare_migrated_mesh_fields(tree, fields)
        author = decode_native_mesh(tree, part["bonePaths"])
        author["source"] = path + "\n" + asset
        author["name"] = part["object"]
        part["authorData"] = eiem_format.encode_mesh(author)

    _prepare_controls(package, parts)
    effects = copy.deepcopy(package["effects"])
    author_files = _effect_author_files(effects, source_root or workspace, eiem_format)
    templates = resolve_templates(sources, package["rules"], include_texture=True)
    native_meshes, native_textures = eiem_native_export.format3_native_records(
        author_format3, _SourceCollection(sources), parts, textures,
        package["rules"])

    destination.parent.mkdir(parents=True, exist_ok=True)
    staging_root = Path(tempfile.mkdtemp(prefix=".eff-format3-migrate-",
                                         dir=destination.parent))
    candidate = staging_root / destination.name
    candidate.mkdir()
    try:
        authored = write_format3_directory(
            candidate, package["modId"], parts, materials, textures,
            package["rules"], [], package["materialAnimations"], effects,
            author_files=author_files,
            state_defaults=package["states"],
            persistent_states=package["persistentStates"],
            key_bindings=package["keyBindings"],
            key_switch_enabled=package["keySwitchEnabled"],
            texture_template=next((t for t in templates if t['kind'] == 'Texture2D'), None))
        eiem_native_export.write_format3_native_assets(
            author_assets, package["modId"], templates, native_meshes,
            native_textures, candidate)
        result = read_format3_directory(candidate)
        for effect in effects:
            for index, bundle in enumerate(effect["native"]["bundles"]):
                attachment = authored['_ini']['EffectAttachment' + author_format3.identifier(effect['id'])]
                bundle_id = attachment['bundles'].split(',')[index]
                relative = authored['_ini'][bundle_id]['path']
                _verify_effect_resource_payload(candidate / relative, relative, bundle["data"])
        expected_sections = {
            *(author_format3.part_resource_id(part["partId"]) for part in parts),
            *(author_format3.identifier(identity) for identity in materials),
            *(author_format3.identifier(texture["id"]) for texture in textures),
            *(author_format3.identifier("Replace" + min(
                rule["targets"], key=lambda target: target["level"])["sourceAsset"])
              for rule in package["rules"]),
            *("Animation" + author_format3.identifier(clip["id"])
              for clip in package["shapeAnimations"]),
            *("EffectAttachment" + author_format3.identifier(effect["id"])
              for effect in effects),
        }
        expected_files = {
            *(row["path"] for row in native_meshes),
            *(row["path"] for row in native_textures),
            *author_format3.named_resource_paths(
                [(material["id"], material.get("label", material.get("sourceAsset", material["id"])))
                 for material in materials.values()], "materials", ".effres").values(),
            *(path[:-len(".effres")] + ".dds" for path in
              author_format3.named_resource_paths(
                  [(texture["id"], texture.get("pixelLabel", texture["id"]))
                   for texture in textures], "textures", ".effres").values()),
            *(row['path'] for row in authored['_ini'].values() if row.get('type') == 'animation-samples'),
            *author_format3.named_resource_paths(
                [(effect["native"]["key"] + "_" + str(index),
                  (effect.get("label") or effect["id"]) + "_" + str(index))
                 for effect in effects for index, _ in enumerate(effect["native"]["bundles"])],
                "effects", ".effres").values(),
        }
        require(result["format"] == 3 and result["modId"] == package["modId"] and
                expected_sections <= set(authored["_ini"].sections()) and
                expected_files <= result["_files"],
                "Format 3 migration verification is missing declarations or resources")
        for source in sources:
            source.assert_current()
        os.replace(candidate, destination)
        return dict(modId=package["modId"], parts=len(parts),
                    materials=len(materials), textures=len(textures),
                    rules=len(package["rules"]), shapeAnimations=len(package["shapeAnimations"]),
                    migratedEffects=len(effects), droppedPfbEffects=0,
                    output=str(destination))
    finally:
        if staging_root.exists():
            shutil.rmtree(staging_root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-effect-prefabs", action="store_true")
    parser.add_argument("--backup-root", type=Path)
    parser.add_argument("--destination-root", type=Path)
    parser.add_argument("--workspace", type=Path)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--author-core", type=Path)
    parser.add_argument("--author-assets", type=Path)
    parser.add_argument("--texconv", type=Path)
    parser.add_argument("--vendor", type=Path)
    parser.add_argument("--mod", action="append", default=[])
    args = parser.parse_args()

    if args.backup_root is None:
        parser.error("--backup-root is required")
    backup_root = args.backup_root.resolve()
    if args.list_effect_prefabs:
        effects = []
        for directory in sorted(backup_root.iterdir(), key=lambda path: path.name.casefold()):
            if not directory.is_dir():
                continue
            package = read_package(directory)
            effects.extend(dict(mod=directory.name, id=effect["id"],
                                prefab=effect["native"]["prefab"])
                           for effect in package["effects"])
        print(json.dumps({"effects": effects}, ensure_ascii=False))
        return 0
    required = ("destination_root", "workspace", "author_core", "author_assets", "texconv")
    missing = ["--" + name.replace("_", "-") for name in required if getattr(args, name) is None]
    if missing:
        parser.error("required for migration: " + ", ".join(missing))
    destination_root = args.destination_root.resolve()
    names = args.mod or [path.name for path in sorted(backup_root.iterdir()) if path.is_dir()]
    results, failures = [], []
    for name in names:
        try:
            result = migrate_package(
                backup_root / name, destination_root / name,
                workspace=args.workspace, source_root=args.source_root,
                author_core=args.author_core,
                author_assets=args.author_assets, texconv=args.texconv,
                vendor=args.vendor)
            results.append(result)
            print(json.dumps({"status": "migrated", **result}, ensure_ascii=False))
        except Exception as error:
            failures.append((name, str(error)))
            print(json.dumps({"status": "failed", "mod": name, "error": str(error)},
                             ensure_ascii=False))
    print(json.dumps({"migrated": len(results), "failed": len(failures)}, ensure_ascii=False))
    return 1 if failures else 0


if __name__ == "__main__":
    main()
