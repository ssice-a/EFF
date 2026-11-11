"""Author-side access to the runtime's control parser; no second interpreter."""
from __future__ import annotations
import ctypes
import io
import json
import os
from pathlib import Path

_libraries = {}
_configured = None


def configure(path):
    global _configured
    _configured = Path(path).resolve()


def describe(ini, library=None):
    candidates = [library, _configured, os.environ.get('EFF_AUTHOR_ASSETS_DLL'),
                  Path(__file__).with_name('eff_author_assets.dll'),
                  Path(__file__).resolve().parents[2] / 'bin/author-assets/eff_author_assets_format3.dll',
                  Path(__file__).resolve().parents[2] / 'bin/author-assets/eff_author_assets.dll']
    path = next((Path(p).resolve() for p in candidates if p and Path(p).is_file()), None)
    if path is None:
        raise ValueError('Unified INI controls require the current shared eff_author_assets.dll')
    key = str(path)
    if key not in _libraries:
        dll = ctypes.CDLL(key)
        try:
            fn = dll.EFF_ControlProgram_Describe
        except AttributeError as error:
            raise ValueError('Shared author tools do not support unified INI controls; update local tools') from error
        fn.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
        fn.restype = ctypes.c_int
        _libraries[key] = (dll, fn)
    if isinstance(ini, str):
        source = ini.encode('utf-8')
    else:
        stream = io.StringIO()
        ini.write(stream)
        source = stream.getvalue().encode('utf-8')
    fn = _libraries[key][1]
    capacity = max(65536, len(source) * 4)
    while True:
        response = ctypes.create_string_buffer(capacity)
        status = fn(ctypes.c_char_p(source), len(source), response, capacity)
        if status == 2:
            capacity *= 2
            continue
        text = response.value.decode('utf-8')
        if status:
            raise ValueError(text)
        return json.loads(text)
