# libtitangfx — LGPL-3.0-or-later

Copyright (C) 2026 SoftCAD C+C TOPO S.R.L.

libtitangfx (motorul grafic al ecosistemului SoftCAD / TVW · TitanView) este
**software liber** sub **GNU Lesser General Public License v3.0 sau orice versiune
ulterioară** (LGPL-3.0-or-later).

## Textele oficiale de licență — INCLUSE
Pachetul conține anteturile LGPL pe fiecare sursă **și** textele integrale, verbatim,
ale licențelor oficiale FSF (versiunea canonică, identică cu gnu.org):

- `COPYING.LESSER` — GNU LGPL-3.0 (textul la care se raportează biblioteca)
- `COPYING`        — GNU GPL-3.0 (la care LGPL-3.0 se referă)

LGPL permite folosirea libtitangfx și în aplicații comerciale/închise, atâta timp cât
biblioteca însăși rămâne modificabilă/reînlocuibilă de utilizator (linkare dinamică).

## Companion vectorial `path2/`
`path2/ts_gfx_path2.c` (motorul vectorial 2D folosit de Context2) e inclus cu sursă,
sub aceeași LGPL-3.0-or-later. Scris de la zero (clean-room), fără cod proprietar.

## Componente third-party
- `src/blake3_vendored/` — BLAKE3, upstream **CC0-1.0 OR Apache-2.0** (public). Textele
  upstream sînt incluse: `src/blake3_vendored/LICENSE`, `LICENSE_Apache-2.0`, `LICENSE_CC0-1.0`.
