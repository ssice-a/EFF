"""Publish a verified local author DLL as a new immutable shared-tool generation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools/Blender'))
import eiem_shared_resources as shared


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def deploy(author, output):
    author = Path(author).resolve(strict=True)
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    root = shared.resource_root()
    if root is None:
        raise ValueError('Set the shared EFF resource root before local deployment')
    tools = root / 'tools'
    with shared._native_tools_install_lock(tools / 'install.lock'):
        installed = shared._installed_tools(root)
        if installed is None:
            raise ValueError('Install a complete shared native tool package before updating its author DLL')
        before = (tools / 'current.json').read_bytes()
        if digest(installed / 'nativepack/eff_author_assets.dll') == digest(author):
            print('PASS shared author DLL already current:', installed)
            return installed
        (output / 'shared-current-before.json').write_bytes(before)
        with tempfile.TemporaryDirectory(prefix='.eff-local-author-', dir=tools) as temporary:
            candidate = Path(temporary) / 'payload'
            shutil.copytree(installed, candidate,
                            ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
            target = candidate / 'nativepack/eff_author_assets.dll'
            shutil.copy2(author, target)
            # A separate process releases Windows DLL mappings before publish.
            # Pass actual source as a separate argument, avoiding shell quoting.
            check = ('import sys; sys.path.insert(0, sys.argv[1]); '
                     'from nativepack import control_program as p; '
                     'assert p.describe(sys.argv[3], sys.argv[2])["defaults"]["$probe"] == 2')
            subprocess.run([sys.executable, '-c', check, str(REPO / 'tools'), str(target),
                            '[ParameterProbe]\nvariable=$probe\ndefault=2\n'], check=True)
            files = {p.relative_to(candidate).as_posix(): digest(p)
                     for p in sorted(candidate.rglob('*')) if p.is_file()}
            for relative, expected in files.items():
                original = installed / relative
                if relative != 'nativepack/eff_author_assets.dll' and digest(original) != expected:
                    raise ValueError('Unchanged shared tool differs: ' + relative)
            archive = output / 'EFF_Shared_Native_Tools_local.zip'
            if archive.exists():
                raise FileExistsError(archive)
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=1) as packed:
                for relative in files:
                    packed.write(candidate / relative, 'EFF_Native_Tools/' + relative)
            identity = digest(archive)
            destination = tools / identity
            if destination.exists():
                raise FileExistsError(destination)
            os.replace(candidate, destination)
            if (tools / 'current.json').read_bytes() != before:
                raise ValueError('Shared tool selection changed during local deployment')
            current = dict(format=shared.TOOLS_INSTALL_FORMAT, sha256=identity,
                           release='local-ini-controls-20261112')
            pending = Path(temporary) / 'current.json'
            pending.write_text(json.dumps(current, indent=2) + '\n', encoding='utf-8')
            os.replace(pending, tools / 'current.json')
            if shared._installed_tools(root) != destination:
                pending.write_bytes(before)
                os.replace(pending, tools / 'current.json')
                raise ValueError('Published local tool generation is not recognized')
            receipt = dict(format='EFF_LOCAL_SHARED_AUTHOR_DEPLOYMENT/1', previous=installed.as_posix(),
                           target=destination.as_posix(), archive=archive.as_posix(),
                           sha256=identity, files=files)
            (output / 'SharedAuthor.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
            print('PASS installed shared author DLL:', destination)
            print('PASS shared tool receipt:', output / 'SharedAuthor.json')
            return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--author', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    deploy(args.author, args.output)
