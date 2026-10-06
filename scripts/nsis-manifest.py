#!/usr/bin/env python3
"""Uninstall only shipped files; retain unrelated files in a chosen installation directory."""
import sys
from pathlib import Path
stage, output = map(Path, sys.argv[1:])
def target(path):
    relative = str(path.relative_to(stage)).replace('/', '\\').replace('$', '$$').replace('"', '$\\"')
    return '"$INSTDIR\\' + relative + '"'
files = sorted(p for p in stage.rglob('*') if p.is_file())
directories = sorted((p for p in stage.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True)
lines = ['Delete ' + target(p) for p in files]
lines += ['RMDir ' + target(p) for p in directories]
lines += ['Delete "$INSTDIR\\Uninstall.exe"', 'RMDir "$INSTDIR"']
output.write_text('\n'.join(lines) + '\n', encoding='utf-8')
