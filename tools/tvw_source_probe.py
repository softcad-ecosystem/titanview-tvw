#!/usr/bin/env python3
"""Read-only plausibility probe, not a TVW grammar parser or security validator."""
from __future__ import annotations
import argparse
import json
from pathlib import Path

MAX_SOURCE_BYTES=65536

def probe(path:Path):
    if path.suffix.lower()!='.tvw':
        return {'status':'wrong_extension','valid_tvw_syntax':False}
    try:
        n=path.stat().st_size
        if n>MAX_SOURCE_BYTES:
            return {'status':'source_size_limit','bytes':n,'valid_tvw_syntax':False}
        payload=path.read_bytes()
    except OSError as exc:
        return {'status':'io_error','error':str(exc),'valid_tvw_syntax':False}
    if not payload:
        return {'status':'empty_source','bytes':0,'valid_tvw_syntax':False}
    if b'\x00' in payload:
        return {'status':'nul_byte','bytes':len(payload),'valid_tvw_syntax':False}
    try:
        payload.decode('utf-8-sig','strict')
    except UnicodeError:
        return {'status':'non_utf8','bytes':len(payload),'valid_tvw_syntax':False}
    return {'status':'utf8_text_candidate','bytes':len(payload),'valid_tvw_syntax':False}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('file',type=Path)
    args=ap.parse_args()
    print(json.dumps(probe(args.file),ensure_ascii=False,indent=2))
    return 0

if __name__=='__main__':
    raise SystemExit(main())
