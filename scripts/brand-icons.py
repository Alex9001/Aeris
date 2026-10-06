#!/usr/bin/env python3
"""Prepare offline brand assets. Requires pinned Pillow and rsvg-convert for SVGs.

With an archive, rebuild selfh.st PNGs from the pinned upstream tarball.
Without an archive, refresh aliases and supplemental artwork in the existing catalog.
No network access is performed. Normal builds use the prepared PNGs and catalog.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
from PIL import Image

REVISION = '2053b70b283ffed5f2cc1424d1e17d9c554a846d'
ARCHIVE_SHA256 = '13dff5bd9f7132818b31b5262c6d19a8dd7fabd786ff2d899527b6d34f3ddc47'
ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_changed(path, data):
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def prepare_png(data, target):
    image = Image.open(io.BytesIO(data)).convert('RGBA')
    image.thumbnail((128, 128), Image.Resampling.LANCZOS)
    output = io.BytesIO()
    image.save(output, format='PNG', optimize=True)
    write_changed(target, output.getvalue())
    return digest(output.getvalue())


def upstream_catalog(archive_path):
    if digest(archive_path.read_bytes()) != ARCHIVE_SHA256:
        raise ValueError('Archive does not match the pinned upstream SHA-256')
    prefix = 'icons-' + REVISION + '/'
    records = []
    with tarfile.open(archive_path, mode='r|gz') as archive:
        contents = {member.name[len(prefix):]: archive.extractfile(member).read()
                    for member in archive if member.isfile() and
                    (member.name.startswith(prefix + 'png/') or
                     member.name in (prefix + 'index.json', prefix + 'LICENSE'))}
    index = contents['index.json']
    for row in sorted(json.loads(index), key=lambda row: row['Reference']):
        ident = row['Reference']
        source = contents['png/' + ident + '.png']
        checksum = prepare_png(source, ROOT / 'assets/brands' / (ident + '.png'))
        records.append({'id': ident, 'name': row['Name'], 'aliases': [],
                        'sourceSha256': digest(source), 'sha256': checksum})
    write_changed(ROOT / 'licenses/selfhst-icons-CC-BY-4.0.txt', contents['LICENSE'])
    return {'source': 'https://github.com/selfhst/icons', 'revision': REVISION,
            'license': 'CC-BY-4.0', 'archiveSha256': ARCHIVE_SHA256,
            'indexSha256': digest(index), 'icons': records}


def supplement_records():
    folder = ROOT / 'assets/brand-sources'
    records = json.loads((folder / 'catalog.json').read_text())
    for row in records:
        source = folder / row.pop('file')
        data = source.read_bytes()
        if digest(data) != row['sourceSha256']:
            raise ValueError('Supplemental source checksum mismatch: ' + str(source))
        if source.suffix == '.svg':
            data = subprocess.run(['rsvg-convert', '--keep-aspect-ratio', '--width=128',
                                   '--height=128', str(source)], check=True,
                                  capture_output=True).stdout
        row['sha256'] = prepare_png(data, ROOT / 'assets/brands' / (row['id'] + '.png'))
    return records


def refresh(catalog):
    aliases = json.loads((ROOT / 'assets/brand-aliases.json').read_text())
    records = [row for row in catalog['icons'] if 'source' not in row]
    for row in records:
        row['aliases'] = aliases.get(row['id'], [])
    if set(aliases) - {row['id'] for row in records}:
        raise ValueError('Alias refers to an unknown base icon')
    records.extend(supplement_records())
    records.sort(key=lambda row: row['id'])
    if len({row['id'] for row in records}) != len(records):
        raise ValueError('Duplicate brand identifier')
    catalog['licenseScope'] = 'Default applies to selfh.st records; supplemental records override it.'
    catalog['icons'] = records
    write_changed(ROOT / 'assets/brands/catalog.json',
                  (json.dumps(catalog, ensure_ascii=False, indent=2) + '\n').encode())
    files = ['catalog.json'] + [row['id'] + '.png' for row in records]
    resource = '<RCC><qresource prefix="/brands">\n' + ''.join(
        f'<file alias="{name}">brands/{name}</file>\n' for name in files) + '</qresource></RCC>\n'
    write_changed(ROOT / 'assets/brands.qrc', resource.encode())
    print(f'Prepared {len(records)} icons')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path, nargs='?')
    args = parser.parse_args()
    (ROOT / 'assets/brands').mkdir(exist_ok=True)
    catalog = (upstream_catalog(args.archive) if args.archive else
               json.loads((ROOT / 'assets/brands/catalog.json').read_text()))
    refresh(catalog)


if __name__ == '__main__':
    main()
