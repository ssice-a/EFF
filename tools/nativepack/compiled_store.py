"""Read committed native package directories without rehashing source payloads."""
import hashlib
import os
from pathlib import Path
import re
import struct

STORES = {'EFF_MOD_BLOCKS/1', 'EFF_MOD_BLOCKS/2'}
BLOCK_PATH = re.compile(r'compiled\.blocks/([0-9A-F]{64})\.bin')
TRANSIENT_PATH = re.compile(r'(?:compiled\.bin|compiled\.blocks/[0-9A-F]{64}\.bin)\.tmp-[0-9]+-[0-9]+-[0-9]+')


def _directory(data, generation, index_offset, external):
    position = 0

    def number():
        nonlocal position
        if len(data) - position < 8:
            raise ValueError('truncated block index')
        value, = struct.unpack_from('<Q', data, position)
        position += 8
        return value

    def string():
        nonlocal position
        size = number()
        if size > len(data) - position:
            raise ValueError('truncated block index string')
        value = data[position:position + size].decode('utf-8')
        position += size
        return value

    structure = 'EFF_MOD_BLOCK_INDEX/external-blobs/le64' if external else 'EFF_MOD_BLOCK_INDEX/le64'
    if string() != structure or number() != generation:
        raise ValueError('block index structure/generation differs')
    count = number()
    if count > (len(data) - position) // 8:
        raise ValueError('block index count exceeds encoded extent')
    names, blobs = set(), {}
    for _ in range(count):
        name, offset, size, digest = string(), number(), number(), string()
        if not name or name in names or re.fullmatch('[0-9A-F]{64}', digest) is None:
            raise ValueError('block index name/digest invalid')
        names.add(name)
        if external:
            path = string()
            if offset != 0 or path != 'compiled.blocks/' + digest + '.bin':
                raise ValueError('block index external path invalid')
            if path in blobs and blobs[path] != (size, digest):
                raise ValueError('block index shared size differs')
            blobs[path] = (size, digest)
        elif offset < 192 or offset > index_offset or size > index_offset - offset:
            raise ValueError('block index payload extent invalid')
    if position != len(data):
        raise ValueError('block index suffix invalid')
    return blobs


def snapshot(root, validate_files=False):
    """Return the latest checked commit, or None for absent/legacy packets."""
    root = Path(root)
    path = root / 'compiled.bin'
    if not path.is_file():
        return None
    with path.open('rb') as stream:
        status = os.fstat(stream.fileno())
        header = stream.read(192)
        store = header[:16].decode('ascii', errors='replace')
        if store not in STORES:
            return None
        valid = []
        for number in range(2):
            slot = header[16 + number * 88:16 + (number + 1) * 88]
            if len(slot) != 88 or hashlib.sha256(slot[:56]).digest() != slot[56:]:
                continue
            generation, offset, size = struct.unpack_from('<QQQ', slot)
            if not generation or offset < 192 or offset > status.st_size or size > status.st_size - offset:
                continue
            stream.seek(offset)
            data = stream.read(size)
            digest = hashlib.sha256(data)
            if len(data) != size or digest.digest() != slot[24:56]:
                continue
            try:
                blobs = _directory(data, generation, offset, store == 'EFF_MOD_BLOCKS/2')
            except (ValueError, UnicodeError):
                continue
            valid.append((generation, digest.hexdigest(), blobs))
    if not valid:
        raise ValueError('Mod 没有可读的已提交代际，保留上一发布')
    generation, digest, blobs = max(valid, key=lambda value: value[0])
    if validate_files:
        for relative, (size, _) in blobs.items():
            file = root / relative
            if not file.is_file() or file.stat().st_size != size:
                raise ValueError('Mod 分块文件缺失或大小错误：' + relative)
    return dict(store=store, revision=(status.st_dev, status.st_ino, 'blocks', generation, digest), blobs=blobs)
