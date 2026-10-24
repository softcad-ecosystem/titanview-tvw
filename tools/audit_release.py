#!/usr/bin/env python3
"""Inventory a canonical archive WITHOUT extracting or executing any file.

Shows hashes and filename/path warnings. NOT a full secrets/license/malware audit.
"""
from __future__ import annotations
import argparse
import collections
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import tarfile
import zipfile

KNOWN_GRAPHICS_SHA256='b2d62c5501a25992dabf2cb718b9ac92c48d562b0f08f9f720832697047e6047'
SENSITIVE_NAME=re.compile(r'(^|/)(?:\.env(?:\..*)?|id_rsa|id_ed25519|credentials?\.json|secrets?\.[^/]+|[^/]+\.(?:p12|pfx|key|pem))$',re.IGNORECASE)

def safe_name(name):
    name=name.replace('\\','/')
    if not name or name.startswith('/') or re.match(r'^[A-Za-z]:/',name) or any(ord(c)<32 for c in name):
        return False
    return '..' not in PurePosixPath(name).parts

def sha256(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            h.update(block)
    return h.hexdigest()

def audit(path:Path,check_crc=False):
    path=Path(path)
    checksum=sha256(path)
    file_sizes=[]
    notes=[]
    if zipfile.is_zipfile(path):
        kind='zip'
        with zipfile.ZipFile(path) as z:
            infos=z.infolist()
            for info in infos:
                if info.is_dir(): continue
                mode=(info.external_attr>>16)&0xffff
                file_sizes.append((info.filename,info.file_size))
                if stat.S_ISLNK(mode):notes.append({'path':info.filename,'warning':'symlink'})
            if check_crc:
                corrupted=z.testzip()
                if corrupted:notes.append({'path':corrupted,'warning':'zip_crc_failed'})
    elif tarfile.is_tarfile(path):
        kind='tar'
        with tarfile.open(path,mode='r:*') as t:
            for member in t.getmembers():
                if member.issym() or member.islnk():notes.append({'path':member.name,'warning':'link'})
                if member.isfile():file_sizes.append((member.name,member.size))
    else:
        raise ValueError('Unsupported archive: expected ZIP or TAR')
    types=collections.Counter()
    for name,size in file_sizes:
        norm=name.replace('\\','/')
        types[PurePosixPath(norm).suffix.lower() or '(no extension)']+=1
        if not safe_name(name):notes.append({'path':name,'warning':'unsafe_archive_path'})
        if SENSITIVE_NAME.search(norm):notes.append({'path':name,'warning':'possibly_sensitive_filename'})
    return {'name':path.name,'sha256':checksum,'format':kind,'file_count':len(file_sizes),
            'total_uncompressed_bytes':sum(size for _,size in file_sizes),
            'extensions':dict(types.most_common()),'warnings':notes,
            'read_only':True,'not_a_license_or_secret_content_audit':True}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive',type=Path)
    p.add_argument('--expected-sha256',type=str)
    p.add_argument('--check-crc',action='store_true')
    p.add_argument('--json-out',type=Path)
    args=p.parse_args()
    try: report=audit(args.archive,args.check_crc)
    except (OSError,ValueError,tarfile.TarError,zipfile.BadZipFile) as exc:
        p.error(str(exc))
    if args.expected_sha256:
        report['sha256_matches_expected']=report['sha256']==args.expected_sha256.lower()
    data=json.dumps(report,indent=2,ensure_ascii=False)+'\n'
    print(data,end='')
    if args.json_out:
        args.json_out.write_text(data,encoding='utf-8')
    return 2 if args.expected_sha256 and not report['sha256_matches_expected'] else 0

if __name__=='__main__':
    raise SystemExit(main())
