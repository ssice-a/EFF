"""Verify that the typed Format 3 author ABI rejects malformed input."""
import ctypes
import hashlib
from pathlib import Path
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def _typed_input_with_unbound_material(template_path):
    def number(value):
        return struct.pack('<Q', value)

    def string(value):
        data = value.encode('utf-8')
        return number(len(data)) + data

    def blob(value):
        return number(len(value)) + value

    def strings(values):
        return number(len(values)) + b''.join(string(value) for value in values)

    template_bytes = Path(template_path).read_bytes()
    template_hash = hashlib.sha256(template_bytes).hexdigest().upper()
    rows = [
        string('EFF_AUTHOR_TYPED_INPUT/1'), string('testmod'),
        number(1), string('Mesh'), string('assets/game/mesh.mesh'), string('MeshA'),
        string('CAB-mesh'), struct.pack('<q', 42), string(str(template_path)), string(template_hash),
        number(1), string('PartA'), string('MeshPartA'), string('meshes/PartA.effres'), string(''),
        string('assets/game/mesh.mesh'), string('MeshA'), number(0), number(3), number(3),
        strings([]), strings([]), number(1), string('field'), blob(b'x'), blob(b''),
        number(0),
    ]
    return b''.join(rows)


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

    with tempfile.TemporaryDirectory(prefix='eff-author-empty-material-abi-') as folder:
        template = Path(folder) / 'mesh-template.bin'
        template.write_bytes(b'invalid serialized file')
        payload = _typed_input_with_unbound_material(template)
        input_buffer = ctypes.create_string_buffer(payload)
        response = ctypes.create_string_buffer(4096)
        status = library.EFF_AuthorAssets_ExportTypedW(
            ctypes.cast(input_buffer, ctypes.POINTER(ctypes.c_ubyte)), len(payload),
            folder, response, len(response))
        message = response.value.decode('utf-8', 'replace')
        if status != 1 or 'SerializedFile header truncated' not in message:
            raise AssertionError('A Mesh without a Material override failed identity validation: ' + message)
        if list(Path(folder).glob('*.effres')):
            raise AssertionError('Rejected malformed template created Format 3 resources')
    print('PASS typed Format 3 ABI rejects malformed input and accepts omitted Mesh Material overrides')


if __name__ == '__main__':
    main()
