"""Check the actual paired DLL/catalog contract before native deployment."""
from pathlib import Path
import hashlib
import re
import struct
import sys


def verify(dll, root):
    binary = Path(dll).read_bytes()
    structures = set(re.findall(rb'EFF_AUTHOR_ASSETS/[^\x00\r\n]{1,500}', binary))
    if len(structures) != 1:
        raise ValueError('DLL must contain exactly one native author directory contract')
    structure = structures.pop()
    catalogs = list(Path(root).glob('*/assets.bin'))
    if not catalogs:
        raise ValueError('No native author directories found')
    for catalog in catalogs:
        data = catalog.read_bytes()
        if len(data) < 40 or hashlib.sha256(data[:-32]).digest() != data[-32:]:
            raise ValueError(f'Invalid author directory checksum: {catalog}')
        size, = struct.unpack_from('<Q', data)
        if data[8:8 + size] != structure:
            raise ValueError(f'DLL/author directory contract differs: {catalog}')
    return len(catalogs)


if __name__ == '__main__':
    count = verify(sys.argv[1], sys.argv[2])
    print(f'PASS deployment contract: {count} author directories match actual DLL')
