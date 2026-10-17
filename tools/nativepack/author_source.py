"""Read a preserved source baseline using the shared C++ serialized parser."""
import ctypes
import hashlib
import json
import contextlib
import os
from pathlib import Path
from .serialized_file import SerializedFile
from .type_tree import require
from .hashing import sha256_bytes

@contextlib.contextmanager
def source_read_lock(package):
    """Coordinate with AnimeStudio's whole-package replacement, without a watcher."""
    if os.name != 'nt':
        yield
        return
    from ctypes import wintypes
    path = str(Path(package).resolve()).rstrip('\\/').replace('\\', '/')
    path = path.translate(str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz'))
    name = 'Local\\EFFSource-' + hashlib.sha256(path.encode('utf-8')).hexdigest().upper()
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.CreateMutexW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.LPCWSTR]
    api.CreateMutexW.restype = wintypes.HANDLE
    api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    api.WaitForSingleObject.restype = wintypes.DWORD
    api.ReleaseMutex.argtypes = [wintypes.HANDLE]
    api.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = api.CreateMutexW(None, False, name)
    if not handle:
        raise OSError(ctypes.get_last_error(), '无法锁定解包来源')
    acquired = False
    try:
        result = api.WaitForSingleObject(handle, 0)
        require(result in (0, 0x80),
                '解包来源正被其他导出或 AnimeStudio 更新占用，请等待该任务完成后重试：' + str(Path(package).resolve()))
        acquired = True
        yield
    finally:
        if acquired:
            api.ReleaseMutex(handle)
        api.CloseHandle(handle)

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
        manifest = (self.package/'source/manifest.json').read_bytes()
        self.manifest_hash = sha256_bytes(manifest)
        self.manifest = json.loads(manifest.decode('utf-8-sig'))
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
    def assert_current(self):
        require(sha256_bytes((self.package/'source/manifest.json').read_bytes()) == self.manifest_hash,
                '解包来源在导出期间发生更新，暂存结果未发布；请重新导出')
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
