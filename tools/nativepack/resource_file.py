"""Versioned author-resource containers for the INI directory format.

Blocks contain native field encodings or compact binding data. They are not
standalone Unity assets: the loader applies them to current serialized input.
"""
from __future__ import annotations

import re
from pathlib import Path, PurePosixPath
import struct

from .type_tree import require
from .hashing import sha256_bytes

MAGIC = b"EFFRES2\0"
VERSION = 2
MESH, MATERIAL, PIXELS = 1, 2, 4  # kind 3 is reserved; texture declarations live in INI
HEADER = struct.Struct("<8sIIII16sQ32s")
ENTRY = struct.Struct("<QQ32s")


def identifier(text):
    require(re.fullmatch(r"[A-Za-z0-9_.-]+", text) is not None, "invalid resource identifier: " + text)
    return text


def digest(data):
    return sha256_bytes(data).hex().upper()


def resource_path(root, relative):
    require(isinstance(relative, str) and relative and "\\" not in relative and ":" not in relative,
            "resource path must be a relative POSIX path")
    parts = PurePosixPath(relative)
    require(not parts.is_absolute() and all(p not in ("", ".", "..") for p in relative.split("/")),
            "resource path escapes package")
    base = Path(root).resolve()
    path = (base / relative).resolve()
    require(path.is_relative_to(base), "resource path escapes package")
    return path


def encode_strings(strings):
    output = bytearray(struct.pack("<I", len(strings)))
    for text in strings:
        data = text.encode("utf-8")
        require(len(data) <= 65535 and "\0" not in text, "invalid binding string")
        output.extend(struct.pack("<H", len(data)))
        output.extend(data)
    return bytes(output)


def decode_strings(data):
    require(len(data) >= 4, "binding string table truncated")
    count, = struct.unpack_from("<I", data)
    require(count <= (len(data) - 4) // 2, "invalid binding string count")
    offset, output = 4, []
    for _ in range(count):
        require(offset + 2 <= len(data), "binding string length truncated")
        size, = struct.unpack_from("<H", data, offset)
        offset += 2
        require(offset + size <= len(data), "binding string truncated")
        text = data[offset:offset + size].decode("utf-8")
        require("\0" not in text, "invalid binding string")
        output.append(text)
        offset += size
    require(offset == len(data), "unreferenced binding string bytes")
    return output


def encode_resource(kind, native_type_hash, blocks):
    require(kind in (MESH, MATERIAL, PIXELS), "unknown resource kind")
    type_hash = bytes.fromhex(native_type_hash)
    require(len(type_hash) == 16, "native type hash must contain 16 bytes")
    entries = list(blocks.items())
    names = [name.encode("utf-8") for name, _ in entries]
    require(all(0 < len(name) <= 65535 for name in names), "invalid resource block name")
    directory_size = sum(2 + len(name) + ENTRY.size for name in names)
    offset = (HEADER.size + directory_size + 15) & ~15
    directory, payload = bytearray(), bytearray(offset - HEADER.size - directory_size)
    for name, (_, data) in zip(names, entries):
        data = bytes(data)
        padding = (-offset) % 16
        payload.extend(bytes(padding))
        offset += padding
        directory.extend(struct.pack("<H", len(name)))
        directory.extend(name)
        directory.extend(ENTRY.pack(offset, len(data), sha256_bytes(data)))
        payload.extend(data)
        offset += len(data)
    if not entries:
        # Empty material edits have a header only, not unused alignment bytes.
        payload.clear()
        offset = HEADER.size
    body = bytes(directory + payload)
    return HEADER.pack(MAGIC, VERSION, kind, len(entries), directory_size, type_hash,
                       offset, sha256_bytes(body)) + body


def decode_resource(data, expected_kind):
    require(len(data) >= HEADER.size, "resource header truncated")
    magic, version, kind, count, directory_size, type_hash, total, checksum = HEADER.unpack_from(data)
    require(magic == MAGIC and version == VERSION, "unsupported resource format/version")
    require(kind in (MESH, MATERIAL, PIXELS), "unsupported resource kind")
    require(kind == expected_kind, "resource kind mismatch")
    require(total == len(data), "resource size mismatch")
    require(sha256_bytes(data[HEADER.size:]) == checksum, "resource checksum mismatch")
    stop = HEADER.size + directory_size
    require(stop <= len(data) and count <= directory_size // (3 + ENTRY.size), "invalid resource directory")
    position, end, blocks, descriptors = HEADER.size, stop, {}, []
    for _ in range(count):
        require(position + 2 <= stop, "resource block directory truncated")
        size, = struct.unpack_from("<H", data, position)
        position += 2
        require(size > 0 and position + size + ENTRY.size <= stop, "resource block name truncated")
        name = data[position:position + size].decode("utf-8")
        position += size
        require("\0" not in name and name not in blocks, "duplicate/invalid resource block")
        offset, length, block_hash = ENTRY.unpack_from(data, position)
        position += ENTRY.size
        require(offset == ((end + 15) & ~15) and offset + length <= len(data),
                "overlapping or unreferenced resource block")
        require(data[end:offset] == bytes(offset - end), "resource alignment contains data")
        block = data[offset:offset + length]
        require(sha256_bytes(block) == block_hash, "resource block checksum mismatch")
        blocks[name] = block
        descriptors.append(dict(name=name, offset=offset, size=length, sha256=block_hash.hex().upper()))
        end = offset + length
    require(position == stop and end == len(data), "unreferenced resource directory/suffix")
    return dict(kind=kind, nativeTypeHash=type_hash.hex().upper(), blocks=blocks, descriptors=descriptors)


def read_resource(path, expected_kind, cache=None):
    path = Path(path)
    if cache is not None:
        # A package is read several times during one export (destination,
        # staging, publication gate). Keep the verified bytes identity for the
        # path's current size/mtime so those passes do not re-hash gigabytes.
        # The first read always verifies the complete container and block
        # checksums; a changed size or mtime invalidates this shortcut.
        signature = (path.stat().st_size, path.stat().st_mtime_ns)
        path_key = ('path', str(path.resolve()), expected_kind)
        previous = cache.get(path_key)
        trusted_roots = cache.get('__trusted_roots__', ())
        trusted = any(path == root or root in path.parents for root in
                      (Path(value) for value in trusted_roots))
        if trusted and previous is not None and previous[0] == signature:
            result = cache[previous[1]]
            return dict(kind=result['kind'], nativeTypeHash=result['nativeTypeHash'],
                        blocks=dict(result['blocks']), descriptors=[dict(row) for row in result['descriptors']])
    data = path.read_bytes()
    if cache is None:
        return decode_resource(data, expected_kind)
    # Scope this cache to one export. Always read/hash current bytes, including
    # replacements preserving size and timestamps; never trust a file stamp.
    key = (expected_kind, sha256_bytes(data))
    if key not in cache:
        cache[key] = decode_resource(data, expected_kind)
    cache[path_key] = (signature, key)
    result = cache[key]
    return dict(kind=result['kind'], nativeTypeHash=result['nativeTypeHash'],
                blocks=dict(result['blocks']), descriptors=[dict(row) for row in result['descriptors']])


def write_resource(root, relative, kind, native_type_hash, blocks):
    path = resource_path(root, relative)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as stream:
        stream.write(encode_resource(kind, native_type_hash, blocks))
    return relative
