#!/usr/bin/env python3
import hashlib, json, os, re, subprocess, tarfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]; out=root/'release-assets'; out.mkdir(exist_ok=True)
version=re.search(r'project\(Aeris VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text())[1]
ref=os.environ.get('GITHUB_REF','')
if ref.startswith('refs/tags/') and ref!='refs/tags/v'+version: raise SystemExit('Tag and CMake version differ')
expected=['aeris_linux_amd64.AppImage','aeris_linux_arm64.AppImage','aeris_macos_universal.dmg']
expected += [f'aeris_windows_{a}{suffix}' for a in ('amd64','arm64') for suffix in ('.zip','-setup.exe')]
expected += [f'aeris_{p}.sbom.json' for p in ('linux_amd64','linux_arm64','windows_amd64','windows_arm64','macos_universal')]
for name in expected:
    if not (out/name).is_file() or (out/name).stat().st_size<100: raise SystemExit('Missing/empty artifact: '+name)
source=out/f'aeris-v{version}-source.tar.gz'
subprocess.run(['git','archive','--format=tar.gz',f'--prefix=aeris-{version}/','-o',str(source),'HEAD'],check=True,cwd=root)
digest=hashlib.sha256(source.read_bytes()).hexdigest()
recipe=(root/'packaging/aur/PKGBUILD.in').read_text().replace('@VERSION@',version).replace('@SHA256@',digest)
(out/'PKGBUILD').write_text(recipe)
provenance={'application':'com.cyberfracture.aeris','version':version,'revision':os.environ.get('GITHUB_SHA','local'),'run':os.environ.get('GITHUB_SERVER_URL','')+'/'+os.environ.get('GITHUB_REPOSITORY','')+'/actions/runs/'+os.environ.get('GITHUB_RUN_ID',''),'publication':ref.startswith('refs/tags/'),'vcpkgBaseline':json.loads((root/'vcpkg.json').read_text())['builtin-baseline']}
(out/'build-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
checksums=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS')
(out/'SHA256SUMS').write_text(checksums)
