#!/usr/bin/env bash
# Verifică libtitangfx.so: anti-scurgere (simboluri) + încărcare + render pixel.
set -e; cd "$(dirname "$0")"; GFX="$PWD/libtitangfx.so"
echo "dim: $(stat -c%s "$GFX") bytes | ts_ exportate: $(nm -D --defined-only "$GFX"|grep -c ' T ts_') | nedefinite ts_: $(nm -D -u "$GFX"|awk '{print $NF}'|grep -c '^ts_')"
BAD=$(nm -D --defined-only "$GFX"|awk '{print $NF}'|grep -E '^ts_(pdf|aes|rc4|rsa|pkcs|hmac|pbkdf|tcp|cgxml|tcad|cgb|dxf|csv|xml|xlsx|tdoc|tss|tdb|db_|validate_cnp|cnp_info|validate_cui|validate_iban|validate_phone|qr_encode|qr_render|template_render|units_|date_add|date_today|date_from|date_diff)|snyder|stereo70' || true)
[ -z "$BAD" ] && echo "ANTI-SCURGERE SIMBOLURI: OK" || { echo "SCURGERE: $BAD"; exit 1; }
echo "ANTI-SCURGERE SURSA: $(grep -rIl -iE 'snyder|stereo70 .*=|tcp_area\(|ts_titan_pdf|aes_encrypt' src --include=*.c --include=*.h 2>/dev/null | grep -v blake3_vendored | wc -l) fisiere cu cod tentacul (asteptat 0)"
python3 - "$GFX" <<'PY'
import ctypes as C,sys
l=C.CDLL(sys.argv[1]); l.ts_render_new.restype=C.c_void_p;l.ts_render_new.argtypes=[C.c_int]*2
l.ts_render_clear.argtypes=[C.c_void_p]+[C.c_int]*3;l.ts_render_circle_filled.argtypes=[C.c_void_p]+[C.c_int]*7
l.ts_render_pixels.restype=C.POINTER(C.c_ubyte);l.ts_render_pixels.argtypes=[C.c_void_p]
rb=l.ts_render_new(100,100);l.ts_render_clear(rb,10,20,30);l.ts_render_circle_filled(rb,50,50,30,255,128,0,255)
p=l.ts_render_pixels(rb);c=lambda x,y:tuple(p[(y*100+x)*3+k] for k in range(3))
print("RENDER:", "OK" if c(50,50)==(255,128,0) and c(5,5)==(10,20,30) else "ESUAT", c(50,50), c(5,5))
PY

# --- companion vectorial path2 ---
P2="$PWD/path2/ts_gfx_path2.so"
if [ -f "$P2" ]; then
  echo "path2 ts_ exportate: $(nm -D --defined-only "$P2"|grep -c ' T ts_') (astept 34) | nedefinite doar libc: $(nm -D -u "$P2"|awk '{print $NF}'|grep -c '^ts_') (astept 0)"
  P2BAD=$(nm -D "$P2"|awk '{print $NF}'|grep -iE 'snyder|stereo|cgxml|shield|sesiune|koenig|sentinel|tcp' || true)
  [ -z "$P2BAD" ] && echo "path2 ANTI-SCURGERE: OK" || { echo "path2 SCURGERE: $P2BAD"; exit 1; }
  python3 - "$P2" <<'PY'
import ctypes as C,sys
l=C.CDLL(sys.argv[1]); vp,f,i,u=C.c_void_p,C.c_float,C.c_int,C.c_uint32
l.ts_canvas_new.argtypes=[i,i];l.ts_canvas_new.restype=vp
l.ts_canvas_clear.argtypes=[vp,u];l.ts_path2_new.restype=vp
l.ts_path2_move_to.argtypes=[vp,f,f];l.ts_path2_line_to.argtypes=[vp,f,f];l.ts_path2_close.argtypes=[vp]
l.ts_path2_fill.argtypes=[vp,vp,i]
class CV(C.Structure):
 _fields_=[("pixels",vp),("w",i),("h",i),("sc",u),("fc",u),("lw",f),("op",f),("tf",f*9),("ta",i),("cx",i),("cy",i),("cw",i),("ch",i),("ce",i)]
cv=l.ts_canvas_new(60,60);s=C.cast(cv,C.POINTER(CV)).contents;s.op=1.0
l.ts_canvas_clear(cv,0xFF102030);s.fc=0xFF0080FF
p=l.ts_path2_new()
for a,b in [(10,10),(50,10),(50,50),(10,50)]:
 (l.ts_path2_move_to if a==10 and b==10 else l.ts_path2_line_to)(p,a,b)
l.ts_path2_close(p);l.ts_path2_fill(cv,p,0)
px=C.cast(s.pixels,C.POINTER(u));v=px[30*60+30]&0xFFFFFF
# rgba LE: low24 = b<<16|g<<8|r = 00,80,FF -> 0x0080FF pentru r=255,g=128,b=0
print("path2 RENDER:", "OK" if v==0x0080FF else "ESUAT", hex(v))
PY
fi

# --- exporturi DLL Windows (prinde bug-ul mingw dllexport-dezactiveaza-autoexport) ---
OBJ=$(command -v x86_64-w64-mingw32-objdump || true)
if [ -n "$OBJ" ] && [ -f libtitangfx.dll ]; then
  NE=$("$OBJ" -p libtitangfx.dll | grep -cE '\] ts_')
  echo "libtitangfx.dll: $NE simboluri ts_ EXPORTATE"
  for s in ts_render_new ts_canvas_new ts_render_pixels ts_render_mgr_new ts_render_triangle; do
    "$OBJ" -p libtitangfx.dll | grep -qE "\] $s\$" || { echo "DLL NU EXPORTA $s (lipseste --export-all-symbols)"; exit 1; }
  done
  IMP=$("$OBJ" -p libtitangfx.dll | grep -i 'DLL Name' | grep -viE 'KERNEL32|USER32|GDI32|SHELL32|msvcrt' || true)
  [ -z "$IMP" ] && echo "libtitangfx.dll: doar DLL-uri de sistem (autonom) OK" || { echo "DLL cere non-sistem: $IMP"; exit 1; }
  if [ -f path2/ts_gfx_path2.dll ]; then
    echo "path2.dll: $("$OBJ" -p path2/ts_gfx_path2.dll | grep -cE '\] ts_') simboluri ts_ EXPORTATE (astept 34)"
  fi
fi
