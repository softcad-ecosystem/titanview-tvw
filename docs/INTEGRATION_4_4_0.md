# Integrating the existing TVW 4.4.0 runtime (separate from graphics SDK)

The full canonical Windows/Linux handoff was previously named:

```
SoftCAD_TVW_4.4.0_CANONIC_PENTRU_TITANCAD_WIN_LINUX_2026-10-19.zip
```

**That archive has not been supplied here.** This repository DOES now contain the actual graphics `libtitangfx` and `path2` libraries from the owner-supplied LGPL bundle, at `graphics/libtitangfx/`, with their matching source and notices. Do not mistake graphics ABI 1.0 for the runtime/compiler version 4.4.0.

The standard unprotected TVW runtime/compiler can be added from its approved canonical archive after a component-level public-redistribution review. Do not publish the full TitanView product, protected compiler or commercial CAD/cadastral/PDF/Shield modules just because the graphics libraries are LGPL.

Use the included read-only inspector to record the archive structure without extracting or publishing it automatically:

```bash
python tools/audit_release.py /path/to/SoftCAD_TVW_4.4.0_CANONIC_PENTRU_TITANCAD_WIN_LINUX_2026-10-19.zip --check-crc
```

The current graphics archive is verified by SHA-256 `b2d62c5501a25992dabf2cb718b9ac92c48d562b0f08f9f720832697047e6047`; the historical different digest `7963af733d9232d6d4a75fac944293bdeb595bd49f1e1dc0740254a9d1874e94` remains documented in [GRAPHICS_VERIFICATION.md](GRAPHICS_VERIFICATION.md).
