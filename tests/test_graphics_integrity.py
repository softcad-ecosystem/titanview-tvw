import hashlib
import json
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
GFX = ROOT / 'graphics' / 'libtitangfx'

class GraphicsDistributionTests(unittest.TestCase):
    def test_lgpl_notice(self):
        self.assertIn('LGPL-3.0-or-later', (GFX/'LICENSE.md').read_text(encoding='utf-8'))
        self.assertIn('GNU LESSER GENERAL PUBLIC LICENSE', (GFX/'COPYING.LESSER').read_text(encoding='utf-8').upper())
        self.assertTrue((GFX/'COPYING').is_file())
        self.assertTrue((GFX/'src'/'blake3_vendored'/'LICENSE').is_file())

    def test_complete_internal_sha256_manifest(self):
        entries = (GFX/'MANIFEST_SHA256.txt').read_text(encoding='utf-8').splitlines()
        self.assertEqual(len(entries), 64)
        for entry in entries:
            self.assertRegex(entry, r'^[0-9a-f]{64}  \./')
            digest, filename = entry.split('  ', 1)
            path = GFX / filename
            self.assertTrue(path.is_file(), filename)
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest, filename)

    def test_native_binary_types(self):
        self.assertEqual((GFX/'libtitangfx.so').read_bytes()[:4], b'\x7fELF')
        self.assertEqual((GFX/'path2'/'ts_gfx_path2.so').read_bytes()[:4], b'\x7fELF')
        self.assertEqual((GFX/'libtitangfx.dll').read_bytes()[:2], b'MZ')
        self.assertEqual((GFX/'path2'/'ts_gfx_path2.dll').read_bytes()[:2], b'MZ')

    def test_public_scope_marks_runtime_absent(self):
        scope=json.loads((ROOT/'RELEASE_SCOPE.json').read_text(encoding='utf-8'))
        self.assertTrue(scope['graphics_lgpl_sources_included'])
        self.assertFalse(scope['tvw_4_4_engine_sources_included'])
        self.assertNotEqual(scope['graphics_package_actual_sha256'],scope['graphics_package_previously_recorded_sha256'])

if __name__=='__main__': unittest.main()
