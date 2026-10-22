"""Assemble the installed Blender package from canonical sources and core.

Does not publish a release or modify the game. Geometry modules have one source
of truth under tools/nativepack; bundled copies are build artifacts.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]

# These are the importable nativepack modules used by the add-on.  ``hashing``
# is deliberately listed: author_source, serialized_file and resource_file
# import it, and an old bundle silently failed only when an export first
# touched a SHA-256 path.
MODULES = (
    'author_source', 'serialized_file', 'type_tree', 'mesh', 'delta_apply',
    'resource_file', 'ini_package', 'ini_reader', 'texture_fields',
    'state_rules', 'payload', 'hashing', 'physics', 'shape_animation', 'runtime_controls', 'mod_directory',
)


# The exact same probe protects both author exports and release bundles. Import
# the pure invocation module directly: importing tools.Blender would require
# bpy and start the UI module as a side effect.
_protocol_spec = importlib.util.spec_from_file_location(
    '_eiem_bundle_offline_protocol', ROOT / 'tools/Blender/eiem_offline_reload.py')
_protocol_module = importlib.util.module_from_spec(_protocol_spec)
_protocol_spec.loader.exec_module(_protocol_module)
_protocol = _protocol_module._compiler_protocol




def _choose_compiler(explicit=None, abi=35):
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    # Build aliases only influence preference; protocols are checked by fields.
    candidates.extend((
        ROOT / ('bin/resource-pack-v35' if abi == 35 else 'bin/resource-pack') / 'eff_resource_pack.exe',
        ROOT / ('bin/resource-pack' if abi == 35 else 'bin/resource-pack-v35') / 'eff_resource_pack.exe',
        ROOT / 'tools/Blender/nativepack/eff_resource_pack.exe',
    ))
    seen = set()
    errors = []
    for candidate in candidates:
        candidate = candidate.resolve()
        if candidate in seen or not candidate.is_file():
            continue
        seen.add(candidate)
        try:
            return candidate, _protocol(candidate)
        except (OSError, ValueError) as error:
            errors.append(str(error))
            if explicit:
                break
    detail = '; '.join(errors) or '未找到资源编译器'
    raise ValueError('无法选择与生产 DLL 匹配的资源编译器：' + detail)


def bundle(destination, compiler=None, abi=35):
    destination = Path(destination).resolve()
    if destination.exists():
        raise FileExistsError('Blender bundle exists: ' + str(destination))
    compiler, protocol = _choose_compiler(compiler, abi)
    if protocol.get('sourcePreparation') != _protocol_module.SOURCE_PREPARATION:
        raise ValueError('Build the compiler with in-process author source discovery')
    destination.mkdir(parents=True)
    for path in (ROOT / 'tools/Blender').glob('*.py'):
        if path.name == '__init__.py' or path.name.startswith('eiem_'):
            shutil.copy2(path, destination / path.name)
    shutil.copytree(ROOT / 'tools/Blender/docs', destination / 'docs')
    core = destination / 'nativepack'
    core.mkdir()
    (core / '__init__.py').write_text('', encoding='utf-8')
    for name in MODULES:
        source = ROOT / 'tools/nativepack' / (name + '.py')
        if not source.is_file():
            raise FileNotFoundError('canonical nativepack module missing: ' + str(source))
        shutil.copy2(source, core / (name + '.py'))
    author_core = ROOT / 'bin/author-core/eff_author_core.dll'
    if not author_core.is_file():
        raise FileNotFoundError('missing author core DLL: ' + str(author_core))
    shutil.copy2(author_core, core / 'eff_author_core.dll')
    shutil.copy2(compiler, core / 'eff_resource_pack.exe')
    source_index = ROOT / 'bin/source-index-helper/eff_source_index.exe'
    if not source_index.is_file():
        raise FileNotFoundError('Build the paired offline source-index helper before packaging Blender')
    shutil.copy2(source_index, core / 'eff_source_index.exe')
    shutil.copy2(ROOT / 'THIRD_PARTY_NOTICES', destination / 'THIRD_PARTY_NOTICES')
    shutil.copy2(ROOT / 'LICENSE', destination / 'LICENSE')
    (core / 'package-protocol.json').write_text(json.dumps(protocol, indent=2) + '\n', 'utf-8')
    vendor = ROOT / 'bin/author-core/vendor'
    if not (vendor / 'PIL').is_dir():
        raise FileNotFoundError(
            'Install Pillow for the target Blender Python into bin/author-core/vendor')
    shutil.copytree(vendor, destination / 'vendor',
                    ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    return dict(
        format='EFF_BLENDER_NATIVE_EXPORT_BUNDLE', version=2,
        path=str(destination), compilerProtocol=protocol,
        sourceGeometryCore='tools/nativepack', sourceParser='src/eiem_native_input.h',
        files={
            p.relative_to(destination).as_posix():
            hashlib.sha256(p.read_bytes()).hexdigest().upper()
            for p in destination.rglob('*') if p.is_file()
        },
    )

if __name__=='__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', type=Path,
                        help='显式指定声明兼容数据结构的 eff_resource_pack.exe')
    parser.add_argument('--abi', type=int, choices=(35, 36), default=35,
                        help='构建布局别名；仅决定默认查找顺序')
    args = parser.parse_args()
    report = bundle(args.output, args.compiler, args.abi)
    args.output.with_suffix('.bundle.json').write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + '\n', 'utf-8')
    print(json.dumps(dict(path=report['path'], files=len(report['files']),
                          compiler=report['compilerProtocol']['compiler']),
                     ensure_ascii=False))
