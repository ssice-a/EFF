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
        build_root = path.parent.resolve()
        bundle_path = (build_root / Path(*relative.parts)).resolve()
        require(bundle_path.is_relative_to(build_root), 'native effect bundle path escapes its build directory')
        data = bundle_path.read_bytes()
        require(sha256_bytes(data).hex().upper() == digest,
                'native effect build digest differs')
        require(native_archive(data),
                'effect input is not encoded in the native game envelope')
        bundles.append(dict(file=file, hash=digest, dependencies=dependencies, data=data))
    game_resources = []
    section_at = reader.offset
    next_section = reader.text() if reader.offset < len(reader.data) else ''
    reader.offset = section_at
    if next_section.startswith('EFF_GAME_RESOURCES/'):
        version = reader.text()
        require(version in ('EFF_GAME_RESOURCES/1', 'EFF_GAME_RESOURCES/2', 'EFF_GAME_RESOURCES/3'),
                'native game resource declaration differs')
        count = reader.integer(8)
        require(count <= 4096, 'too many game resource dependencies')
        for _ in range(count):
            path, kind, asset = reader.text(), reader.text(), reader.text()
            require(path.startswith(('assets/', 'packages/')) and '\\' not in path and ':' not in path and
                    all(p not in ('', '.', '..') for p in path.split('/')) and kind and (asset or version == 'EFF_GAME_RESOURCES/3'),
                    'invalid game resource dependency')
            resource = dict(path=path, type=kind, asset=asset)
            if version in ('EFF_GAME_RESOURCES/2', 'EFF_GAME_RESOURCES/3'):
                prefab_source, root_source = reader.text(), reader.text()
                require(not root_source or prefab_source, 'game resource root has no source Prefab')
                if prefab_source:
                    require(prefab_source.startswith('assets/') and prefab_source.endswith('.prefab') and
                            '\\' not in prefab_source and ':' not in prefab_source and
                            all(p not in ('', '.', '..') for p in prefab_source.split('/')),
                            'invalid game resource source Prefab')
                    resource.update(sourcePrefab=prefab_source, sourceRoot=root_source)
            if version == 'EFF_GAME_RESOURCES/3':
                cab, pid, size = reader.text(), reader.integer(8), reader.integer(8)
                require(size <= 65536 and (bool(cab) == bool(pid)), 'invalid effect relocation declaration')
                patches = []
                for _ in range(size):
                    bundle, offset, kind_id = reader.integer(8), reader.integer(8), reader.integer(8)
                    require(bundle < len(bundles) and kind_id in (0, 1), 'invalid effect relocation site')
                    require(offset + (36 if kind_id == 0 else 8) <= len(bundles[bundle]['data']),
                            'effect relocation site exceeds bundle')
                    patches.append(dict(bundle=bundle, offset=offset, kind=kind_id))
                if cab:
                    require(len(cab) == 36 and cab.startswith('CAB-') and
                            all(c in '0123456789abcdefABCDEF' for c in cab[4:]) and patches,
                            'invalid effect relocation native slot')
                    resource.update(relocationCab=cab, relocationId=pid, relocations=patches)
            game_resources.append(resource)
    author_resources = []
    if reader.offset < len(reader.data):
        require(reader.text() == 'EFF_AUTHOR_RESOURCES/1', 'native author resource version differs')
        count = reader.integer(8)
        require(count <= 4096, 'too many effect author dependencies')
        for _ in range(count):
            resource, kind, cab = reader.text(), reader.text(), reader.text()
            pid, size = reader.integer(8), reader.integer(8)
            require(resource and kind in ('Mesh', 'Material', 'Texture2D') and
                    len(cab) == 36 and cab.startswith('CAB-') and
                    all(c in '0123456789abcdefABCDEF' for c in cab[4:]) and pid and
                    0 < size <= 65536, 'invalid native author resource marker')
            patches = []
            for _ in range(size):
                bundle, offset, kind_id = reader.integer(8), reader.integer(8), reader.integer(8)
                require(bundle < len(bundles) and kind_id in (0, 1) and
                        offset + (36 if kind_id == 0 else 8) <= len(bundles[bundle]['data']),
                        'invalid author resource relocation site')
                patches.append(dict(bundle=bundle, offset=offset, kind=kind_id))
            author_resources.append(dict(resource=resource, type=kind, relocationCab=cab,
                                         relocationId=pid, relocations=patches))
    reader.end()
    return dict(key=key, prefab=prefab, rootName=root_name, root=root,
                particles=particles, bundles=bundles, gameResources=game_resources,
                authorResources=author_resources)
