#!/usr/bin/env python3
"""Run native Aeris release builds in BuildLab and collect verified packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGETS = ('windows-x64', 'macos-x64')
PLATFORMS = {'windows-x64': 'windows_amd64', 'macos-x64': 'macos_x64'}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def required_artifacts(target):
    platform = PLATFORMS[target]
    names = [f'aeris_{platform}.sbom.json', f'build-provenance.local.{platform}.json',
             f'ctest.{platform}.xml']
    if target == 'windows-x64':
        names += ['aeris_windows_amd64.zip', 'aeris_windows_amd64-setup.exe']
    else:
        names += ['aeris_macos_x64.dmg']
    return names


def collect(result, target, output):
    """Promote only complete, successful runs; retain failed runs separately."""
    source = Path(result['artifacts_directory']) / 'artifacts' if result.get('artifacts_directory') else None
    success = result.get('state') == 'succeeded' and result.get('exit_code') == 0
    if success:
        missing = [name for name in required_artifacts(target)
                   if source is None or not (source / name).is_file() or (source / name).stat().st_size == 0]
        if missing:
            result = dict(result, state='failed', exit_code=1,
                          error='Missing artifacts: ' + ', '.join(missing))
            success = False
        else:
            record = json.loads((source / f'build-provenance.local.{PLATFORMS[target]}.json')
                                .read_text(encoding='utf-8'))
            if not record.get('verified') or record.get('target') != target:
                result = dict(result, state='failed', exit_code=1, error='Guest acceptance failed')
                success = False
            elif any(digest(source / name) != expected for name, expected in record.get('artifacts', {}).items()):
                result = dict(result, state='failed', exit_code=1, error='Artifact checksum mismatch')
                success = False
    destination = output if success else output / 'failed' / target / result.get('run_id', 'preflight')
    destination.mkdir(parents=True, exist_ok=True)
    if source is not None and source.is_dir():
        for path in source.iterdir():
            if path.is_file() and not path.is_symlink():
                temporary = destination / (path.name + '.tmp')
                shutil.copy2(path, temporary)
                temporary.replace(destination / path.name)
    platform = PLATFORMS[target]
    provenance = destination / f'build-provenance.local.{platform}.json'
    if provenance.is_file():
        record = json.loads(provenance.read_text(encoding='utf-8'))
        record['verified'] = success
        record['buildLab'] = {key: result.get(key) for key in
                              ('run_id', 'target', 'source', 'source_sha256', 'resources', 'log')}
        write_json(provenance, record)
    return dict(result, verified=success, collected_directory=str(destination))


def checksums(output):
    paths = sorted(p for p in output.iterdir()
                   if p.is_file() and p.name != 'SHA256SUMS' and not p.name.endswith('.tmp'))
    temporary = output / 'SHA256SUMS.tmp'
    temporary.write_text(''.join(f'{digest(p)}  {p.name}\n' for p in paths), encoding='utf-8')
    temporary.replace(output / 'SHA256SUMS')


def call_lab(buildlab, argv):
    process = subprocess.run([str(buildlab), *argv], cwd=buildlab.parent,
                             stdout=subprocess.PIPE, text=True, encoding='utf-8', errors='replace')
    try:
        value = json.loads(process.stdout)
    except json.JSONDecodeError:
        value = {'state': 'failed', 'exit_code': process.returncode or 1,
                 'error': 'BuildLab did not return JSON', 'output': process.stdout[-4000:]}
    return process.returncode, value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=(*TARGETS, 'all'), default='all')
    parser.add_argument('--buildlab', type=Path, default=Path(os.environ.get(
        'AERIS_BUILDLAB', '/run/media/user/edat/VMs/buildlab')))
    parser.add_argument('--output', type=Path, default=ROOT / 'release-assets')
    parser.add_argument('--timeout', type=int, default=21600, help='Guest timeout in seconds (default: 6 hours)')
    parser.add_argument('--wait-lock', type=int, default=600)
    parser.add_argument('--wait-memory', type=int, default=600)
    parser.add_argument('--keep', action='store_true', help='Keep one target VM for inspection; reset it before another build')
    args = parser.parse_args()
    if args.keep and args.target == 'all':
        parser.error('--keep requires a single target')
    if args.timeout < 1 or min(args.wait_lock, args.wait_memory) < 0:
        parser.error('Timeout must be positive; wait limits must be nonnegative')
    buildlab = args.buildlab.resolve()
    if not buildlab.is_file():
        parser.error(f'BuildLab executable not found: {buildlab}')
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        relative = output.relative_to(ROOT)
        if subprocess.run(['git', 'check-ignore', '--quiet', str(relative) + '/'], cwd=ROOT).returncode:
            parser.error('Output inside the checkout must be gitignored (use release-assets/)')
    output.mkdir(parents=True, exist_ok=True)
    doctor_code, doctor = call_lab(buildlab, ['doctor'])
    targets_code, inventory = call_lab(buildlab, ['targets'])
    status_code, status = call_lab(buildlab, ['status'])
    readiness = {item['name']: item for item in inventory.get('targets', [])}
    selected = TARGETS if args.target == 'all' else (args.target,)
    summary_path = output / 'local-builds.json'
    summary = json.loads(summary_path.read_text()) if summary_path.exists() else {'targets': {}}
    for target in selected:
        print(f'Aeris local build: {target}', file=sys.stderr, flush=True)
        if doctor_code or targets_code or status_code or not doctor.get('ok'):
            result = {'state': 'failed', 'exit_code': 1, 'error': 'BuildLab preflight failed'}
        elif not readiness.get(target, {}).get('ready'):
            result = {'state': 'failed', 'exit_code': 1, 'error': f'{target} is not verified and ready'}
        else:
            code, result = call_lab(buildlab, ['run', '--target', target, '--repo', str(ROOT),
                '--timeout', str(args.timeout), '--wait-lock', str(args.wait_lock),
                '--wait-memory', str(args.wait_memory), *(['--keep'] if args.keep else []), '--', 'python3',
                'scripts/build-native.py', '--target', target])
            if code and result.get('exit_code') == 0:
                result = dict(result, state='failed', exit_code=code)
        try:
            result = collect(result, target, output)
        except (OSError, ValueError) as error:
            result = dict(result, state='failed', verified=False, error=str(error))
        summary['targets'][target] = result
        write_json(summary_path, summary)
        checksums(output)
        print(f'{target}: {"verified" if result.get("verified") else "FAILED"}; '
              f'log: {result.get("log", result.get("error"))}', file=sys.stderr, flush=True)
    print(json.dumps(summary, indent=2))
    return 0 if all(summary['targets'][target].get('verified') for target in selected) else 1


if __name__ == '__main__':
    sys.exit(main())
