# Graphics bundle inspection and provenance - 2026-10-24

## Source archive

Name: `SoftCAD_libtitangfx_TVW_LGPL_2026-10-21.zip`  
Supplied ZIP SHA-256: `b2d62c5501a25992dabf2cb718b9ac92c48d562b0f08f9f720832697047e6047`  
Previously documented SHA-256: `7963af733d9232d6d4a75fac944293bdeb595bd49f1e1dc0740254a9d1874e94`  
**Result: DIFFERENT archive-level hashes.** The earlier value is not verified by this upload. This alone does not show malicious modifications; ZIP repacking can change the archive digest while leaving files unchanged. No byte-for-byte comparison with the earlier ZIP was available.

## Inspected content

- 69 ZIP entries (65 files, 4 directories), 2,245,276 uncompressed bytes.
- ZIP central-directory CRC verified: no failures; no unsafe `../` or absolute archive paths found.
- Every one of the 64 files referenced by the supplied `MANIFEST_SHA256.txt` passed (the manifest itself is the additional 65th file).
- Linux `libtitangfx.so`: ELF x86_64, 572 exported `ts_*` functions, no undefined `ts_*` references, required `libm`, `libz`, `libc` dependencies.
- Linux `path2/ts_gfx_path2.so`: ELF x86_64, 34 exported `ts_*` functions, required `libm`, `libc` dependencies.
- Windows `libtitangfx.dll`: PE x64, 589 `ts_*` exports (Linux's 572 plus 17 native window functions), system DLL imports only: GDI32, KERNEL32, msvcrt, SHELL32, USER32.
- Windows `path2/ts_gfx_path2.dll`: PE x64, 34 `ts_*` exports, system DLL imports only: KERNEL32, msvcrt.
- `bash VERIFICARE.sh`: succeeded, including engine and path2 rendered-pixel checks.
- Native Linux GCC rebuilds of `src/libtitangfx_all.c` and `path2/ts_gfx_path2.c`: succeeded with 572 and 34 exports respectively.
- Public single-header C99 canvas sample compiled and emitted `16 16 ff635816` against the provided header.
- A scoped check of source/exports found no exported proprietary PDF/crypto/cadastral/Shield APIs; this is not a forensic security review or proof of proprietary non-overlap.

## Evidence limits

- **Windows DLLs not executed on a physical Windows host** in this preparation session. A GitHub Actions `windows-latest` job is configured to attempt a smoke check; its result must be inspected after push.
- **Full TVW 4.4.0 runtime/compiler not supplied by this archive.** Graphics engine alone does not validate the TVW language syntax or compiler.
- Checksums and builds do not by themselves prove complete license ownership or independent redistributability of every upstream contribution; third-party BLAKE3 notices were retained.

Files from the uploaded ZIP were copied to `graphics/libtitangfx/` **without altering their contents**. Git attributes prevent line-ending rewriting of this source snapshot, so the original manifest stays meaningful after Git checkout.
