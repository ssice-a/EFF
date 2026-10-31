"""Verify that the typed Format 3 author ABI rejects malformed input."""
import ctypes
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    library_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'bin/author-assets/eff_author_assets.dll'
    library = ctypes.CDLL(str(library_path.resolve()))
    library.EFF_AuthorAssetsAbi.argtypes = []
    library.EFF_AuthorAssetsAbi.restype = ctypes.c_uint
    library.EFF_AuthorAssets_ExportTypedW.argtypes = [
        ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t,
        ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_size_t]
    library.EFF_AuthorAssets_ExportTypedW.restype = ctypes.c_int
    if library.EFF_AuthorAssetsAbi() != 3:
        raise AssertionError('Expected typed Format 3 author ABI 3')

    with tempfile.TemporaryDirectory(prefix='eff-author-typed-abi-smoke-') as folder:
        output = Path(folder) / 'format3'
        output.mkdir()
        signature = b'EFF_AUTHOR_TYPED_INPUT/invalid'
        payload = len(signature).to_bytes(8, 'little') + signature
        input_buffer = ctypes.create_string_buffer(payload)
        response = ctypes.create_string_buffer(4096)
        status = library.EFF_AuthorAssets_ExportTypedW(
            ctypes.cast(input_buffer, ctypes.POINTER(ctypes.c_ubyte)), len(payload),
            str(output), response, len(response))
        message = response.value.decode('utf-8', 'replace')
        if status != 1 or 'typed author input signature/version differs' not in message:
            raise AssertionError('Malformed typed input was not rejected at the schema boundary: ' + message)
        if list(output.iterdir()):
            raise AssertionError('Malformed input created Format 3 resources')
    print('PASS typed Format 3 author ABI 3 rejects malformed input without writing files')


if __name__ == '__main__':
    main()
