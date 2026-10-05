"""Strict, byte-preserving TypeTree codec for offline native-write experiments.

Schema comes from the current source SerializedFile, not a guessed Unity class.
All bytes must be consumed; unchanged scalars and alignment bytes retain their
original representation. Unsupported schema is a hard error, not a fallback.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import struct


PRIMITIVES = {
    "SInt8": "b", "UInt8": "B", "char": "B", "bool": "B",
    "short": "h", "SInt16": "h", "UInt16": "H", "unsigned short": "H",
    "int": "i", "SInt32": "i", "UInt32": "I", "unsigned int": "I", "Type*": "I",
    "long long": "q", "SInt64": "q", "UInt64": "Q", "unsigned long long": "Q", "FileSize": "Q",
    "float": "f", "double": "d",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


@dataclass
class Schema:
    type: str
    name: str
    align: bool
    children: list["Schema"] = field(default_factory=list)

    @classmethod
    def compile(cls, nodes):
        require(nodes and nodes[0]["level"] == 0, "TypeTree missing root")
        stack = []
        root = None
        for row in nodes:
            node = cls(row["type"], row["name"], bool(row["metaFlag"] & 0x4000))
            level = row["level"]
            require(level <= len(stack), "TypeTree skips a level")
            stack = stack[:level]
            if level:
                stack[-1].children.append(node)
            else:
                require(root is None, "multiple TypeTree roots")
                root = node
            stack.append(node)
        return root


@dataclass
class Value:
    schema: Schema
    data: object
    atom_bytes: bytes | None = None
    padding: bytes = b""

    def __getitem__(self, key):
        require(isinstance(self.data, dict) and key in self.data, f"absent schema field: {self.schema.name}/{key}")
        return self.data[key]

    def set(self, key, data):
        child = self[key]
        self.data[key] = from_data(child.schema, data)

    def array_node(self):
        if self.schema.type == "Array":
            return self
        require(isinstance(self.data, dict) and list(self.data) == ["Array"], "not a schema array: " + self.schema.name)
        return self.data["Array"]

    def items(self):
        return self.array_node().data

    def set_items(self, items):
        array = self.array_node()
        element = array.schema.children[1]
        if isinstance(items, bytes):
            require(element.type in ("UInt8", "char") and not element.align, "not a raw byte array")
            array.data = items
        else:
            array.data = [item if isinstance(item, Value) else from_data(element, item) for item in items]

    def plain(self):
        if isinstance(self.data, dict):
            return {key: value.plain() for key, value in self.data.items()}
        if isinstance(self.data, list):
            return [v.plain() for v in self.data]
        return self.data


def from_data(schema, data):
    """Construct a complete new value; missing/extra struct fields are errors."""
    if schema.type in PRIMITIVES or schema.type in ("string", "TypelessData"):
        return Value(schema, data)
    if schema.type == "Array":
        require(len(schema.children) == 2 and schema.children[0].name == "size", "array schema")
        if isinstance(data, bytes):
            require(schema.children[1].type in ("UInt8", "char"), "byte array schema")
            return Value(schema, data)
        return Value(schema, [x if isinstance(x, Value) else from_data(schema.children[1], x) for x in data])
    if len(schema.children) == 1 and schema.children[0].type == "Array":
        return Value(schema, {schema.children[0].name: from_data(schema.children[0], data)})
    require(isinstance(data, dict) and set(data) == {c.name for c in schema.children}, "incomplete new struct: " + schema.name)
    return Value(schema, {c.name: from_data(c, data[c.name]) for c in schema.children})


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, size):
        require(0 <= size <= len(self.data)-self.pos, "truncated native object")
        result = self.data[self.pos:self.pos+size]
        self.pos += size
        return result

    def count(self):
        count = struct.unpack("<i", self.take(4))[0]
        require(0 <= count <= 100_000_000, "native array count")
        return count

    def read(self, schema):
        code = PRIMITIVES.get(schema.type)
        atom = None
        if code:
            atom = self.take(struct.calcsize(code))
            data = struct.unpack("<" + code, atom)[0]
        elif schema.type in ("string", "TypelessData"):
            data = self.take(self.count())
            if schema.type == "string":
                data = data.decode("utf-8", errors="strict")
        elif schema.type == "Array":
            require(len(schema.children) == 2 and schema.children[0].name == "size", "array schema")
            count = self.count()
            element = schema.children[1]
            if element.type in ("UInt8", "char") and not element.align:
                data = self.take(count)
            else:
                require(count <= len(self.data)-self.pos or count == 0, "array exceeds object")
                data = [self.read(element) for _ in range(count)]
        else:
            require(schema.children, "unknown native leaf: " + schema.type)
            require(len({c.name for c in schema.children}) == len(schema.children), "duplicate struct fields")
            data = {c.name: self.read(c) for c in schema.children}
        aligned = schema.align or schema.type == "string"
        padding = self.take((-self.pos) % 4) if aligned else b""
        return Value(schema, data, atom, padding)


def decode(schema, data):
    reader = Reader(data)
    result = reader.read(schema)
    require(reader.pos == len(data), f"TypeTree consumed {reader.pos}/{len(data)} bytes")
    return result


def encode(value):
    output = bytearray()

    def write(node):
        schema, data = node.schema, node.data
        code = PRIMITIVES.get(schema.type)
        if code:
            output.extend(node.atom_bytes if node.atom_bytes is not None else struct.pack("<"+code, data))
        elif schema.type in ("string", "TypelessData"):
            raw = data.encode("utf-8") if schema.type == "string" else data
            output.extend(struct.pack("<i", len(raw)))
            output.extend(raw)
        elif schema.type == "Array":
            output.extend(struct.pack("<i", len(data)))
            if isinstance(data, bytes):
                output.extend(data)
            else:
                for child in data:
                    write(child)
        else:
            for child in schema.children:
                write(node[child.name])
        if schema.align or schema.type == "string":
            length = (-len(output)) % 4
            output.extend(node.padding if len(node.padding) == length else bytes(length))

    write(value)
    return bytes(output)
