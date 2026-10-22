"""Keep independent nested Mods outside a parent's author-resource scope."""
import os
from pathlib import Path
import stat

from .type_tree import require


def _reparse(path):
    return path.is_symlink() or bool(getattr(path.lstat(), 'st_file_attributes', 0)
        & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0))


def entries(root):
    root = Path(root)

    def failed(error):
        raise error

    for current, directories, files in os.walk(root, onerror=failed, followlinks=False):
        current = Path(current)
        for name in list(directories):
            child = current / name
            require(not _reparse(child), 'symlink/reparse resources forbidden')
            if (child / 'mod.ini').is_file():
                directories.remove(name)
                yield 'mod', child
        for name in files:
            path = current / name
            require(not _reparse(path), 'symlink/reparse resources forbidden')
            yield 'file', path


def package_files(root):
    return (path for kind, path in entries(root) if kind == 'file')


def child_mods(root):
    return (path for kind, path in entries(root) if kind == 'mod')
