# Graphics SDK - real, bundled LGPL sources / SDK grafic LGPL

The approved graphics scope covers `libtitangfx` and `ts_gfx_path2` under LGPL-3.0-or-later. Both components and their supplied Windows/Linux binaries are now included **as exact original files** under `graphics/libtitangfx/`.

The copy contains the full `COPYING`, `COPYING.LESSER`, `LICENSE.md`, `NOTICE.md`, `MANIFEST_SHA256.txt`, third-party BLAKE3 notices, C99 sources, `.so` and `.dll` files. See the original [graphics README](../graphics/libtitangfx/README.md) and [test report](GRAPHICS_VERIFICATION.md).

Owner-supplied ZIP SHA-256: `b2d62c5501a25992dabf2cb718b9ac92c48d562b0f08f9f720832697047e6047`. Prior bootstrap claimed `7963af733d9232d6d4a75fac944293bdeb595bd49f1e1dc0740254a9d1874e94`. The prior value must **not** be presented as the digest of this uploaded file. All individual files in the received ZIP match their own original checksum manifest.

Graphics ABI version is 1.0 in `src/ts_engine_abi.h`; **this is not a TVW 4.4.0 runtime version**.

Build the example using included headers:

```bash
gcc -std=c99 -O2 -I graphics/libtitangfx/src examples/graphics/example_canvas.c -o example_canvas
./example_canvas
```

Expected output: `16 16 ff635816`.
