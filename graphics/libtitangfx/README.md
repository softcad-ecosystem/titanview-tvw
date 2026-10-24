# libtitangfx — motorul grafic deschis SoftCAD (TVW / TitanView)

Alternativă deschisă la stiva grafică Qt/PySide, **cu tot cu motorizare**, LGPL-3.0.
Motor C99 pur (zlib pentru PNG + BLAKE3 vendat). 572 simboluri grafice, 21/21 CORE_SYMBOLS.

## Build
```bash
# Linux (.so) — -Wl,-z,defs forteaza rezolvarea tuturor simbolurilor la linkare
gcc -std=c99 -O3 -fPIC -DTS_BUILDING -shared -I src src/libtitangfx_all.c \
    -Wl,-z,defs -lm -lz -o libtitangfx.so

# Windows (.dll) — STATIC + --export-all-symbols OBLIGATORIU:
#   unele headere folosesc __declspec(dllexport), ceea ce dezactiveaza auto-exportul
#   mingw pentru restul (ts_render_new etc. ar ramane neexportate). --export-all-symbols
#   forteaza exportul intregului API. Fara el, .dll se incarca dar nu expune functiile.
x86_64-w64-mingw32-gcc -O2 -DTS_BUILDING -shared -I src src/libtitangfx_all.c \
    -Wl,--export-all-symbols -static-libgcc -Wl,-Bstatic -lz -Wl,-Bdynamic -lgdi32 -luser32 \
    -o libtitangfx.dll

# companion vectorial path2 (autonom, doar libc)
gcc -O2 -std=c99 -fPIC -shared -o path2/ts_gfx_path2.so path2/ts_gfx_path2.c -lm
x86_64-w64-mingw32-gcc -O2 -std=c99 -shared -o path2/ts_gfx_path2.dll path2/ts_gfx_path2.c -lm
```
`.so` și `.dll` gata-construite sînt incluse (convenință).

## Dependințe de distribuție (verificat 2026-10-21)
- **Linux** `libtitangfx.so`: `DT_NEEDED` = libm.so.6 + libz.so.1 + libc.so.6 (declarate
  explicit); `dlopen` reușește în proces curat. `path2/ts_gfx_path2.so`: doar libm + libc.
- **Windows** `libtitangfx.dll`: importă DOAR DLL-uri de sistem (KERNEL32/USER32/GDI32/
  SHELL32/msvcrt) — pornește pe Windows curat. `path2/ts_gfx_path2.dll`: doar KERNEL32+msvcrt.
- **Windows real**: `.dll`-urile sînt cross-compilate (mingw), nerulate încă pe Windows fizic.
  Rulează `TEST_TVW_WINDOWS.py` pe Windows → „TOT VERDE" = poți marca „Windows verificat".

## Verificare
```bash
bash VERIFICARE.sh    # anti-scurgere (simboluri motor + path2) + render pixel
```

## Folosire cu TitanView
Pune `libtitangfx.so`/`.dll` ca `ts_engines` pe calea motorului TVW. Cele 21 CORE_SYMBOLS
cerute de `native_engine_abi` sînt toate prezente. Companionul `ts_gfx_path2` (căi/stroke/text,
folosit de `context2.py`) e inclus cu sursă în `path2/` — pune `path2/ts_gfx_path2.so`/`.dll`
în `titanview/lib/`. Vezi `path2/BUILD.md`.

## Licență
LGPL-3.0-or-later. Textele oficiale `COPYING` (GPLv3) + `COPYING.LESSER` (LGPLv3) sînt incluse.
