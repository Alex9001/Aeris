#!/usr/bin/env python3
"""Prepare dependencies, build, test and package Aeris inside a native guest."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
QT_VERSION = '6.10.2'
AQT_VERSION = '3.3.0'
SYFT_VERSION = '1.51.0'
NSIS_VERSION = '3.13'
SYFT_HASHES = {
    'windows': 'fc5ffaeffb993576ece9c791da5a688fb2c8969a1479bbfe58583672c64da336',
    'darwin': 'cddf9a044145caf0a1a3194d00d1dd51a1666f4814f2919cdb4768a0c062ad95',
}
NSIS_HASH = 'ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'
AUTOTOOLS = [
    ('m4', '1.4.20', 'e236ea3a1ccf5f6c270b1c4bb60726f371fa49459a8eaaebc90b216b328daf2b'),
    ('autoconf', '2.72', 'ba885c1319578d6c94d46e9b0dceb4014caafe2490e437a0dbca3f270a223f5a'),
    ('automake', '1.17', '8920c1fc411e13b90bf704ef9db6f29d540e76d232cb3b2c9f4dc4cc599bd990'),
    ('libtool', '2.5.4', 'f81f5860666b0bc7d84baddefa60d1cb9fa6fceb2398cc3baca6afaa60266675'),
    ('autoconf-archive', '2024.10.16', '7bcd5d001916f3a50ed7436f4f700e3d2b1bade3ed803219c592d62502a57363'),
]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def download(url, path, expected):
    if path.is_file() and digest(path) == expected:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.partial')
    for attempt in range(3):
        try:
            print('Downloading ' + url, flush=True)
            request = urllib.request.Request(url, headers={'User-Agent': 'Aeris-local-build/0.1'})
            with urllib.request.urlopen(request, timeout=120) as response, temporary.open('wb') as stream:
                shutil.copyfileobj(response, stream)
            if digest(temporary) != expected:
                raise ValueError('Download checksum mismatch: ' + url)
            temporary.replace(path)
            return
        except (OSError, ValueError):
            temporary.unlink(missing_ok=True)
            if attempt == 2:
                raise
            time.sleep(2)


class Build:
    def __init__(self, target, jobs):
        self.target, self.jobs = target, jobs
        self.windows = target == 'windows-x64'
        self.name = 'windows_amd64' if self.windows else 'macos_x64'
        self.triplet = 'x64-windows' if self.windows else 'x64-osx-dynamic'
        self.cache = Path(os.environ.get('BUILDLAB_CACHE', ROOT / 'build-deps/cache')) / 'aeris'
        self.out = Path(os.environ.get('BUILDLAB_ARTIFACTS', ROOT / 'artifacts'))
        self.out.mkdir(parents=True, exist_ok=True)
        self.env = dict(os.environ)
        self.env.update(VCPKG_DISABLE_METRICS='1', VCPKG_MAX_CONCURRENCY=str(jobs),
                        CMAKE_BUILD_PARALLEL_LEVEL=str(jobs))
        if not self.windows:
            self.env.update(CC='/usr/bin/clang', CXX='/usr/bin/clang++')
        baseline = json.loads((ROOT / 'vcpkg.json').read_text())['builtin-baseline']
        self.record = {
            'application': 'com.cyberfracture.aeris', 'target': target, 'verified': False,
            'version': re.search(r'project\(Aeris VERSION ([0-9.]+)',
                                 (ROOT / 'CMakeLists.txt').read_text())[1],
            'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'platform': platform.platform(), 'architecture': platform.machine(), 'compilerJobs': jobs,
            'started': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'vcpkgBaseline': baseline, 'vcpkgTriplet': self.triplet,
            'vcpkgHostTriplet': self.triplet, 'qtVersion': QT_VERSION, 'aqtVersion': AQT_VERSION,
            'syftVersion': SYFT_VERSION, 'checks': [],
            'limitations': ['Unsigned local build; no GitHub attestation', 'x64 runtime acceptance only'],
        }

    def run(self, argv, check=None, cwd=ROOT):
        argv = list(map(str, argv))
        resolved = shutil.which(argv[0], path=self.env.get('PATH'))
        if resolved:
            argv[0] = resolved
        print('+ ' + repr(argv), flush=True)
        started = time.monotonic()
        process = subprocess.run(argv, cwd=cwd, env=self.env)
        if check:
            self.record['checks'].append({'name': check, 'exit_code': process.returncode,
                                          'seconds': round(time.monotonic() - started, 2)})
        process.check_returncode()

    def tools(self):
        tools = self.cache / 'tools'
        suffix = '.exe' if self.windows else ''
        syft = tools / f'syft-{SYFT_VERSION}'
        if not (syft / ('syft' + suffix)).is_file():
            host = 'windows' if self.windows else 'darwin'
            extension = 'zip' if self.windows else 'tar.gz'
            name = f'syft_{SYFT_VERSION}_{host}_amd64.{extension}'
            archive = self.cache / 'downloads' / name
            download(f'https://github.com/anchore/syft/releases/download/v{SYFT_VERSION}/{name}',
                     archive, SYFT_HASHES[host])
            syft.mkdir(parents=True, exist_ok=True)
            if self.windows:
                with zipfile.ZipFile(archive) as package:
                    package.extractall(syft)
            else:
                with tarfile.open(archive) as package:
                    package.extractall(syft, filter='data')
        self.env['PATH'] = str(syft) + os.pathsep + self.env.get('PATH', '')
        if self.windows:
            nsis = tools / f'nsis-{NSIS_VERSION}'
            if not (nsis / 'makensis.exe').is_file():
                archive = self.cache / 'downloads' / f'nsis-{NSIS_VERSION}.zip'
                download(f'https://downloads.sourceforge.net/project/nsis/NSIS%203/{NSIS_VERSION}/'
                         f'nsis-{NSIS_VERSION}.zip', archive, NSIS_HASH)
                with zipfile.ZipFile(archive) as package:
                    package.extractall(tools)
            self.env['AERIS_MAKENSIS'] = str(nsis / 'makensis.exe')
            self.record['nsisVersion'] = NSIS_VERSION

    def qt(self):
        tools = self.cache / 'tools'
        venv = tools / f'aqt-{AQT_VERSION}-py{sys.version_info.major}{sys.version_info.minor}'
        python = venv / ('Scripts/python.exe' if self.windows else 'bin/python')
        if not python.is_file():
            self.run([sys.executable, '-m', 'venv', venv])
        if not (venv / '.complete').is_file():
            self.run([python, '-m', 'pip', 'install', f'aqtinstall=={AQT_VERSION}'])
            (venv / '.complete').touch()
        sdk = self.cache / f'qt-{QT_VERSION}-{self.target}'
        qt = sdk / QT_VERSION / ('msvc2022_64' if self.windows else 'macos')
        marker = sdk / '.complete'
        if not marker.is_file():
            archives = ['qtbase', 'qttools', 'qtsvg']
            if self.windows:
                archives += ['d3dcompiler_47', 'opengl32sw']
            self.run([python, '-m', 'aqt', 'install-qt',
                      'windows' if self.windows else 'mac', 'desktop', QT_VERSION,
                      'win64_msvc2022_64' if self.windows else 'clang_64', '--outputdir', sdk,
                      '--keep', '--archive-dest', self.cache / 'downloads/qt',
                      '--archives', *archives])
            if not (qt / 'lib/cmake/Qt6/Qt6Config.cmake').is_file():
                raise RuntimeError('Qt installation incomplete: ' + str(qt))
            marker.touch()
        self.env['QT_ROOT_DIR'] = str(qt)
        self.env['CMAKE_PREFIX_PATH'] = str(qt)
        self.env['PATH'] = str(qt / 'bin') + os.pathsep + self.env['PATH']
        self.qt_root = qt

    def autotools(self):
        if self.windows:
            return
        # MacPorts' base image lacks these vcpkg prerequisites. Keep them on
        # the cache volume so disposable guests do not need administrative installs.
        prefix = self.cache / 'tools/autotools'
        self.env['PATH'] = str(prefix / 'bin') + os.pathsep + self.env['PATH']
        self.env['ACLOCAL_PATH'] = str(prefix / 'share/aclocal')
        self.env['LIBTOOLIZE'] = str(prefix / 'bin/glibtoolize')
        self.record['autotools'] = {name: version for name, version, _ in AUTOTOOLS}
        for name, version, sha in AUTOTOOLS:
            marker = prefix / f'.{name}-{version}.complete'
            if marker.is_file():
                continue
            archive = self.cache / 'downloads' / f'{name}-{version}.tar.xz'
            download(f'https://ftp.gnu.org/gnu/{name}/{archive.name}', archive, sha)
            work = self.cache / 'autotools-buildtrees'
            work.mkdir(parents=True, exist_ok=True)
            source = work / f'{name}-{version}'
            if source.exists():
                shutil.rmtree(source)
            with tarfile.open(archive) as package:
                package.extractall(work, filter='data')
            configure = ['bash', source / 'configure', f'--prefix={prefix}']
            if name == 'libtool':
                configure += ['--program-prefix=g']
            self.run(configure, cwd=source)
            self.run(['make', f'-j{self.jobs}'], cwd=source)
            self.run(['make', 'install'], f'{name}-bootstrap', cwd=source)
            marker.touch()

    def dependencies(self):
        baseline = self.record['vcpkgBaseline']
        vcpkg = self.cache / ('vcpkg-' + baseline)
        binary = vcpkg / ('vcpkg.exe' if self.windows else 'vcpkg')
        if not (vcpkg / '.git').exists():
            self.run(['git', 'init', vcpkg])
            self.run(['git', '-C', vcpkg, 'remote', 'add', 'origin', 'https://github.com/microsoft/vcpkg.git'])
        if not binary.is_file():
            self.run(['git', '-C', vcpkg, 'fetch', '--depth', '1', 'origin', baseline])
            self.run(['git', '-C', vcpkg, 'checkout', '--detach', baseline])
            bootstrap = vcpkg / ('bootstrap-vcpkg.bat' if self.windows else 'bootstrap-vcpkg.sh')
            self.run((['cmd.exe', '/d', '/c', bootstrap] if self.windows else ['bash', bootstrap])
                     + ['-disableMetrics'])
        actual = subprocess.check_output(['git', '-C', vcpkg, 'rev-parse', 'HEAD'], text=True).strip()
        if actual != baseline:
            raise RuntimeError('Cached vcpkg checkout does not match the pinned baseline')
        self.env['VCPKG_ROOT'] = str(vcpkg)
        for name in ['downloads/vcpkg', 'vcpkg-binary', 'vcpkg-buildtrees', 'vcpkg-packages']:
            (self.cache / name).mkdir(parents=True, exist_ok=True)
        self.env['VCPKG_DOWNLOADS'] = str(self.cache / 'downloads/vcpkg')
        self.env['VCPKG_BINARY_SOURCES'] = f'clear;files,{self.cache / "vcpkg-binary"},readwrite'
        # These are native builds: keep host tools on the same triplet. Using
        # x64-osx as host for x64-osx-dynamic needlessly builds protobuf twice.
        self.run([binary, 'install', '--triplet', self.triplet,
                  f'--host-triplet={self.triplet}',
                  f'--x-manifest-root={ROOT}', f'--x-install-root={ROOT / "vcpkg_installed"}',
                  f'--overlay-triplets={ROOT / "packaging/triplets"}',
                  f'--x-buildtrees-root={self.cache / "vcpkg-buildtrees"}',
                  f'--x-packages-root={self.cache / "vcpkg-packages"}'], 'dependency-install')
        self.toolchain = vcpkg / 'scripts/buildsystems/vcpkg.cmake'
        args = [sys.executable, ROOT / 'scripts/build-keychain.py', '--prefix', 'build-deps/install']
        if not self.windows:
            args += ['--arch', 'x86_64']
        self.run(args, 'qtkeychain-build')
        self.record['qtKeychainRevision'] = subprocess.check_output(
            ['git', '-C', ROOT / 'build-deps/qtkeychain-source', 'rev-parse', 'HEAD'], text=True).strip()

    def build(self):
        prefix = str(ROOT / 'build-deps/install') + ';' + str(self.qt_root)
        args = ['cmake', '-S', '.', '-B', 'build-release', '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_TOOLCHAIN_FILE={self.toolchain}',
                f'-DVCPKG_TARGET_TRIPLET={self.triplet}',
                f'-DVCPKG_HOST_TRIPLET={self.triplet}', '-DVCPKG_MANIFEST_INSTALL=OFF',
                f'-DVCPKG_INSTALLED_DIR={ROOT / "vcpkg_installed"}', f'-DCMAKE_PREFIX_PATH={prefix}']
        if not self.windows:
            args += ['-DCMAKE_OSX_ARCHITECTURES=x86_64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0']
        self.run(args, 'configure')
        self.run(['cmake', '--build', 'build-release', '--parallel', self.jobs], 'native-build')
        if self.windows:
            self.env['PATH'] = os.pathsep.join([str(ROOT / 'vcpkg_installed' / self.triplet / 'bin'),
                str(ROOT / 'build-deps/install/bin'), self.env['PATH']])
        self.run(['ctest', '--test-dir', 'build-release', '--output-on-failure', '--output-junit',
                  self.out / f'ctest.{self.name}.xml'], 'native-ctest')
        self.run([ROOT / ('build-release/keychain_probe.exe' if self.windows
                          else 'build-release/keychain_probe')], 'native-keychain-vault')

    def package(self):
        if self.windows:
            self.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                      'scripts/package-windows.ps1', '-Arch', 'amd64', '-Triplet', self.triplet,
                      '-Makensis', self.env['AERIS_MAKENSIS']], 'windows-package-acceptance')
        else:
            self.run(['bash', 'scripts/package-macos.sh', self.triplet, '--dmg'], 'macos-package-acceptance')

    def execute(self):
        try:
            self.tools()
            self.qt()
            self.autotools()
            self.dependencies()
            self.build()
            self.package()
            self.record['dependencies'] = json.loads((ROOT / 'build-release/dependencies.json').read_text())
            self.record['verified'] = True
        except Exception as error:
            self.record['error'] = str(error)
            raise
        finally:
            assets = ROOT / 'release-assets'
            if assets.is_dir():
                for path in assets.iterdir():
                    if path.is_file():
                        shutil.copy2(path, self.out / path.name)
            self.record['finished'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            self.record['artifacts'] = {p.name: digest(p) for p in self.out.iterdir() if p.is_file()}
            (self.out / f'build-provenance.local.{self.name}.json').write_text(
                json.dumps(self.record, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True, choices=('windows-x64', 'macos-x64'))
    parser.add_argument('--jobs', type=int, help='Compiler jobs (default: Windows 2, macOS 1)')
    args = parser.parse_args()
    expected = 'win32' if args.target == 'windows-x64' else 'darwin'
    if sys.platform != expected or platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('Run this command on the matching native x64 builder')
    jobs = args.jobs if args.jobs is not None else (2 if args.target == 'windows-x64' else 1)
    if jobs < 1:
        parser.error('--jobs must be positive')
    os.chdir(ROOT)
    Build(args.target, jobs).execute()


if __name__ == '__main__':
    main()
