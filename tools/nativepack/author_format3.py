"""Build exact author template tables and validate format-3 directories."""
from __future__ import annotations

import configparser
import hashlib
import json
from pathlib import Path
import re
import struct

from .type_tree import require

HEADER = struct.Struct("<16sIIQQ32s")
MAGIC = b"EFF_AUTHOR_FILE3"
METADATA_STRUCTURE = "EFF_AUTHOR_RESOURCE_METADATA/3"
def _digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest().upper()


def _manifest(package):
    package = Path(package).resolve()
    raw = (package / "source/manifest.json").read_bytes()
    value = json.loads(raw.decode("utf-8-sig"))
    require(value.get("format") == "EFFSOURCE" and value.get("version") == 1,
            "unsupported EFFSOURCE author baseline")
    rows = {row["cab"]: row for row in value.get("serializedFiles", ())}
    require(len(rows) == len(value.get("serializedFiles", ())), "duplicate CAB in EFFSOURCE")
    return package, value, rows


def _template(package, manifest, files, kind, path, asset, resource=None):
    if resource is None:
        matches = [row for row in manifest["resources"]
                   if row.get("type") == kind and row.get("logicalPath") == path and row.get("name") == asset]
        require(len(matches) == 1, f"{kind} source has {len(matches)} exact EFFSOURCE matches: {path} / {asset}")
        resource = matches[0]
    else:
        require((resource.get("type"), resource.get("logicalPath"), resource.get("name")) ==
                (kind, path, asset), "selected EFFSOURCE template identity differs")
    native = resource["native"]
    file = files.get(native["cab"])
    require(file is not None, "EFFSOURCE native CAB is absent: " + native["cab"])
    snapshot = (package / file["snapshot"]["file"]).resolve()
    require(package in snapshot.parents and snapshot.is_file(), "EFFSOURCE snapshot path is invalid")
    require(_digest(snapshot) == file["snapshot"]["sha256"], "EFFSOURCE snapshot checksum differs: " + native["cab"])
    return [kind, path, asset, native["cab"], str(native["pathId"]), str(snapshot), file["snapshot"]["sha256"]], file


def resolve_templates(mod_root, source_packages, ini_bytes=None):
    """Resolve exact native templates from the selected EFFSOURCE snapshots."""
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), strict=True)
    ini.optionxform = str
    if ini_bytes is None:
        mod_root = Path(mod_root).resolve()
        with (mod_root / "mod.ini").open(encoding="utf-8-sig") as stream:
            ini.read_file(stream)
    else:
        ini.read_string(bytes(ini_bytes).decode("utf-8-sig", "strict"))
    require(ini.get("Mod", "format", fallback="") == "2", "format-3 authoring input must be format 2 staging")

    packages = [_manifest(path) for path in sorted({Path(p).resolve() for p in source_packages}, key=lambda p: str(p).casefold())]
    wanted = set()
    for name in ini.sections():
        values = ini[name]
        if name.startswith("Replace") and values.get("handling") != "skip":
            wanted.add(("Mesh", values["target.path"], values["target.asset"]))
            for key in values:
                if key.startswith("lod.") and key.endswith(".path"):
                    level = key.removesuffix(".path")
                    wanted.add(("Mesh", values[key], values["lod." + level.removeprefix("lod.") + ".asset"]))

    texture_template = None
    rows_by_identity = {}
    for package, manifest, files in packages:
        wanted_rows = {}
        for resource in manifest.get("resources", ()):
            identity = (resource.get("type"), resource.get("logicalPath"), resource.get("name"))
            if identity in wanted:
                require(identity not in wanted_rows,
                        "duplicate exact EFFSOURCE template: " + repr(identity))
                wanted_rows[identity] = resource
            if resource.get("type") == "Texture2D" and texture_template is None:
                texture_template, _ = _template(package, manifest, files, "Texture2D",
                                                resource["logicalPath"], resource["name"], resource)
        for identity, resource in wanted_rows.items():
            kind, path, asset = identity
            if resource is None:
                continue
            row, file = _template(package, manifest, files, kind, path, asset, resource)
            identity = kind, path, asset
            prior = rows_by_identity.setdefault(identity, row)
            require(prior == row, f"ambiguous exact {kind} template across selected source packages: {path} / {asset}")

    missing = wanted - rows_by_identity.keys()
    require(not missing, "selected EFFSOURCE packages lack exact author templates: " + repr(sorted(missing)))
    rows = [rows_by_identity[key] for key in sorted(rows_by_identity)]
    if any(section.startswith("Texture") for section in ini.sections()):
        require(texture_template is not None, "Texture2D author template is absent from EFFSOURCE")
        rows.append(texture_template)
    return [dict(kind=row[0], path=row[1], asset=row[2], cab=row[3],
                 pathId=int(row[4]), file=row[5], sha256=row[6]) for row in rows]


class _MetadataReader:
    def __init__(self, data):
        self.data = data
        self.offset = 0

    def integer(self):
        require(self.offset + 8 <= len(self.data), "format-3 metadata is truncated")
        value = int.from_bytes(self.data[self.offset:self.offset + 8], "little")
        self.offset += 8
        return value

    def string(self):
        size = self.integer()
        require(size <= len(self.data) - self.offset, "format-3 metadata string extent differs")
        value = self.data[self.offset:self.offset + size].decode("utf-8", "strict")
        self.offset += size
        return value

    def end(self):
        require(self.offset == len(self.data), "format-3 metadata has trailing bytes")


def _effmat_values(data, label):
    try:
        text = data.decode("utf-8-sig", "strict")
    except UnicodeDecodeError as error:
        raise ValueError("Material author snapshot is not UTF-8: " + label) from error
    values = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith((";", "#")):
            continue
        require("=" in line, "Material author snapshot line is invalid: " + label)
        key, value = line.split("=", 1)
        key, value = key.strip(), value.strip()
        require(key and key not in values, "Material author snapshot key is empty or duplicated: " + label)
        values[key] = value
    require(values.get("format") == "EFFMAT" and values.get("version") == "1" and
            values.get("overrides", "").lower() == "true" and values.get("source", "").strip(),
            "Material author snapshot header differs: " + label)
    return values


def _validate_material_resource(root, relative, section, ini, payload_bytes):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        _, _, _, metadata_bytes, _, _ = HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        reader = _MetadataReader(metadata)
        require(reader.string() == "EFF_AUTHOR_MATERIAL_EDITS/1",
                "format-3 Material metadata structure differs: " + relative)
        material_id, resource_file, source_path, source_asset = (
            reader.string() for _ in range(4))
        author_bytes, author_hash = reader.integer(), reader.string()
        reader.end()
        require(material_id == section and resource_file == relative and source_path.startswith("assets/") and
                source_path == ini[section].get("source.path") and
                source_asset == ini[section].get("source.asset") and
                len(author_hash) == 64 and author_hash.upper() == author_hash and
                author_bytes > 0 and author_bytes == payload_bytes,
                "format-3 Material identity/extents differ from mod.ini: " + section)
        author_data = stream.read(author_bytes)
    require(len(author_data) == author_bytes and hashlib.sha256(author_data).hexdigest().upper() == author_hash,
            "format-3 Material edit checksum differs: " + section)
    values = _effmat_values(author_data, section)
    require(values["source"] == source_path, "format-3 Material author source differs: " + section)
    snapshot_textures = {key: value for key, value in values.items() if key.startswith("texture.")}
    manifest_textures = {key: value for key, value in ini[section].items() if key.startswith("texture.")}
    require(snapshot_textures == manifest_textures,
            "format-3 Material author texture bindings differ from mod.ini: " + section)
    declared_textures = {name for name in ini.sections() if name.startswith("Texture")}
    require(all(value in declared_textures for value in snapshot_textures.values()),
            "format-3 Material author references an undeclared Texture: " + section)


def _validate_physics_resource(root, relative, section, ini, payload_bytes):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        _, _, _, metadata_bytes, _, _ = HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        reader = _MetadataReader(metadata)
        require((reader.string(), reader.integer()) == (METADATA_STRUCTURE, 4),
                "format-3 Physics metadata structure differs: " + relative)
        resource_id, resource_file, native_hash = (reader.string() for _ in range(3))
        native_kind, native_bytes = reader.integer(), reader.integer()
        target_count = reader.integer()
        require(target_count <= (len(metadata) - reader.offset) // 16,
                "format-3 Physics target metadata is truncated: " + relative)
        targets = {(reader.string(), reader.string()) for _ in range(target_count)}
        author_bytes, author_hash = reader.integer(), reader.string()
        reader.end()
        require(resource_id == section and resource_file == relative and native_kind == 5 and
                len(native_hash) == len(author_hash) == 64 and
                native_hash.upper() == native_hash and author_hash.upper() == author_hash and
                native_bytes > 0 and author_bytes > 0 and native_bytes + author_bytes == payload_bytes and
                targets,
                "format-3 Physics identity/extents differ from mod.ini: " + section)
        native_digest = hashlib.sha256()
        remaining = native_bytes
        while remaining:
            block = stream.read(min(1024 * 1024, remaining))
            require(block, "format-3 Physics native payload is truncated: " + section)
            native_digest.update(block)
            remaining -= len(block)
        author = stream.read(author_bytes)
    require(native_digest.hexdigest().upper() == native_hash and len(author) == author_bytes and
            hashlib.sha256(author).hexdigest().upper() == author_hash,
            "format-3 Physics native/author checksum differs: " + section)
    require(len(author) >= 28 and author[:8] == b"EFFPHS3\0",
            "format-3 Physics author snapshot header differs: " + section)
    version, document_bytes, skeleton_bytes = struct.unpack_from("<IQQ", author, 8)
    require(version == 1 and document_bytes > 0 and skeleton_bytes > 0 and
            28 + document_bytes + skeleton_bytes == len(author) and
            author[28:36] == b"EFFPHYS\0" and
            author[28 + document_bytes:36 + document_bytes] == b"EFFSKEL\0",
            "format-3 Physics author snapshot dependencies/extents differ: " + section)
    rule_ids = {item.strip() for item in ini[section].get("targets", "").split(",") if item.strip()}
    declared_targets = set()
    for rule in rule_ids:
        if rule not in ini:
            continue
        values = ini[rule]
        declared_targets.add((values.get("target.path", ""), values.get("target.asset", "")))
        lod_levels = set()
        for key in values:
            if not key.startswith("lod."):
                continue
            match = re.fullmatch(r"lod\.([1-9][0-9]*)\.(path|asset)", key)
            require(match, "format-3 Physics rule has an invalid LOD target: " + rule)
            lod_levels.add(match.group(1))
        for level in lod_levels:
            path_key, asset_key = "lod." + level + ".path", "lod." + level + ".asset"
            require(path_key in values and asset_key in values,
                    "format-3 Physics rule has an incomplete LOD target: " + rule)
            declared_targets.add((values[path_key], values[asset_key]))
    require(targets == declared_targets,
            "format-3 Physics Mesh targets differ from mod.ini: " + section)


def _parse_dds(path):
    size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(148)
    require(len(header) >= 128 and size >= 128 and header[:4] == b"DDS " and
            struct.unpack_from("<I", header, 4)[0] == 124 and
            struct.unpack_from("<I", header, 76)[0] == 32,
            "DDS header is invalid: " + str(path))
    flags = struct.unpack_from("<I", header, 8)[0]
    require(flags & 0x1007 == 0x1007 and
            struct.unpack_from("<I", header, 112)[0] == 0 and
            struct.unpack_from("<I", header, 24)[0] == 0,
            "DDS must be one ordinary 2D texture: " + str(path))
    height, width = struct.unpack_from("<II", header, 12)
    mips = struct.unpack_from("<I", header, 28)[0] or 1
    pixel_flags, fourcc = struct.unpack_from("<II", header, 80)
    offset = 128
    fmt = 0
    if pixel_flags & 4 and fourcc == int.from_bytes(b"DX10", "little"):
        require(len(header) >= 148 and size >= 148,
                "DDS DX10 array/cube/dimension/alpha mode is invalid: " + str(path))
        dimension, misc_flag, array_size, alpha_mode = struct.unpack_from("<4I", header, 132)
        require((dimension, misc_flag, array_size) == (3, 0, 1) and alpha_mode <= 4,
                "DDS DX10 array/cube/dimension/alpha mode is invalid: " + str(path))
        dxgi = struct.unpack_from("<I", header, 128)[0]
        fmt = 4 if dxgi in (28, 29) else 27 if dxgi == 83 else 25 if dxgi in (98, 99) else 0
        offset = 148
    elif pixel_flags & 4 and fourcc in (int.from_bytes(b"ATI2", "little"), int.from_bytes(b"BC5U", "little")):
        fmt = 27
    elif pixel_flags & 0x41 == 0x41 and struct.unpack_from("<5I", header, 88) == (
            32, 0xff, 0xff00, 0xff0000, 0xff000000):
        fmt = 4
    require(fmt in (4, 25, 27), "DDS format is unsupported: " + str(path))
    require(0 < width <= 16384 and 0 < height <= 16384,
            "DDS dimensions are invalid: " + str(path))
    max_mips = max(width, height).bit_length()
    require(0 < mips <= max_mips, "DDS mip count is invalid: " + str(path))
    total = 0
    mip_width, mip_height = width, height
    for _ in range(mips):
        total += (mip_width * mip_height * 4 if fmt == 4 else
                  max(1, (mip_width + 3) // 4) * max(1, (mip_height + 3) // 4) * 16)
        mip_width, mip_height = max(1, mip_width // 2), max(1, mip_height // 2)
    require(offset + total == size, "DDS mip data extent differs: " + str(path))
    return width, height, mips, fmt, offset, total


def _validate_effres(path, expected_kind):
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        require(len(header) == HEADER.size, "format-3 resource header is truncated: " + str(path))
        magic, version, kind, metadata_bytes, payload_bytes, digest = HEADER.unpack(header)
        require(magic == MAGIC and version == 3 and kind == expected_kind and metadata_bytes > 0,
                "format-3 resource envelope differs: " + str(path))
        require(HEADER.size + metadata_bytes + payload_bytes == path.stat().st_size,
                "format-3 resource extent differs: " + str(path))
        metadata = stream.read(metadata_bytes)
    require(len(metadata) == metadata_bytes and hashlib.sha256(metadata).digest() == digest,
            "format-3 resource metadata checksum differs: " + str(path))
    return metadata, payload_bytes


def read_format3_directory(root):
    """Return publication metadata after checking relationships, DDS and envelopes."""
    root = Path(root).resolve()
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), strict=True)
    ini.optionxform = str
    with (root / "mod.ini").open(encoding="utf-8-sig") as stream:
        ini.read_file(stream)
    require(ini.get("Mod", "format", fallback="") == "3", "format-3 manifest is absent")
    require(ini.has_option("Mod", "id") and ini.get("Mod", "id").strip(), "format-3 Mod id is absent")
    referenced = {"mod.ini"}
    resources = {}
    dds_rows = {}
    for section in ini.sections():
        if section in ("Mod", "Constants") or section.startswith(("Part", "Replace", "Key", "ShapeControl")):
            continue
        expected_kind = None
        if section.startswith("Mesh"):
            expected_kind = 1
        elif section.startswith(("ShapeAnimation", "MaterialAnimation", "EffectAttachment")):
            expected_kind = 6
        elif section.startswith("Material"):
            expected_kind = 2
        elif section.startswith("Texture"):
            expected_kind = 3
        elif section.startswith("Resource"):
            resource_type = ini[section].get("type", "")
            expected_kind = {"physics": 4, "effect": 5, "controls": 6,
                             "effect-author": 7}.get(resource_type)
            require(expected_kind is not None, "format-3 Resource type is invalid: " + section)
        require(expected_kind is not None, "unknown format-3 resource section: " + section)
        path = ini[section].get("path")
        require(path, "format-3 resource path is absent: " + section)
        candidate = (root / path).resolve()
        require(root in candidate.parents and candidate.is_file(), "format-3 resource is missing or escapes Mod: " + path)
        relative = candidate.relative_to(root).as_posix()
        prior = resources.setdefault(relative, expected_kind)
        require(prior == expected_kind, "format-3 resource path has conflicting kinds: " + relative)
        referenced.add(relative)
        dds = ini[section].get("dds")
        if dds:
            candidate = (root / dds).resolve()
            require(root in candidate.parents and candidate.is_file(), "format-3 DDS is missing or escapes Mod: " + dds)
            dds_relative = candidate.relative_to(root).as_posix()
            referenced.add(dds_relative)
            dds_rows[relative] = (dds_relative, section)
    resource_metadata = {}
    for relative, kind in resources.items():
        metadata, payload_bytes = _validate_effres(root / relative, kind)
        resource_metadata[relative] = (metadata, payload_bytes)
    for relative, (dds_relative, section) in dds_rows.items():
        metadata, _ = resource_metadata[relative]
        reader = _MetadataReader(metadata)
        require(reader.string() == METADATA_STRUCTURE and reader.integer() == 3,
                "format-3 Texture metadata structure differs: " + relative)
        texture_id, texture_file, texture_dds = reader.string(), reader.string(), reader.string()
        reader.string()
        reader.string()
        reader.string()
        reader.integer()
        pixel_offset, pixel_bytes = reader.integer(), reader.integer()
        width, height, mips, fmt = (reader.integer() for _ in range(4))
        reader.end()
        require(texture_id == section and texture_file == relative and texture_dds == dds_relative,
                "format-3 Texture metadata paths/identity differ from mod.ini: " + section)
        metadata_payload_bytes = resource_metadata[relative][1]
        require(pixel_offset > 0 and pixel_offset <= metadata_payload_bytes,
                "format-3 Texture split range differs: " + section)
        actual = _parse_dds(root / dds_relative)
        require(actual[:4] == (width, height, mips, fmt) and actual[5] == pixel_bytes,
                "format-3 DDS dimensions/format/mips differ from native Texture metadata: " + section)
    for section in ini.sections():
        if section.startswith("Material"):
            _validate_material_resource(root, ini[section]["path"], section, ini,
                                        resource_metadata[ini[section]["path"]][1])
        elif section.startswith("Resource") and ini[section].get("type") == "physics":
            _validate_physics_resource(root, ini[section]["path"], section, ini,
                                       resource_metadata[ini[section]["path"]][1])
        elif section.startswith("Resource") and ini[section].get("type") == "effect-author":
            _validate_effect_author_resource(root, ini[section]["path"], section,
                                             resource_metadata[ini[section]["path"]][1])
        elif section.startswith("EffectAttachment"):
            author_id = ini[section].get("author", "")
            require(author_id in ini and ini[author_id].get("type") == "effect-author",
                    "format-3 EffectAttachment author resource is unresolved: " + section)
    files = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}
    return dict(format=3, modId=ini["Mod"]["id"], _files=files)


def _validate_effect_author_resource(root, relative, section, payload_bytes):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        _, _, _, metadata_bytes, _, _ = HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        reader = _MetadataReader(metadata)
        require((reader.string(), reader.integer()) == (METADATA_STRUCTURE, 7),
                "format-3 Effect author metadata structure differs: " + relative)
        resource_id, resource_file = reader.string(), reader.string()
        author_bytes, author_hash = reader.integer(), reader.string()
        reader.end()
        require(resource_id == section and resource_file == relative and
                author_bytes == payload_bytes and len(author_hash) == 64 and
                author_hash.upper() == author_hash,
                "format-3 Effect author identity/extents differ: " + section)
        author = stream.read(author_bytes)
    require(len(author) == author_bytes and hashlib.sha256(author).hexdigest().upper() == author_hash,
            "format-3 Effect author checksum differs: " + section)
    require(len(author) >= 92 and author[:8] == b"EFFFXA3\0",
            "format-3 Effect author snapshot header differs: " + section)
    version, archive_bytes, state_bytes = struct.unpack_from("<IQQ", author, 8)
    archive_hash, state_hash = author[28:60], author[60:92]
    require(version == 1 and archive_bytes > 0 and state_bytes > 0 and
            archive_bytes + state_bytes == len(author) - 92 and
            hashlib.sha256(author[92:92 + archive_bytes]).digest() == archive_hash and
            hashlib.sha256(author[92 + archive_bytes:]).digest() == state_hash,
            "format-3 Effect author snapshot extents/checksum differ: " + section)
    try:
        value = json.loads(author[92 + archive_bytes:].decode("utf-8", "strict"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("format-3 Effect author document is invalid: " + section) from error
    require(value.get("format") == "EFF_EFFECT_AUTHOR3" and value.get("version") == 1 and
            isinstance(value.get("workspace"), dict) and isinstance(value.get("attachment"), dict),
            "format-3 Effect author document structure differs: " + section)
