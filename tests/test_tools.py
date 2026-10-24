from pathlib import Path
import importlib.util
import json
import os
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_release import audit,sha256,safe_name,KNOWN_GRAPHICS_SHA256
from tvw_source_probe import probe,MAX_SOURCE_BYTES

class SourceTextTests(unittest.TestCase):
    def test_valid_utf8_is_not_syntax_pass(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'example.tvw';p.write_text('ordinary text only',encoding='utf-8')
            r=probe(p)
            self.assertEqual(r['status'],'utf8_text_candidate')
            self.assertFalse(r['valid_tvw_syntax'])
    def test_binary_is_not_candidate(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'sample.tvw';p.write_bytes(b'\x00\x01')
            self.assertEqual(probe(p)['status'],'nul_byte')
    def test_over_limit(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'sample.tvw';p.write_bytes(b'a'*(MAX_SOURCE_BYTES+1))
            self.assertEqual(probe(p)['status'],'source_size_limit')
    def test_wrong_extension(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'sample.tss';p.write_text('hi')
            self.assertEqual(probe(p)['status'],'wrong_extension')
    def test_non_utf8(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'sample.tvw';p.write_bytes(b'\xff\xfe')
            self.assertEqual(probe(p)['status'],'non_utf8')

class ArchiveAuditTests(unittest.TestCase):
    def test_sha256(self):
        import hashlib
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'test.bin';p.write_bytes(b'abc')
            self.assertEqual(sha256(p),hashlib.sha256(b'abc').hexdigest())
    def test_safe_names(self):
        for name in ('../x','a/../x','/absolute','C:\\secrets\\key','a\n.txt'):
            self.assertFalse(safe_name(name),name)
        self.assertTrue(safe_name('src/example.c'))
    def test_zip_inventory_and_warnings(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'f.zip'
            with zipfile.ZipFile(p,'w') as z:
                z.writestr('src/main.c','int main(void){return 0;}')
                z.writestr('../sneak.txt','x')
                z.writestr('private/.env','KEY=1')
            r=audit(p,check_crc=True)
            self.assertEqual(r['file_count'],3)
            self.assertEqual(r['format'],'zip')
            self.assertIn('unsafe_archive_path',[x['warning'] for x in r['warnings']])
            self.assertIn('possibly_sensitive_filename',[x['warning'] for x in r['warnings']])
    def test_tar_no_extraction(self):
        import io
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'f.tar.gz'
            with tarfile.open(p,'w:gz') as t:
                blob=b'abc';info=tarfile.TarInfo(name='src/x.c');info.size=len(blob)
                t.addfile(info,io.BytesIO(blob))
            self.assertEqual(audit(p)['file_count'],1)
    def test_graphics_sha_format(self):
        self.assertEqual(len(KNOWN_GRAPHICS_SHA256),64)
        int(KNOWN_GRAPHICS_SHA256,16)
    def test_docs_and_license_present(self):
        for name in ('README.md','README_RO.md','LICENSE','NOTICE.md',
                     'docs/PUBLICATION_SCOPE.md','docs/INTEGRATION_4_4_0.md',
                     'examples/graphics/example_canvas.c'):
            self.assertTrue((ROOT/name).is_file(),name)
        readme=(ROOT/'README.md').read_text(encoding='utf-8')
        self.assertIn('not the full TVW 4.4.0 language compiler or runtime',readme)

if __name__=='__main__':
    unittest.main()
