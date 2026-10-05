"""Native digest equivalence and export-cache invalidation contracts."""
import hashlib
import os
from pathlib import Path
import tempfile
from concurrent.futures import ThreadPoolExecutor
import unittest

from nativepack.hashing import sha256_bytes
from nativepack.resource_file import MESH, MATERIAL, HEADER, encode_resource, decode_resource, read_resource


class HashingTests(unittest.TestCase):
    def test_vectors_and_concurrent_buffers(self):
        samples = [b'', b'abc', bytes(range(256))*8, bytes(range(251))*16711]
        with ThreadPoolExecutor(max_workers=4) as pool:
            self.assertEqual(list(pool.map(sha256_bytes, samples*3)),
                             [hashlib.sha256(value).digest() for value in samples*3])
        for value in samples:
            self.assertEqual(sha256_bytes(bytearray(value)), hashlib.sha256(value).digest())
            self.assertEqual(sha256_bytes(memoryview(value)), hashlib.sha256(value).digest())

    def test_large_container_rejects_bad_block_with_valid_outer_hash(self):
        data = bytearray(encode_resource(MESH, '12'*16, {'field': b'x'*16384}))
        data[-1] ^= 1
        fields = list(HEADER.unpack_from(data))
        fields[-1] = hashlib.sha256(data[HEADER.size:]).digest()
        data[:HEADER.size] = HEADER.pack(*fields)
        with self.assertRaisesRegex(ValueError, 'block checksum'):
            decode_resource(data, MESH)

    def test_cache_checks_current_content_and_isolates_results(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'resource.mesh'
            first = encode_resource(MESH, '12'*16, {'field': b'x'*16384})
            path.write_bytes(first)
            cache = {}
            result = read_resource(path, MESH, cache)
            result['blocks']['field'] = b'caller mutation'
            result['descriptors'][0]['name'] = 'caller mutation'
            self.assertEqual(read_resource(path, MESH, cache), decode_resource(first, MESH))
            stamp = path.stat()
            second = encode_resource(MESH, '12'*16, {'field': b'y'*16384})
            path.write_bytes(second)
            os.utime(path, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
            self.assertEqual(read_resource(path, MESH, cache), decode_resource(second, MESH))
            cache['__trusted_roots__'] = (Path(folder),)
            self.assertEqual(read_resource(path, MESH, cache), decode_resource(second, MESH))
            with self.assertRaisesRegex(ValueError, 'kind'):
                read_resource(path, MATERIAL, cache)
            corrupt = bytearray(second); corrupt[-1] ^= 1
            path.write_bytes(corrupt)
            with self.assertRaisesRegex(ValueError, 'checksum'):
                read_resource(path, MESH, cache)


if __name__ == '__main__':
    unittest.main()
