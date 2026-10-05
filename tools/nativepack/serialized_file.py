"""Version-22 SerializedFile object-table rebuild, retaining source metadata.

No type/external table guessing: added objects use existing source types and
references use existing dependencies. Other format versions are rejected.
"""
from __future__ import annotations

import hashlib
import struct
from .type_tree import Schema, decode, encode, require
from .hashing import sha256_bytes


class SerializedFile:
    def __init__(self, data, manifest, schema):
        self.original, self.manifest, self.cab = data, manifest, manifest["cab"]
        require(len(data) == manifest["snapshot"]["size"] and
                sha256_bytes(data).hex().upper() == manifest["snapshot"]["sha256"], "source snapshot hash")
        require(struct.unpack_from(">I", data, 8)[0] == 22 and data[16] == 0, "requires v22 little-endian SerializedFile")
        self.metadata_size, self.file_size, self.data_offset = struct.unpack_from(">Iqq", data, 20)
        require(self.file_size == len(data) and self.data_offset == manifest["header"]["dataOffset"], "source header")
        self.schemas = {t["index"]: Schema.compile(t["nodes"]) for t in schema["types"] if t["nodes"]}
        self.objects = {r["pathId"]: dict(r, payload=data[r["offset"]:r["offset"]+r["size"]]) for r in manifest["objects"]}
        self.order = [r["pathId"] for r in manifest["objects"]]
        self.changed, self.added = set(), set()
        # Skip precisely the current type table; its original bytes stay intact.
        position = data.index(b"\0", 48) + 1 + 4
        require(data[position] == 1, "source TypeTree disabled")
        position += 1
        count = struct.unpack_from("<i", data, position)[0]
        position += 4
        require(count == len(schema["types"]), "source type count")
        for t in schema["types"]:
            position += 4 + 1 + 2
            if t["classId"] == 114:
                position += 16
            position += 16
            nodes, strings = struct.unpack_from("<ii", data, position)
            position += 8
            require(nodes == len(t["nodes"]) and strings >= 0, "source schema count")
            position += nodes * 32 + strings
            dependencies = struct.unpack_from("<i", data, position)[0]
            require(dependencies >= 0, "type dependencies")
            position += 4 + dependencies * 4
        self.count_offset = position
        count = struct.unpack_from("<i", data, position)[0]
        require(count == len(self.order), "source object count")
        position += 4
        self.table_start = (position + 3) & ~3
        position = self.table_start
        for path_id in self.order:
            position = (position + 3) & ~3
            actual_id, offset, size, type_index = struct.unpack_from("<qqIi", data, position)
            row = self.objects[path_id]
            require((actual_id, offset + self.data_offset, size, type_index) ==
                    (path_id, row["offset"], row["size"], row["typeIndex"]), "source object table mismatch")
            position += 24
        self.table_end = position
        require(48 <= self.count_offset < self.table_end <= 48 + self.metadata_size <= self.data_offset, "metadata ranges")

    def tree(self, path_id):
        row = self.objects[path_id]
        value = decode(self.schemas[row["typeIndex"]], row["payload"])
        require(encode(value) == row["payload"], "unchanged TypeTree roundtrip differs: " + self.cab + ":" + str(path_id))
        return value

    def write_tree(self, path_id, tree):
        payload = encode(tree)
        require(encode(decode(tree.schema, payload)) == payload, "changed TypeTree roundtrip differs")
        self.objects[path_id]["payload"] = payload
        self.changed.add(path_id)

    def allocate(self, label):
        value = int.from_bytes(hashlib.sha256((self.cab + ":" + label).encode()).digest()[:8], "little", signed=True)
        require(value != 0 and value not in self.objects, "new PathID collision: " + label)
        return value

    def add_tree(self, path_id, template_id, tree):
        require(path_id not in self.objects, "duplicate new PathID")
        row = self.objects[template_id]
        self.objects[path_id] = dict(pathId=path_id, typeIndex=row["typeIndex"], classId=row["classId"], payload=b"")
        self.order.append(path_id)
        self.added.add(path_id)
        self.write_tree(path_id, tree)

    def build(self):
        if not self.changed:
            return self.original
        metadata = bytearray(self.original[48:self.count_offset])
        metadata.extend(struct.pack("<i", len(self.order)))
        metadata.extend(bytes((-len(metadata)) % 4))
        table_offset = len(metadata)
        metadata.extend(bytes(24 * len(self.order)))
        metadata.extend(self.original[self.table_end:48+self.metadata_size])
        data_offset = (48 + len(metadata) + 15) & ~15
        payloads = bytearray()
        table_positions = {path_id: i for i, path_id in enumerate(sorted(self.order))}
        for path_id in self.order:
            row = self.objects[path_id]
            payloads.extend(bytes((-len(payloads)) % 8))
            offset = len(payloads)
            payloads.extend(row["payload"])
            struct.pack_into("<qqIi", metadata, table_offset+table_positions[path_id]*24,
                             path_id, offset, len(row["payload"]), row["typeIndex"])
        header = bytearray(self.original[:48])
        struct.pack_into(">Iqq", header, 20, len(metadata), data_offset+len(payloads), data_offset)
        # v22 uses the extended size fields; retain the legacy fields exactly.
        return bytes(header + metadata + bytes(data_offset-48-len(metadata)) + payloads)


def unityfs(members, unity_revision):
    """One uncompressed UnityFS v7 block; CAB/resS names remain exact."""
    data = b"".join(payload for _name, payload in members)
    require(len(data) <= 0xffffffff, "prototype bundle block exceeds uint32")
    info = bytearray(hashlib.md5(data).digest())
    info.extend(struct.pack(">IIIH", 1, len(data), len(data), 0))
    info.extend(struct.pack(">I", len(members)))
    offset = 0
    for name, payload in members:
        require("/" not in name and "\\" not in name and "\0" not in name, "bundle member path")
        info.extend(struct.pack(">qqI", offset, len(payload), 4 if name.startswith("CAB-") and not name.endswith(".resS") else 0))
        info.extend(name.encode() + b"\0")
        offset += len(payload)
    header = bytearray(b"UnityFS\0" + struct.pack(">I", 7) + b"2021.x.x\0" + unity_revision.encode() + b"\0")
    size_offset = len(header)
    header.extend(struct.pack(">qIII", 0, len(info), len(info), 0x40))
    header.extend(bytes((-len(header)) % 16))
    struct.pack_into(">q", header, size_offset, len(header)+len(info)+len(data))
    return bytes(header + info + data)
