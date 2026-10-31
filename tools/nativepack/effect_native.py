"""Read the offline-built native effect input used by Format 3 export."""
from pathlib import Path, PurePosixPath

from .hashing import sha256_bytes
from .type_tree import require


def native_archive(data):
    if len(data) < 48:
        return False

    def be(offset, size):
        return int.from_bytes(data[offset:offset + size], 'big')

    def rol(value, amount, bits):
        return ((value << amount) | (value >> (bits - amount))) & ((1 << bits) - 1)

    header = be(0, 4) ^ 0x4A92F0CD
    if be(4, 4) != (((4 * header) & 0xFFFF0000) ^ rol(header, 18, 32) ^ 0xD8B1E637):
        return False
    field = be(10, 4)
    if be(14, 4) ^ field != 8 or be(22, 4) ^ field ^ 0xA7F49310 != 0x240:
        return False
    low = be(18, 4)
    packed = ((be(34, 4) ^ low ^ 0xDAD76848) << 32) | low
    length = rol(packed, 14, 64) ^ 0xA4F1A11747816520

    def block_size(high, low_offset):
        value = be(low_offset, 2)
        encoded = (((be(high, 2) ^ value ^ 0xA121) & 0xFFFF) << 16) | value
        return rol(encoded, 14, 32) ^ 0xF74324EE

    info_size, raw_size = block_size(38, 8), block_size(26, 32)
    return (length == len(data) and info_size > 0 and info_size == raw_size and
            ((info_size + 15) & ~15) < len(data) - 48)


class _Reader:
    def __init__(self, data):
        self.data = data
        self.offset = 0

    def raw(self, count):
        require(0 <= count <= len(self.data) - self.offset, 'native effect build is truncated')
        value = self.data[self.offset:self.offset + count]
        self.offset += count
        return value

    def integer(self, size=4):
        return int.from_bytes(self.raw(size), 'little')

    def text(self):
        return self.raw(self.integer(8)).decode('utf-8', 'strict')

    def end(self):
        require(self.offset == len(self.data), 'native effect build has trailing bytes')


def read_native_build(path):
    path = Path(path).resolve()
    path = path / 'runtime.bin' if path.is_dir() else path
    reader = _Reader(path.read_bytes())
    require(reader.text() == 'EFF_EFFECT_DEBUG_NATIVE/inactive-root+le64',
            'native effect build structure differs')
    key, prefab, root_name = reader.text(), reader.text(), reader.text()
    root, particles, count = reader.integer(8), reader.integer(8), reader.integer(8)
    require(0 < count <= (len(reader.data) - reader.offset) // 24 and root < count,
            'native effect bundle count/root differs')
    bundles = []
    for _ in range(count):
        file, digest = reader.text(), reader.text()
        dependency_count = reader.integer(8)
        require(dependency_count <= count, 'native effect dependency count differs')
        dependencies = [reader.integer(8) for _ in range(dependency_count)]
        require(len(set(dependencies)) == len(dependencies) and
                all(index < count and index != len(bundles) for index in dependencies),
                'native effect dependency graph differs')
        require(isinstance(file, str) and file and "\\" not in file and ":" not in file,
                'native effect bundle path is invalid')
        relative = PurePosixPath(file)
        require(not relative.is_absolute() and all(part not in ('', '.', '..') for part in file.split('/')),
                'native effect bundle path escapes its build directory')
        root = path.parent.resolve()
        bundle_path = (root / Path(*relative.parts)).resolve()
        require(bundle_path.is_relative_to(root), 'native effect bundle path escapes its build directory')
        data = bundle_path.read_bytes()
        require(sha256_bytes(data).hex().upper() == digest,
                'native effect build digest differs')
        require(native_archive(data),
                'effect input is not encoded in the native game envelope')
        bundles.append(dict(file=file, hash=digest, dependencies=dependencies, data=data))
    reader.end()
    return dict(key=key, prefab=prefab, rootName=root_name, root=root,
                particles=particles, bundles=bundles)
