"""Package the paired Blender add-on and native tools for an EFF release."""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]
ADDON = ROOT / 'tools/Blender'
sys.path.insert(0, str(ROOT / 'tools'))


def addon_version():
    source = ast.parse((ADDON / '__init__.py').read_text('utf-8'))
    info = next(ast.literal_eval(node.value) for node in source.body
                if isinstance(node, ast.Assign)
                and any(isinstance(target, ast.Name) and target.id == 'bl_info'
                        for target in node.targets))
    return tuple(info['version'])


def package(output, compiler=None, dll_protocol=None, verify_pin=True,
            framework_tag=None, manifest_output=None):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = addon_version()
    archive = output / ('EFF_Blender_Native_v' + '.'.join(map(str, version)) + '.zip')
    if archive.exists():
        raise FileExistsError('Native base already exists: ' + str(archive))

    spec = importlib.util.spec_from_file_location(
        '_eiem_bundle_blender_native', ROOT / 'tools/diagnostics/bundle_blender_native_export.py')
    bundle = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bundle)

    with tempfile.TemporaryDirectory(prefix='.blender-native-base-', dir=output) as temp:
        package_root = Path(temp) / 'EFF_Blender'
        report = bundle.bundle(package_root, compiler=compiler, abi=35)
        presets = ADDON / 'physics_presets'
        if presets.is_dir():
            shutil.copytree(presets, package_root / 'physics_presets')

        if dll_protocol:
            declared = json.loads(Path(dll_protocol).read_text('utf-8-sig'))
            actual = report['compilerProtocol']
            for key in ('compiledMod', 'compiledModStore', 'resourceUpdate'):
                if actual.get(key) != declared.get(key):
                    raise ValueError('Blender compiler and DLL protocol differ at ' + key)
            for key, value in actual.get('structures', {}).items():
                if declared.get('structures', {}).get(key) != value:
                    raise ValueError('Blender compiler and DLL protocol differ at structures.' + key)

        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_STORED) as target:
            for path in sorted(package_root.rglob('*')):
                if path.is_file():
                    name = 'EFF_Blender/' + path.relative_to(package_root).as_posix()
                    entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                    entry.compress_type = zipfile.ZIP_STORED
                    entry.create_system = 0
                    entry.external_attr = 0
                    target.writestr(entry, path.read_bytes())

    manifest = json.loads((ADDON / 'native-base.json').read_text('utf-8'))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if verify_pin and (archive.stat().st_size != manifest.get('bytes')
                       or digest != manifest.get('sha256')):
        raise ValueError('Generated native base differs from native-base.json')
    if manifest_output:
        if not framework_tag:
            raise ValueError('--framework-tag is required with --manifest-output')
        receipt = {
            'format': 'EFF_BLENDER_NATIVE_BASE/1',
            'framework': framework_tag,
            'url': 'https://github.com/ssice-a/EFF/releases/download/'
                   + framework_tag + '/' + archive.name,
            'bytes': archive.stat().st_size,
            'sha256': digest,
        }
        Path(manifest_output).write_text(
            json.dumps(receipt, ensure_ascii=False, indent=2) + '\n',
            encoding='utf-8')
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--dll-protocol', type=Path)
    parser.add_argument('--allow-unpinned', action='store_true',
                        help='Build a one-time candidate for a new pinned native base')
    parser.add_argument('--framework-tag',
                        help='EFF release tag that will publish the new base')
    parser.add_argument('--manifest-output', type=Path,
                        help='Write the exact size/SHA256/URL receipt for native-base.json')
    args = parser.parse_args()
    print(package(args.output, args.compiler, args.dll_protocol,
                  verify_pin=not args.allow_unpinned,
                  framework_tag=args.framework_tag,
                  manifest_output=args.manifest_output))
