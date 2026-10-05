"""Exact SHA-256 using Windows CNG for large author payloads.

Blender's bundled OpenSSL can lack the CPU acceleration used by CNG. Keep
the same digest and all container checks; small buffers and other platforms
use hashlib. The provider is stateless and shared, never the hash state.
"""
import ctypes
from functools import lru_cache
import hashlib
import os


class _Provider:
    def __init__(self):
        self.api = ctypes.WinDLL('bcrypt.dll')
        self.api.BCryptOpenAlgorithmProvider.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_wchar_p,
                                                       ctypes.c_wchar_p, ctypes.c_ulong]
        self.api.BCryptOpenAlgorithmProvider.restype = ctypes.c_long
        self.api.BCryptCloseAlgorithmProvider.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.api.BCryptCloseAlgorithmProvider.restype = ctypes.c_long
        self.api.BCryptHash.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong,
                                       ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_ulong]
        self.api.BCryptHash.restype = ctypes.c_long
        self.handle = ctypes.c_void_p()
        status = self.api.BCryptOpenAlgorithmProvider(ctypes.byref(self.handle), 'SHA256', None, 0)
        if status < 0:
            raise OSError('CNG SHA-256 provider failed: %08X' % (status & 0xffffffff))

    def __del__(self):
        if getattr(self, 'handle', None) and self.handle.value:
            self.api.BCryptCloseAlgorithmProvider(self.handle, 0)

    def digest(self, data):
        output = ctypes.create_string_buffer(32)
        status = self.api.BCryptHash(self.handle, None, 0, ctypes.c_char_p(data), len(data), output, 32)
        if status < 0:
            raise OSError('CNG SHA-256 failed: %08X' % (status & 0xffffffff))
        return output.raw


@lru_cache(maxsize=1)
def _provider():
    return _Provider()


def sha256_bytes(data):
    if os.name == 'nt' and 2048 <= len(data) <= 0xffffffff:
        # bytes keeps its buffer alive for the synchronous CNG call.
        return _provider().digest(data if isinstance(data, bytes) else bytes(data))
    return hashlib.sha256(data).digest()
