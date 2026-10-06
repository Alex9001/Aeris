#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
for path in Path(sys.argv[1]).rglob('*'):
    if not path.is_file() or path.is_symlink(): continue
    if 'Mach-O' not in subprocess.check_output(['file','-b',str(path)],text=True): continue
    for line in subprocess.check_output(['otool','-L',str(path)],text=True).splitlines()[1:]:
        dep=line.strip().split(' (')[0]
        if not dep.startswith(('@','/System/','/usr/lib/')):
            raise SystemExit(f'Unbundled dependency in {path}: {dep}')
