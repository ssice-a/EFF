"""Read a preserved source baseline using the shared C++ serialized parser."""
import ctypes
import hashlib
import json
from pathlib import Path
from .serialized_file import SerializedFile
from .type_tree import require
from .hashing import sha256_bytes

class AuthorCore:
    def __init__(self, path):
        self.dll = ctypes.CDLL(str(Path(path).resolve()))
        self.dll.EFF_SourceSchema.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.dll.EFF_SourceSchema.restype = ctypes.c_void_p
        self.dll.EFF_FreeAuthorResult.argtypes = [ctypes.c_void_p]
        self.dll.EFF_FreeAuthorResult.restype = None
    def schema(self, data):
        buffer = ctypes.create_string_buffer(data)
        result = self.dll.EFF_SourceSchema(buffer, len(data))
        require(result, 'author core could not allocate a result')
        try: parsed = json.loads(ctypes.string_at(result).decode('utf-8'))
        finally: self.dll.EFF_FreeAuthorResult(result)
        require('error' not in parsed, parsed.get('error', 'invalid author schema'))
        return parsed

class AuthorSource:
    def __init__(self, package, core):
        self.package = Path(package).resolve()
        self.core = core
        self.manifest = json.loads((self.package/'source/manifest.json').read_text('utf-8-sig'))
        require(self.manifest.get('format') == 'EFFSOURCE' and isinstance(self.manifest.get('serializedFiles'), list) and isinstance(self.manifest.get('resources'), list), 'unsupported source baseline structure')
        self.rows = {}
        identities = set()
        for row in self.manifest['serializedFiles']:
            cab = row['cab']
            require(isinstance(cab, str) and cab and cab.casefold() not in identities,
                    'duplicate/empty source CAB identity')
            identities.add(cab.casefold())
            self.rows[cab] = row
        self.files = {}
    def resolve(self, kind, path, asset):
        values = [r for r in self.manifest['resources'] if
                  (r['type'], r['logicalPath'], r['name']) == (kind, path, asset)]
        require(len(values) == 1, f'{kind} source has {len(values)} matches: {path} / {asset}')
        return values[0]
    def file(self, resource):
        cab = resource['native']['cab']
        if cab not in self.files:
            row = self.rows[cab]
            path = (self.package/row['snapshot']['file']).resolve()
            require(self.package in path.parents, 'source snapshot escapes baseline')
            data = path.read_bytes()
            require(sha256_bytes(data).hex().upper() == row['snapshot']['sha256'], 'source snapshot hash')
            self.files[cab] = SerializedFile(data, row, self.core.schema(data))
        return self.files[cab]
    def tree(self, resource):
        return self.file(resource).tree(resource['native']['pathId'])
