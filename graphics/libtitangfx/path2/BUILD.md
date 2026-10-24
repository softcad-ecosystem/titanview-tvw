# ts_gfx_path2 — companion vectorial 2D (sursă deschisă, LGPL-3.0)

Motor vectorial 2D autonom folosit de `titanview/runtime/context2.py` (Context2, stil
Cairo): canvas RGBA8, matrice CTM, căi Bézier/arc, umplere AA nonzero/even-odd, contur
cu capete/îmbinări/dash, surse solid/gradient-liniar/gradient-radial/imagine, text
TrueType-ca-cale (cu diacritice) și salvare PNG. **Fără dependințe în afară de libc.**

Implementare scrisă de la zero după contractul public de interfață (`context2.py` +
semnăturile din antet). **Nu conține cod din motorul C99 proprietar SoftCAD.**

## Build
```bash
# Linux (.so)
gcc -O2 -std=c99 -fPIC -shared -o ts_gfx_path2.so ts_gfx_path2.c -lm
# Windows (.dll), cross din Linux cu mingw
x86_64-w64-mingw32-gcc -O2 -std=c99 -shared -o ts_gfx_path2.dll ts_gfx_path2.c -lm
```
`.so` și `.dll` gata-construite sînt incluse (convenință). Pune-l în `titanview/lib/`
(TVW îl caută acolo sau prin `TITAN_GFX2_PATH`).

## Verificare
`ts_gfx_path2.so` exportă exact cele 34 de simboluri cerute de Context2, iar
simbolurile nedefinite sînt EXCLUSIV libc (zero legături cu motorul proprietar):
```bash
nm -D ts_gfx_path2.so | grep ' T ts_' | wc -l      # => 34
nm -D ts_gfx_path2.so | grep ' U ' | awk '{print $2}' | sort -u   # doar libc
```
`difftest_vs_referinta.py <referinta.so> ts_gfx_path2.so` compară randarea cu binarul
de referință pe 10 scene (umplere, even-odd, Bézier, stroke, dash, gradiente, CTM, text):
diferențele sînt doar pe muchia anti-aliasată (medie < 0.6/255), structură identică,
advance text identic.
