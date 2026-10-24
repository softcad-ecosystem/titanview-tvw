# Publication boundaries / Limitele distribuirii

| Component | Present? | License/status |
|---|---|---|
| New docs, read-only inspection tools, tests, C99 example | YES | Apache-2.0 (root LICENSE) where owned by SoftCAD |
| `libtitangfx` C99 engine headers and implementation | YES | LGPL-3.0-or-later, own COPYING / COPYING.LESSER |
| `libtitangfx` Linux x64 `.so`, Windows x64 `.dll` | YES | LGPL-3.0-or-later; binaries provided with complete corresponding source |
| `ts_gfx_path2` source and Linux/Windows libraries | YES | LGPL-3.0-or-later |
| BLAKE3 vendored files | YES | CC0-1.0 OR Apache-2.0 (upstream notices retained) |
| TVW `.tvw` parser, language compiler/runtime 4.4.0 | NO | Separate canonical TVW 4.4.0 distribution; not automatically LGPL |
| Protected compiler/full TitanView application | NO | Proprietary; not approved by this graphics grant |
| TitanCAD, TitanPack, TitanArchive, TitanScann application sources | NO | Separate products |
| Shield / license server / proprietary cadastral, CAD, PDF, crypto | NO | Not published here |

**A graphics SDK can be distributed without open-sourcing a protected compiler.** The owner-provided archive states `libtitangfx` and `ts_gfx_path2` are LGPL-3.0-or-later. No broader license claim is made for the TVW runtime or software products using it.

Before publishing a release note that asserts byte-for-byte identity with a previous canonical ZIP, reconcile the older checksum against the actual uploaded archive. See `GRAPHICS_VERIFICATION.md`.
