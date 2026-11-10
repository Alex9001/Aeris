"""Release collection must never promote failed or corrupted guest artifacts."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


def load_script(name):
    path = Path(__file__).resolve().parents[1] / 'scripts' / (name + '.py')
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


local = load_script('build-local')
native = load_script('build-native')
mac_audit = load_script('audit-macos')


class CollectionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'guest/artifacts'
        self.source.mkdir(parents=True)
        self.output = self.root / 'release'
        self.output.mkdir()
        self.target = 'macos-x64'
        self.provenance = 'build-provenance.local.macos_x64.json'
        for name in local.required_artifacts(self.target):
            (self.source / name).write_text('synthetic ' + name)
        record = {'verified': True, 'target': self.target,
                  'artifacts': {p.name: local.digest(p) for p in self.source.iterdir()
                                if p.name != self.provenance}}
        (self.source / self.provenance).write_text(json.dumps(record))
        self.result = {'state': 'succeeded', 'exit_code': 0, 'run_id': 'synthetic-run',
                       'artifacts_directory': str(self.source.parent), 'source_sha256': 'snapshot-hash'}

    def test_complete_run_promotes_and_records_source(self):
        (self.output / 'existing.AppImage').write_text('existing Linux package')
        result = local.collect(self.result, self.target, self.output)
        self.assertTrue(result['verified'])
        self.assertEqual((self.output / 'existing.AppImage').read_text(), 'existing Linux package')
        record = json.loads((self.output / self.provenance).read_text())
        self.assertEqual(record['buildLab']['source_sha256'], 'snapshot-hash')

    def test_failed_run_preserves_previous_package(self):
        (self.output / 'aeris_macos_x64.dmg').write_text('previous verified package')
        result = local.collect(dict(self.result, state='failed', exit_code=1), self.target, self.output)
        self.assertFalse(result['verified'])
        self.assertEqual((self.output / 'aeris_macos_x64.dmg').read_text(), 'previous verified package')
        self.assertTrue((Path(result['collected_directory']) / 'aeris_macos_x64.dmg').is_file())

    def test_missing_package_is_not_promoted(self):
        (self.source / 'aeris_macos_x64.dmg').unlink()
        result = local.collect(self.result, self.target, self.output)
        self.assertFalse(result['verified'])
        self.assertIn('Missing artifacts', result['error'])
        self.assertFalse((self.output / self.provenance).exists())

    def test_corrupted_package_is_not_promoted(self):
        (self.source / 'aeris_macos_x64.dmg').write_text('corrupted')
        result = local.collect(self.result, self.target, self.output)
        self.assertFalse(result['verified'])
        self.assertEqual(result['error'], 'Artifact checksum mismatch')

    def test_preflight_does_not_collect_an_unrelated_directory(self):
        result = local.collect({'state': 'failed', 'exit_code': 1}, self.target, self.output)
        self.assertFalse(result['verified'])
        self.assertFalse((self.output / self.provenance).exists())

    def test_checksums_cover_top_level_files_only(self):
        (self.output / 'good.zip').write_bytes(b'good')
        failed = self.output / 'failed'
        failed.mkdir()
        (failed / 'bad.zip').write_bytes(b'bad')
        local.checksums(self.output)
        self.assertEqual((self.output / 'SHA256SUMS').read_text(),
                         hashlib.sha256(b'good').hexdigest() + '  good.zip\n')
        local.checksums(self.output)
        self.assertNotIn('SHA256SUMS', (self.output / 'SHA256SUMS').read_text())

    def test_all_runs_the_second_target_after_the_first_fails(self):
        buildlab = self.root / 'buildlab'
        buildlab.touch()
        calls = []
        def fake_lab(executable, argv):
            calls.append(argv)
            if argv[0] == 'doctor': return 0, {'ok': True}
            if argv[0] == 'targets':
                return 0, {'targets': [{'name': t, 'ready': True} for t in local.TARGETS]}
            if argv[0] == 'status': return 0, {}
            target = argv[argv.index('--target') + 1]
            if target == 'windows-x64': return 1, {'state': 'failed', 'exit_code': 1}
            return 0, self.result
        argv = ['build-local.py', '--target', 'all', '--buildlab', str(buildlab), '--output', str(self.output)]
        with patch.object(sys, 'argv', argv), patch.object(local, 'call_lab', side_effect=fake_lab):
            with patch.object(sys, 'stdout', io.StringIO()), patch.object(sys, 'stderr', io.StringIO()):
                self.assertEqual(local.main(), 1)
        summary = json.loads((self.output / 'local-builds.json').read_text())
        self.assertFalse(summary['targets']['windows-x64']['verified'])
        self.assertTrue(summary['targets']['macos-x64']['verified'])
        self.assertEqual(sum(argv[0] == 'run' for argv in calls), 2)

    def test_dependency_feature_paragraph_retains_upstream_version(self):
        install = self.root / 'installed'
        (install / 'vcpkg').mkdir(parents=True)
        (install / 'vcpkg/status').write_text(
            'Package: nu-book-zxing-cpp\nVersion: 2.3.0\n\n'
            'Package: nu-book-zxing-cpp\nFeature: example\n\n')
        metadata = self.root / 'dependencies.json'
        metadata.write_text(json.dumps({'zxing-cpp': {'version': ''}}))
        script = Path(__file__).resolve().parents[1] / 'scripts/normalize-dependencies.py'
        subprocess.run([sys.executable, script, metadata, install / 'x64-windows'], check=True)
        self.assertEqual(json.loads(metadata.read_text())['zxing-cpp']['version'], '2.3.0')


class DownloadTests(unittest.TestCase):
    def test_checksum_failure_does_not_leave_an_executable_archive(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'tool.zip'
            with patch.object(native.urllib.request, 'urlopen', side_effect=lambda *a, **k: io.BytesIO(b'bad')):
                with patch.object(native.time, 'sleep'):
                    with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                        native.download('https://example.test/tool.zip', path, 'invalid')
            self.assertFalse(path.exists())
            self.assertFalse(path.with_suffix('.zip.partial').exists())

    def test_verified_cache_avoids_a_download(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'tool.zip'
            path.write_bytes(b'verified')
            with patch.object(native.urllib.request, 'urlopen') as request:
                native.download('https://example.test/tool.zip', path, native.digest(path))
            request.assert_not_called()


class MacBundleTests(unittest.TestCase):
    def test_rpath_dependency_must_exist_inside_bundle(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            app = root / 'Aeris.app'
            executable = app / 'Contents/MacOS/aeris'
            library = app / 'Contents/Frameworks/libtest.dylib'
            library.parent.mkdir(parents=True)
            library.touch()
            external = root / 'SDK'
            external.mkdir()
            (external / 'libexternal.dylib').touch()
            search = [external, library.parent]
            self.assertTrue(mac_audit.bundled_dependency(
                '@rpath/libtest.dylib', executable, executable, app, search))
            self.assertFalse(mac_audit.bundled_dependency(
                '@rpath/libexternal.dylib', executable, executable, app, search))
            self.assertFalse(mac_audit.bundled_dependency(
                '@rpath/libmissing.dylib', executable, executable, app, search))
            self.assertFalse(mac_audit.bundled_dependency(
                str(library), executable, executable, app, search))
            self.assertIsNone(mac_audit.expand(str(library.parent), executable, executable))

    def test_loader_path_cannot_escape_bundle_through_symlink(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            app = root / 'Aeris.app'
            executable = app / 'Contents/MacOS/aeris'
            executable.parent.mkdir(parents=True)
            external = root / 'external.dylib'
            external.touch()
            (executable.parent / 'libtest.dylib').symlink_to(external)
            self.assertFalse(mac_audit.bundled_dependency(
                '@loader_path/libtest.dylib', executable, executable, app, []))
            self.assertTrue(mac_audit.bundled_dependency(
                '/usr/lib/libSystem.B.dylib', executable, executable, app, []))


if __name__ == '__main__':
    unittest.main()
