#!/usr/bin/env python3
"""Collect full notices from the actual dependency installation used for a package."""
import argparse, shutil, subprocess
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('destination'); p.add_argument('--vcpkg'); p.add_argument('--qt'); a=p.parse_args()
root=Path(__file__).resolve().parents[1]; out=Path(a.destination); out.mkdir(parents=True,exist_ok=True)
shutil.copytree(root/'licenses',out,dirs_exist_ok=True)
shutil.copy2(root/'LICENSE',out/'Aeris-MIT.txt')
if a.vcpkg:
    for source in Path(a.vcpkg).glob('share/*/copyright'):
        shutil.copy2(source,out/(source.parent.name+'-copyright.txt'))
if a.qt:
    base=Path(a.qt)
    for source in base.rglob('LICENSES'):
        if source.is_dir(): shutil.copytree(source,out/'Qt',dirs_exist_ok=True)
# Debian copyright files cover runtime and bundled plugin transitive dependencies.
if Path('/var/lib/dpkg/status').exists():
    for source in Path('/usr/share/doc').glob('*/copyright'):
        shutil.copy2(source,out/('debian-'+source.parent.name+'.txt'))
if not (out/'Qt-LGPL-3.0.txt').exists(): raise SystemExit('Missing Qt LGPL text')
