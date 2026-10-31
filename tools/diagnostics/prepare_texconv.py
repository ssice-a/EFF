"""Download the pinned DirectXTex texconv used by shared EFF tools."""
import hashlib
from pathlib import Path
import tempfile
from urllib.request import Request, urlopen


URL = 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe'
SHA256 = 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'
ROOT = Path(__file__).resolve().parents[2]
DESTINATION = ROOT / 'bin/texconv/texconv.exe'


def digest(path):
    checksum = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            checksum.update(block)
    return checksum.hexdigest().upper()


def prepare(destination=DESTINATION):
    destination = Path(destination)
    if destination.is_file() and digest(destination) == SHA256:
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    request = Request(URL, headers={'User-Agent': 'EFF-Texture-Tool/1'})
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix='.texconv-', suffix='.exe',
                                         dir=destination.parent, delete=False) as output:
            temporary = Path(output.name)
            with urlopen(request, timeout=60) as response:
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    output.write(block)
        if digest(temporary) != SHA256:
            raise ValueError('Downloaded DirectXTex texconv.exe SHA256 differs from the pinned release')
        temporary.replace(destination)
        return destination
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    print('PASS pinned DirectXTex texconv:', prepare())
