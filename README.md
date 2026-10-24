# TitanView / TVW - SoftCAD developer resources and native graphics SDK

[Romana](README_RO.md) | [SoftCAD](https://softcad.ro) | [TVW documentation](https://softcad.ro/documentatie-tvw.html) | [Format tools](https://github.com/softcad-ecosystem/softcad-formats)

**TitanView / TVW is an existing SoftCAD technology.** The owner reports use of TVW in TitanPack, TitanArchive, TitanScann and TitanCAD. This repository brings together public language/developer references **and the actual LGPL C99 graphics source and binaries supplied by the owner**.

## Included

- `graphics/libtitangfx/src/`: complete source from the supplied LGPL graphics bundle, including public headers and BLAKE3 upstream notices.
- `graphics/libtitangfx/libtitangfx.so`, `libtitangfx.dll`: Linux x86_64 and Windows x64 graphics engines.
- `graphics/libtitangfx/path2/ts_gfx_path2.c`, `.so`, `.dll`: vector path companion with source for both systems.
- `graphics/libtitangfx/COPYING`, `COPYING.LESSER`, `LICENSE.md`, `NOTICE.md`, `MANIFEST_SHA256.txt`: unmodified upstream package licensing and checksum records.
- Public documentation, C99 graphics sample, safe archive/source inspectors and tests.

### What this is NOT

**This is the graphics SDK, not the full TVW 4.4.0 language compiler or runtime.** The existing canonical TVW 4.4.0 archive is a distinct distribution, not embedded here. No TitanCAD, TitanArchive, TitanPack, TitanScann application source, protected compiler, Shield, CAD/cadastral/PDF or commercial modules have been added. The `.tvw` source probe is NOT a compiler or grammar validator.

## Build / try it on Linux

```bash
# Verify the owner-supplied binaries + graphics smoke test
cd graphics/libtitangfx
sha256sum -c MANIFEST_SHA256.txt
bash VERIFICARE.sh

# Build engine and path2 from the supplied C99 sources
mkdir -p /tmp/softcad-tvw-build
gcc -std=c99 -O2 -fPIC -DTS_BUILDING -shared -I src src/libtitangfx_all.c -Wl,-z,defs -lm -lz -o /tmp/softcad-tvw-build/libtitangfx.so
gcc -std=c99 -O2 -fPIC -shared path2/ts_gfx_path2.c -lm -o /tmp/softcad-tvw-build/ts_gfx_path2.so

# C99 single-header public graphics illustration
cd ../..
gcc -std=c99 -O2 -I graphics/libtitangfx/src examples/graphics/example_canvas.c -o /tmp/softcad-tvw-build/example_canvas
/tmp/softcad-tvw-build/example_canvas

# Python documentation / provenance checks
python -m unittest discover -s tests -v
```

The C99 example emits `16 16 ff635816`. Linux smoke and recompilation from this exact supplied package passed in the preparation environment. **Physical Windows testing is not claimed**: the owner-supplied Windows DLLs were inspected as PE x64 files with only system DLL imports and contain the required API exports; `.github/workflows/check.yml` will test them on a Windows runner after publication.

## Packaging and provenance

Supplied file: `SoftCAD_libtitangfx_TVW_LGPL_2026-10-21.zip`  
**SHA-256 of the uploaded archive:** `b2d62c5501a25992dabf2cb718b9ac92c48d562b0f08f9f720832697047e6047`.

An earlier bootstrap document listed `7963af733d9232d6d4a75fac944293bdeb595bd49f1e1dc0740254a9d1874e94` for that same filename. **These SHA-256 values differ.** Every member of the newly uploaded ZIP matches its own included `MANIFEST_SHA256.txt`, but that does not establish bytewise identity with the earlier referenced ZIP. The exact user-supplied files are preserved under `graphics/libtitangfx/`; consult [the verification report](docs/GRAPHICS_VERIFICATION.md) before describing the ZIP as identical to a prior release.

## Licenses / licensing scope

- `graphics/libtitangfx` C99 graphics source and binaries: **LGPL-3.0-or-later** under its own `LICENSE.md`, `COPYING.LESSER` and `COPYING`. This includes `path2/`.
- `graphics/libtitangfx/src/blake3_vendored`: **CC0-1.0 OR Apache-2.0**, with upstream licenses included.
- Root `LICENSE` (Apache-2.0): **only** new documentation, utilities, sample and tests separately authored for this repository, where SoftCAD owns rights; it does not relicense LGPL graphics or TVW runtime.

Documentation: [Graphics SDK](docs/GRAPHICS_LGPL.md) | [License boundary](docs/PUBLICATION_SCOPE.md) | [TVW 4.4.0 separate integration](docs/INTEGRATION_4_4_0.md) | [Verification](docs/GRAPHICS_VERIFICATION.md)
