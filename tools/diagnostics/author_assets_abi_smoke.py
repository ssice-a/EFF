"""Verify the current author DLL memory ABI rejects malformed input cleanly."""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'Blender'))
from eiem_native_export import write_format3_native_assets


def main():
    library = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'bin/author-assets/eff_author_assets.dll'
    with tempfile.TemporaryDirectory(prefix='eff-author-abi-smoke-') as folder:
        output = Path(folder) / 'format3'
        try:
            write_format3_native_assets(
                library, b'[Mod]\nformat=2\nid=Invalid\n', [], {}, {}, output)
        except ValueError as error:
            if 'Mod and Constants required' not in str(error):
                raise
        else:
            raise AssertionError('Malformed in-memory Mod was accepted')
        template = dict(
            kind='Mesh', path='assets/test.asset', asset='S_test',
            cab='CAB-test', pathId=-1,
            file=str(Path(folder) / 'template.bytes'), sha256='A' * 64)
        try:
            write_format3_native_assets(
                library, b'[Mod]\nformat=2\nid=Invalid\n', [template], {}, {}, output)
        except ValueError as error:
            if 'Mod and Constants required' not in str(error):
                raise
        else:
            raise AssertionError('Negative Unity path ID was rejected or malformed Mod was accepted')
        if output.exists():
            raise AssertionError('Rejected input created an output directory')
    print('PASS author-assets memory ABI 2: malformed package rejected; signed nonzero template IDs accepted')


if __name__ == '__main__':
    main()
