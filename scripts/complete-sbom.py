#!/usr/bin/env python3
"""Add build-system dependency evidence to Syft's file inventory (Syft misses Qt dylibs)."""
import argparse, hashlib, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('sbom'); p.add_argument('--vcpkg'); p.add_argument('--build',default='build-release'); a=p.parse_args()
f=Path(a.sbom); doc=json.loads(f.read_text()); metadata=json.loads((Path(a.build)/'dependencies.json').read_text())
if a.vcpkg:
    status=Path(a.vcpkg).parent/'vcpkg/status'
    if status.exists():
        for paragraph in status.read_text().split('\n\n'):
            fields=dict(line.split(': ',1) for line in paragraph.splitlines() if ': ' in line)
            if fields.get('Package') in metadata: metadata[fields['Package']]['version']=fields.get('Version','NOASSERTION')
packages=doc.setdefault('packages',[]); relationships=doc.setdefault('relationships',[])
for name,info in metadata.items():
    ident='SPDXRef-Aeris-Dependency-'+name
    package={'name':name,'SPDXID':ident,'versionInfo':info.get('version') or 'NOASSERTION','downloadLocation':info['source'],'filesAnalyzed':False,'licenseConcluded':'NOASSERTION','licenseDeclared':info['license'],'copyrightText':'NOASSERTION','comment':info.get('comment','Direct build dependency, recorded by CMake and the pinned dependency manager.')}
    packages.append(package)
    relationships.append({'spdxElementId':doc.get('SPDXID','SPDXRef-DOCUMENT'),'relationshipType':'DESCRIBES','relatedSpdxElement':ident})
f.write_text(json.dumps(doc,indent=2)+'\n')
