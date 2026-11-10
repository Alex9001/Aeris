#!/usr/bin/env python3
"""Verify native slices and that every non-system Mach-O dependency is bundled."""
import argparse
from pathlib import Path
import subprocess


def rpaths(path):
    lines = subprocess.check_output(['otool', '-l', str(path)], text=True).splitlines()
    paths = []
    for index, line in enumerate(lines):
        if line.strip() == 'cmd LC_RPATH':
            for entry in lines[index + 1:index + 4]:
                if entry.strip().startswith('path '):
                    paths.append(entry.strip()[5:].split(' (offset ')[0])
    return paths


def expand(path, loader, executable):
    for token, directory in [('@loader_path', loader.parent),
                             ('@executable_path', executable.parent)]:
        if path == token or path.startswith(token + '/'):
            return directory / path[len(token):].lstrip('/')
    return None


def bundled_dependency(dependency, loader, executable, app, search_paths):
    if dependency.startswith(('/System/', '/usr/lib/')):
        return True
    if not dependency.startswith('@'):
        return False
    if dependency.startswith('@rpath/'):
        candidates = [directory / dependency[len('@rpath/'):] for directory in search_paths]
    else:
        path = expand(dependency, loader, executable)
        candidates = [path] if path is not None else []
    app = app.resolve()
    return any(path.is_file() and app in path.resolve().parents for path in candidates)


def audit(app, arch=None):
    executable = app / 'Contents/MacOS/aeris'
    inherited = [expand(path, executable, executable) for path in rpaths(executable)]
    for path in app.rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        if 'Mach-O' not in subprocess.check_output(['file', '-b', str(path)], text=True):
            continue
        if arch:
            subprocess.run(['lipo', str(path), '-verify_arch', arch], check=True)
        search = [expand(entry, path, executable) for entry in rpaths(path)] + inherited
        search = [entry for entry in search if entry is not None]
        for line in subprocess.check_output(['otool', '-L', str(path)], text=True).splitlines()[1:]:
            if line.endswith(':'):
                continue
            dependency = line.strip().split(' (')[0]
            if not bundled_dependency(dependency, path, executable, app, search):
                raise SystemExit(f'Unbundled dependency in {path}: {dependency}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    parser.add_argument('--arch')
    args = parser.parse_args()
    audit(args.app, args.arch)


if __name__ == '__main__':
    main()
