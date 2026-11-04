"""Verify a locally built Blender continuous update ZIP and feed."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

BLENDER_ROOT = Path(__file__).resolve().parents[1] / 'Blender'
sys.path.insert(0, str(BLENDER_ROOT))
import eiem_release as updates


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def verify(archive, feed_path, revision):
    archive = Path(archive)
    feed_path = Path(feed_path)
    feed = json.loads(feed_path.read_text(encoding='utf-8-sig'))
    updates.validate_feed(feed)
    if feed['revision'] != revision or archive.name != f'EFF_Blender_main-{revision}.zip':
        raise ValueError('Blender update feed does not identify the local source revision')
    if feed['bytes'] != archive.stat().st_size or feed['sha256'] != digest(archive):
        raise ValueError('Blender update ZIP differs from the feed size or SHA256')
    with tempfile.TemporaryDirectory(prefix='eff-blender-update-check-') as temporary:
        candidate = updates.extract_package(archive, temporary)
        build = updates.verify_package(candidate, revision)
    return {
        'revision': revision,
        'version': feed['version'],
        'bytes': feed['bytes'],
        'sha256': feed['sha256'],
        'files': len(build['files']),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--feed', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.archive, args.feed, args.revision), indent=2))


if __name__ == '__main__':
    main()
