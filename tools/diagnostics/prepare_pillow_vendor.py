"""Merge Pillow wheels for Blender's Python 3.11 and 3.13 runtimes."""
import argparse
from pathlib import Path, PurePosixPath
import re
import shutil
import tempfile
import zipfile


WHEEL_ABIS = ('cp311', 'cp313')
NATIVE_MEMBER = re.compile(r'^PIL/(.+)\.(cp311|cp313)-win_amd64\.pyd$')


def _read_wheel(path, abi):
    files = {}
    extensions = {}
    with zipfile.ZipFile(path) as wheel:
        for member in wheel.infolist():
            relative = PurePosixPath(member.filename)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Pillow wheel contains an unsafe path: ' + member.filename)
            if not relative.parts or relative.parts[0] != 'PIL' or member.is_dir():
                continue
            match = NATIVE_MEMBER.fullmatch(member.filename)
            if match:
                module, member_abi = match.groups()
                if member_abi != abi:
                    continue
                extensions[module] = member.filename
            else:
                files[member.filename] = member.filename

    if 'PIL/__init__.py' not in files:
        raise ValueError(f'{abi} Pillow wheel does not contain PIL/__init__.py')
    if '_imaging' not in extensions:
        raise ValueError(f'{abi} Pillow wheel does not contain _imaging')
    return files, extensions


def prepare_vendor(cp311_wheel, cp313_wheel, destination):
    """Create a vendor tree with the Python modules and both native ABIs."""
    _, cp311_extensions = _read_wheel(cp311_wheel, 'cp311')
    cp313_files, cp313_extensions = _read_wheel(cp313_wheel, 'cp313')
    if set(cp311_extensions) != set(cp313_extensions):
        only_311 = sorted(set(cp311_extensions) - set(cp313_extensions))
        only_313 = sorted(set(cp313_extensions) - set(cp311_extensions))
        raise ValueError(f'Pillow wheel native modules differ: cp311-only={only_311}, cp313-only={only_313}')

    destination = Path(destination).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix='.pillow-vendor-', dir=destination.parent))
    staged_vendor = staging / 'vendor'
    backup = staging / 'previous-vendor'
    try:
        if destination.is_dir():
            shutil.copytree(destination, staged_vendor)
        else:
            staged_vendor.mkdir()
        pil = staged_vendor / 'PIL'
        if pil.exists():
            shutil.rmtree(pil)
        pil.mkdir(parents=True)

        with zipfile.ZipFile(cp313_wheel) as wheel:
            for member in cp313_files.values():
                target = staged_vendor.joinpath(*PurePosixPath(member).parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with wheel.open(member) as source, target.open('wb') as output:
                    shutil.copyfileobj(source, output)

        for wheel_path, extensions in (
                (cp311_wheel, cp311_extensions),
                (cp313_wheel, cp313_extensions)):
            with zipfile.ZipFile(wheel_path) as wheel:
                for member in extensions.values():
                    target = staged_vendor.joinpath(*PurePosixPath(member).parts)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with wheel.open(member) as source, target.open('wb') as output:
                        shutil.copyfileobj(source, output)

        for abi in WHEEL_ABIS:
            if not (pil / f'_imaging.{abi}-win_amd64.pyd').is_file():
                raise ValueError('Prepared Pillow vendor is missing _imaging for ' + abi)

        if destination.exists():
            destination.rename(backup)
        try:
            staged_vendor.rename(destination)
        except Exception:
            if backup.exists() and not destination.exists():
                backup.rename(destination)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    finally:
        shutil.rmtree(staging, ignore_errors=True)

    print('PASS Pillow vendor:', destination,
          'native modules:', len(cp311_extensions),
          'ABIs:', ', '.join(WHEEL_ABIS))
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cp311-wheel', type=Path, required=True)
    parser.add_argument('--cp313-wheel', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    prepare_vendor(args.cp311_wheel, args.cp313_wheel, args.destination)


if __name__ == '__main__':
    main()
