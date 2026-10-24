# NOTICE — libtitangfx (TVW / TitanView, motor grafic)

Copyright (C) 2026 SoftCAD C+C TOPO S.R.L. — LGPL-3.0-or-later.

## Ce ESTE (deschis, complet)
Motorul grafic complet: canvas, desen 2D (linii/poligoane/arce/bezier), transformări
(CTM, CRS→pixel), text, export (PNG/raster/SVG), 3D + raster3D, imgproc, framebuffer,
tile cache, render manager, spatial index/ops, polygon simplify, plot raster, charts,
blend modes, SDF fonts, UDF, voronoi/quadtree. **572 simboluri `ts_*`**, 21/21 CORE_SYMBOLS
cerute de TitanView 4.x. Hashing generic: SHA-256 (pur), BLAKE3 (vendat), CRC32.

## Ce NU conține (bijuteriile — rămân comerciale, închise)
Șterse FIZIC din sursa publică (nu doar `#ifdef`):
- Stereo70 / CRS snyder (transformarea cadastrală exactă) — rămâne doar ghicitorul de
  format + eticheta, nu matematica;
- TCP (arie exactă, overlap, forced-split), topologie, audit cadastral, UAT pipeline;
- PDF (ts_titan_pdf/pdf_doctor/koenigsegg), cripto grea (AES/RC4/RSA/PKCS/HMAC/PBKDF);
- formatele (CGXML, TCAD, CGB, DXF, WKT, CSV, XLSX, TDOC), DB, TSS, terravault.
Verificabil: `nm -D libtitangfx.so | grep -E 'pdf|aes|rc4|tcp|cgxml|snyder|stereo70'` = gol.

## Scos în plus la auditul din 2026-10-21 (business-logic non-grafic)
Fișierul `src/ts_utilities.h` (grab-bag „cross-produs") a fost ELIMINAT complet din build —
nu era folosit de motorul grafic (0 referințe) și nu avea ce căuta într-un toolkit grafic:
- **validatori date românești**: `ts_validate_cnp`, `ts_cnp_info` (decoda CNP→dată/sex/județ),
  `ts_validate_cui`, `ts_validate_iban`, `ts_validate_phone_ro` — SCOASE (nu „de aprobat");
- QR (`ts_qr_*`), aritmetică de date pentru contracte (`ts_date_*`), motor de template
  (`ts_template_render`), procesare text RO (`ts_text_*_ro`), conversii de unități cadastrale
  (`ts_units_*`), conversii spațiu-culoare (`ts_color_rgb_to_hsl/hsl_to_rgb/rgb_to_cmyk`) — SCOASE.
Rezultat: 603 → **572** simboluri. Dacă vrei conversia RGB↔HSL↔CMYK ca API grafic public,
se re-adaugă doar acea secțiune (≈30 linii) — restul e business-logic, stă în altă parte.
Paleta cadastrală `ts_color_palette_cadastral` (doar RGB pentru straturi) e grafică — PĂSTRATĂ.

## Companion vectorial — INCLUS cu sursă (`path2/`)
- `ts_gfx_path2` (`ts_path2_*`/`ts_ctm_*`/`ts_text_path_*`): motorul 2D stil Cairo pe care
  TitanView îl încarcă separat. Sursa deschisă în `path2/ts_gfx_path2.c` (LGPL-3.0, clean-room
  după `context2.py`, fără cod proprietar). Binare `path2/ts_gfx_path2.{so,dll}`: 34 simboluri,
  **autonom pe ambele platforme** — Linux cere doar libm+libc, Windows doar KERNEL32+msvcrt
  (DLL-uri de sistem). Verificat diferențial vs. binarul istoric: randare identică pînă la
  muchia AA, advance text identic. Vezi `path2/BUILD.md`.

## Dependințe de distribuție (QA 2026-10-21)
- **Linux `libtitangfx.so`**: reconstruit cu `-Wl,-z,defs -lm -lz` → `DT_NEEDED` declară
  explicit libm.so.6 + libz.so.1 + libc.so.6; `dlopen` reușește într-un proces curat.
  (Bug reparat: versiunea anterioară nu avea libm/libz în NEEDED → `undefined symbol: sincos`
  la dlopen din C; Python masca eroarea fiindcă avea deja libm încărcat.)
- **Windows `libtitangfx.dll`**: reconstruit STATIC (`-static-libgcc` + `libz.a` static) →
  importă DOAR DLL-uri de sistem (KERNEL32/USER32/GDI32/SHELL32/msvcrt). **Fără `zlib1.dll`,
  fără `libgcc_s_seh-1.dll`** — pornește pe un Windows curat.
- **Verificare Windows reală**: `.dll`-urile sînt cross-compilate (mingw), nerulate încă pe
  Windows fizic. Rulează o dată `TEST_TVW_WINDOWS.py` pe Windows înainte de „verificat Windows".

## Third-party
- BLAKE3 (`src/blake3_vendored/`) — CC0-1.0 OR Apache-2.0 (texte incluse în folder).
