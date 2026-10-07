#!/usr/bin/env python3
"""Record verified native Artix package contents and external runtime libraries."""
import argparse
import datetime
import hashlib
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--work', default='build-artix/0.1.0')
parser.add_argument('--audit', default='build-artix/audit')
parser.add_argument('--output', default='release-assets')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
work, audit, output = (root / value for value in (args.work, args.audit, args.output))
checks = json.loads((audit / 'checks.json').read_text())
if not checks or not all(value is True for value in checks.values()):
    raise SystemExit('Native package acceptance checks must all pass before publication')
package, = output.glob('aeris-*-x86_64.pkg.tar.zst')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


metadata = subprocess.check_output(['bsdtar', '-xOf', str(package), '.PKGINFO'], text=True)
pkgver = next(line.split(' = ', 1)[1] for line in metadata.splitlines() if line.startswith('pkgver = '))
version = pkgver.rsplit('-', 1)[0]
sbom_path = output / 'aeris_artix_x86_64.sbom.json'
sbom = json.loads(sbom_path.read_text())
sbom['packages'] = [item for item in sbom['packages']
                    if not item['SPDXID'].startswith('SPDXRef-Artix-Runtime-')]
sbom['relationships'] = [item for item in sbom['relationships']
                         if not item['relatedSpdxElement'].startswith('SPDXRef-Artix-Runtime-')]
runtime = []
for paragraph in (audit / 'runtime-library-packages.txt').read_text().split('\n\n'):
    fields = dict(line.split(':', 1) for line in paragraph.splitlines() if ':' in line and not line.startswith(' '))
    fields = {key.strip(): value.strip() for key, value in fields.items()}
    if 'Name' not in fields:
        continue
    record = {'name': fields['Name'], 'version': fields['Version'], 'licenses': fields['Licenses']}
    runtime.append(record)
    identifier = 'SPDXRef-Artix-Runtime-' + record['name']
    sbom['packages'].append({
        'name': record['name'], 'versionInfo': record['version'], 'SPDXID': identifier,
        'downloadLocation': 'NOASSERTION', 'filesAnalyzed': False,
        'licenseDeclared': 'NOASSERTION', 'licenseConcluded': 'NOASSERTION',
        'copyrightText': 'NOASSERTION',
        'comment': 'External shared-library package from standard Artix repositories; not bundled. '
                   'Artix license metadata: ' + record['licenses'],
    })
    sbom['relationships'].append({
        'spdxElementId': 'SPDXRef-Aeris-Dependency-Aeris',
        'relationshipType': 'DEPENDS_ON', 'relatedSpdxElement': identifier,
    })
sbom_path.write_text(json.dumps(sbom, indent=2) + '\n')
revision = subprocess.check_output(['git', 'rev-parse', f'v{version}^{{}}'], cwd=root, text=True).strip()
packaging_revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
image, = json.loads((work / 'container-image.json').read_text())
record = {
    'application': 'com.cyberfracture.aeris', 'version': version, 'packageVersion': pkgver,
    'sourceRevision': revision, 'packagingRevision': packaging_revision, 'builtLocally': True,
    'createdAtUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'builder': {'distribution': 'Artix Linux', 'architecture': 'x86_64',
                'containerImageId': image['Id'], 'containerRepoDigests': image.get('RepoDigests', []),
                'repositories': (work / 'repositories.txt').read_text().splitlines(),
                'packages': (work / 'build-packages.txt').read_text().splitlines()},
    'sourceArchiveSha256': digest(work / f'aeris-v{version}-source.tar.gz'),
    'packageFile': package.name, 'packageSha256': digest(package),
    'recipeSha256': digest(output / 'PKGBUILD'),
    'runtimeLibraries': runtime, 'checks': checks,
    'limitations': ['Unsigned local build, no GitHub attestation',
                    'Native package acceptance limited to current Artix x86_64',
                    'Rolling library ABI upgrades may require a package rebuild'],
}
(output / 'build-provenance.artix.json').write_text(json.dumps(record, indent=2) + '\n')
print(f'Recorded {package.name}, {len(runtime)} external runtime packages, and {len(checks)} passed checks')
