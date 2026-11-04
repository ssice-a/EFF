"""Install a locally built EFF Blender package with the add-on's rollback path."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import tempfile

TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_ROOT / 'Blender'))

import eiem_release as updates
import eiem_update_worker as worker


def install(package, target):
    package = Path(package).resolve()
    target = updates.check_install_target(target)
    build = updates.verify_package(package)
    for path in package.rglob('*'):
        if updates.linked(path):
            raise ValueError('Built Blender package contains a link')

    stage = Path(tempfile.mkdtemp(prefix='.eff-local-stage-', dir=target.parent))
    candidate = stage / 'unpacked' / 'EFF_Blender'
    candidate.parent.mkdir(parents=True)
    shutil.copytree(package, candidate)
    job_path = stage / 'job.json'
    job = {
        'format': 'EFF_BLENDER_INSTALL/1',
        'target': str(target),
        'candidate': str(candidate),
        'revision': build['revision'],
        'oldRevision': updates.read_build(target).get('revision'),
        'status': 'prepared',
    }
    job_path.write_text(json.dumps(job, ensure_ascii=False, indent=2), encoding='utf-8')
    try:
        result = worker.install(job_path)
        return result
    finally:
        shutil.rmtree(stage, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--target', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(install(args.package, args.target), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
