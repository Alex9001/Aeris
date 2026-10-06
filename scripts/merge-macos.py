#!/usr/bin/env python3
"""Merge native app bundles, refusing missing or architecture-incomplete binaries."""
import shutil, subprocess, sys, tempfile
from pathlib import Path
left,right,out=map(Path,sys.argv[1:])
shutil.copytree(left,out,symlinks=True)
for path in list(out.rglob('*')):
    if path.name=='_CodeSignature' and path.is_dir(): shutil.rmtree(path)
for path in out.rglob('*'):
    if not path.is_file() or path.is_symlink(): continue
    rel=path.relative_to(out); other=right/rel
    if not other.exists(): raise SystemExit(f'Missing arm64 file: {rel}')
    if 'Mach-O' not in subprocess.check_output(['file','-b',str(path)],text=True): continue
    with tempfile.TemporaryDirectory() as d:
        slices=[]
        for arch,source in [('x86_64',left/rel),('arm64',other)]:
            arches=subprocess.check_output(['lipo','-archs',str(source)],text=True).split()
            if arch not in arches: raise SystemExit(f'Missing {arch} slice: {rel}')
            target=Path(d)/arch
            if len(arches)==1: shutil.copy2(source,target)
            else: subprocess.run(['lipo',str(source),'-thin',arch,'-output',str(target)],check=True)
            slices.append(str(target))
        subprocess.run(['lipo','-create',*slices,'-output',str(path)],check=True)
    # Ad-hoc signatures are required to execute arm64 code; no developer certificate/notarization.
    subprocess.run(['codesign','--force','--sign','-',str(path)],check=True)
subprocess.run(['codesign','--force','--deep','--sign','-',str(out)],check=True)
