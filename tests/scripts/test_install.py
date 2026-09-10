"""Filesystem regression tests; all installation prefixes are temporary."""
import importlib.util
import json
import runpy
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('installlib', ROOT / 'scripts/installlib.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix = self.root / 'prefix with spaces'
        self.stage = self.root / 'stage'
        self.put(self.stage, 'include/alib5/current.h', 'v1')
        self.put(self.stage, 'lib/libaaaa0ggmcLib.so', 'binary')

    def put(self, root, rel, text):
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
        return p

    def install(self, **kwargs):
        installer.apply(self.prefix, self.stage, 'alib5', **kwargs)

    def test_upgrade_removes_stale_and_preserves_other_library(self):
        self.put(self.stage, 'include/alib5/old.h', 'old')
        self.install()
        other = self.put(self.prefix, 'lib/libother.so', 'keep')
        (self.stage / 'include/alib5/old.h').unlink()
        self.put(self.stage, 'include/alib5/current.h', 'v2')
        self.install()
        self.assertFalse((self.prefix / 'include/alib5/old.h').exists())
        self.assertEqual(other.read_text(), 'keep')
        self.assertEqual((self.prefix / 'include/alib5/current.h').read_text(), 'v2')
        backups = list((self.prefix / '.ul-install/backups').iterdir())
        self.install()
        self.assertEqual(len(list((self.prefix / '.ul-install/backups').iterdir())), len(backups))

    def test_migrate_backup_uninstall(self):
        self.put(self.prefix, 'include/alib5/old.h', 'legacy')
        self.put(self.prefix, 'lib/libaaaa0ggmcLib.so', 'old binary')
        self.install(adopt=True)
        self.assertFalse((self.prefix / 'include/alib5/old.h').exists())
        backup = next((self.prefix / '.ul-install/backups').iterdir())
        self.assertEqual((backup / 'include/alib5/old.h').read_text(), 'legacy')
        installer.apply(self.prefix, None, 'alib5')
        self.assertFalse((self.prefix / 'lib/libaaaa0ggmcLib.so').exists())
        self.assertFalse((self.prefix / '.ul-install/alib5.json').exists())

    def test_modified_stale_file_is_not_deleted(self):
        self.install()
        p = self.put(self.prefix, 'include/alib5/current.h', 'user edit')
        with self.assertRaisesRegex(RuntimeError, 'modified'):
            installer.apply(self.prefix, None, 'alib5')
        self.assertEqual(p.read_text(), 'user edit')
        with self.assertRaisesRegex(RuntimeError, 'first'):
            self.install(adopt=True)

    def test_unowned_collision_and_dry_run(self):
        self.install(dry_run=True)
        self.assertFalse(self.prefix.exists())
        self.put(self.prefix, 'lib/libaaaa0ggmcLib.so', 'unmanaged')
        with self.assertRaisesRegex(RuntimeError, 'Unmanaged'):
            self.install()

    def test_symlink_and_manifest_traversal_rejected(self):
        outside = self.root / 'outside'
        outside.mkdir()
        self.prefix.mkdir()
        (self.prefix / 'include').symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            self.install()
        (self.prefix / 'include').unlink()
        self.put(self.prefix, '.ul-install/alib5.json', json.dumps({
            'schema': 1, 'name': 'alib5', 'files': {'../../outside': 'fake'}
        }))
        with self.assertRaisesRegex(RuntimeError, 'Unsafe'):
            self.install()

    def test_copy_failure_rolls_back(self):
        self.install()
        original_manifest = (self.prefix / '.ul-install/alib5.json').read_bytes()
        self.put(self.stage, 'include/alib5/current.h', 'v2')
        self.put(self.stage, 'lib/libaaaa0ggmcLib.so', 'new binary')
        real_copy = installer.atomic_copy
        failed = False

        def fail_once(src, dst):
            nonlocal failed
            if src == self.stage / 'lib/libaaaa0ggmcLib.so' and not failed:
                failed = True
                raise OSError('simulated disk error')
            real_copy(src, dst)

        with patch.object(installer, 'atomic_copy', side_effect=fail_once):
            with self.assertRaises(OSError):
                self.install()
        self.assertEqual((self.prefix / 'include/alib5/current.h').read_text(), 'v1')
        self.assertEqual((self.prefix / 'lib/libaaaa0ggmcLib.so').read_text(), 'binary')
        self.assertEqual((self.prefix / '.ul-install/alib5.json').read_bytes(), original_manifest)

    def alib6_stage(self):
        stage = self.root / 'alib6-stage'
        self.put(stage, 'include/alib6/main.cppm', 'export module alib6;')
        self.put(stage, 'lib/libaaaa0ggmcLib6.so', 'alib6 binary')
        self.put(stage, 'share/alib6/modules/main/main.cppm', 'export module alib6;')
        self.put(stage, 'share/alib6/modules/main/main.cppm.meta-info', '{}')
        self.put(stage, 'share/alib6/repository/packages/a/alib6/xmake.lua', 'package("alib6")')
        return stage

    def test_alib6_migration_and_uninstall_do_not_touch_other_modules(self):
        stage = self.alib6_stage()
        self.put(self.prefix, 'modules/old/old.cppm', 'old interface')
        self.put(self.prefix, 'modules/old/old.cppm.meta-info', json.dumps({
            'name': 'alib6.core:old', 'file': 'old/old.cppm'}))
        other = self.put(self.prefix, 'modules/other/api.cppm', 'other library')
        self.put(self.prefix, 'modules/other/api.cppm.meta-info', json.dumps({
            'name': 'other', 'file': 'other/api.cppm'}))
        alib5 = self.put(self.prefix, 'include/alib5/current.h', 'keep')
        installer.apply(self.prefix, stage, 'alib6', adopt=True)
        self.assertFalse((self.prefix / 'modules/old/old.cppm').exists())
        self.assertTrue(other.exists())
        installer.apply(self.prefix, None, 'alib6')
        self.assertFalse((self.prefix / 'share/alib6/modules/main/main.cppm').exists())
        self.assertEqual(alib5.read_text(), 'keep')
        self.assertEqual(other.read_text(), 'other library')

    def test_alib6_upgrade_removes_old_module_pairs(self):
        stage = self.alib6_stage()
        old = 'share/alib6/modules/old/old.cppm'
        self.put(stage, old, 'old interface')
        self.put(stage, old + '.meta-info', '{}')
        installer.apply(self.prefix, stage, 'alib6')
        (stage / old).unlink()
        (stage / (old + '.meta-info')).unlink()
        installer.apply(self.prefix, stage, 'alib6')
        self.assertFalse((self.prefix / old).exists())
        self.assertFalse((self.prefix / (old + '.meta-info')).exists())

    def test_alib6_incomplete_install_is_rejected(self):
        stage = self.alib6_stage()
        (stage / 'share/alib6/modules/main/main.cppm.meta-info').unlink()
        with self.assertRaisesRegex(RuntimeError, 'module metadata'):
            installer.apply(self.prefix, stage, 'alib6')

    def test_alib6_metadata_export_has_own_package_and_no_alib5_defines(self):
        stage = self.root / 'export'
        self.put(stage, 'modules/hash/main.cppm', 'export module alib6;')
        self.put(stage, 'modules/hash/main.cppm.meta-info', json.dumps({
            'name': 'alib6', 'file': 'hash/main.cppm'}))
        sys.path.insert(0, str(ROOT / 'scripts'))
        try:
            entry = runpy.run_path(str(ROOT / 'scripts/install'), run_name='install_test')
        finally:
            sys.path.pop(0)
        entry['metadata'](stage, 'alib6')
        self.assertTrue((stage / 'share/alib6/modules/hash/main.cppm').is_file())
        self.assertTrue((stage / 'share/alib6/repository/packages/a/alib6/xmake.lua').is_file())
        self.assertNotIn('ALIB5', (stage / 'lib/pkgconfig/aaaa0ggmcLib6.pc').read_text())


if __name__ == '__main__':
    unittest.main()
