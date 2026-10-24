#!/usr/bin/env python3
# TEST_TVW_WINDOWS.py — testează stiva grafică nativă TVW pe Windows:
#   1) motorul  libtitangfx.dll   (render cerc + pixeli)
#   2) companionul path2 ts_gfx_path2.dll (umplere vectorială AA + PNG)
# Rulează-l prin RULEAZA_TEST_TVW_WINDOWS.bat (dublu-click) sau: python TEST_TVW_WINDOWS.py
import ctypes as C, os, sys, platform

here = os.path.dirname(os.path.abspath(__file__))
is_win = (os.name == "nt")
ENG  = os.path.join(here, "libtitangfx.dll" if is_win else "libtitangfx.so")
P2a  = os.path.join(here, "path2", "ts_gfx_path2.dll" if is_win else "ts_gfx_path2.so")
P2b  = os.path.join(here, "ts_gfx_path2.dll" if is_win else "ts_gfx_path2.so")
P2   = P2a if os.path.exists(P2a) else P2b

vp, f, i, u, ub = C.c_void_p, C.c_float, C.c_int, C.c_uint32, C.c_ubyte
def rgba(r, g, b, a=255): return ((a & 255) << 24) | ((b & 255) << 16) | ((g & 255) << 8) | (r & 255)

fails = []

# ─── 1) MOTOR libtitangfx ───────────────────────────────────────────────
def test_engine():
    if not os.path.exists(ENG):
        return "LIPSĂ (%s negăsit)" % os.path.basename(ENG)
    try:
        l = C.CDLL(ENG)
    except OSError as e:
        return "NU SE ÎNCARCĂ: %s" % e
    l.ts_render_new.restype = vp; l.ts_render_new.argtypes = [i, i]
    l.ts_render_clear.argtypes = [vp, i, i, i]
    l.ts_render_circle_filled.argtypes = [vp, i, i, i, i, i, i, i]
    l.ts_render_pixels.restype = C.POINTER(ub); l.ts_render_pixels.argtypes = [vp]
    rb = l.ts_render_new(100, 100)
    l.ts_render_clear(rb, 10, 20, 30)
    l.ts_render_circle_filled(rb, 50, 50, 30, 255, 128, 0, 255)
    p = l.ts_render_pixels(rb)
    def px(x, y): return tuple(p[(y * 100 + x) * 3 + k] for k in range(3))
    return "OK" if (px(50, 50) == (255, 128, 0) and px(5, 5) == (10, 20, 30)) \
                else "ESUAT %s %s" % (px(50, 50), px(5, 5))

# ─── 2) COMPANION path2 ─────────────────────────────────────────────────
def test_path2():
    if not os.path.exists(P2):
        return "LIPSĂ (ts_gfx_path2 negăsit în path2/ sau lângă .bat)"
    try:
        l = C.CDLL(P2)
    except OSError as e:
        return "NU SE ÎNCARCĂ: %s" % e
    l.ts_canvas_new.argtypes = [i, i]; l.ts_canvas_new.restype = vp
    l.ts_canvas_clear.argtypes = [vp, u]; l.ts_path2_new.restype = vp
    l.ts_path2_move_to.argtypes = [vp, f, f]; l.ts_path2_line_to.argtypes = [vp, f, f]
    l.ts_path2_close.argtypes = [vp]; l.ts_path2_fill.argtypes = [vp, vp, i]
    l.ts_png_save.argtypes = [vp, C.c_char_p]; l.ts_png_save.restype = i
    class CV(C.Structure):
        _fields_ = [("pixels", vp), ("w", i), ("h", i), ("sc", u), ("fc", u),
                    ("lw", f), ("op", f), ("tf", f * 9), ("ta", i),
                    ("cx", i), ("cy", i), ("cw", i), ("ch", i), ("ce", i)]
    cv = l.ts_canvas_new(80, 80)
    s = C.cast(cv, C.POINTER(CV)).contents; s.op = 1.0
    l.ts_canvas_clear(cv, rgba(255, 255, 255)); s.fc = rgba(0, 128, 255)
    p = l.ts_path2_new()
    for k, (x, y) in enumerate([(15, 15), (65, 15), (65, 65), (15, 65)]):
        (l.ts_path2_move_to if k == 0 else l.ts_path2_line_to)(p, x, y)
    l.ts_path2_close(p); l.ts_path2_fill(cv, p, 0)
    buf = C.cast(s.pixels, C.POINTER(u))
    inside = buf[40 * 80 + 40] & 0xFFFFFF          # centrul dreptunghiului umplut
    want = rgba(0, 128, 255) & 0xFFFFFF            # culoarea asteptata, din aceeasi functie
    png_ok = (l.ts_png_save(cv, b"_tvw_test.png") == 0)
    return "OK" if (inside == want and png_ok) else "ESUAT (pixel=%s want=%s png=%s)" % (hex(inside), hex(want), png_ok)

def guard(fn):
    try:
        return fn()
    except AttributeError as e:
        return "ESUAT: simbol lipsă din DLL (%s)" % e
    except Exception as e:
        return "ESUAT: %s" % e

print("=" * 56)
print("  TEST TVW NATIV — %s %s · Python %s" % (platform.system(), platform.machine(), platform.python_version()))
print("=" * 56)
r1 = guard(test_engine); print("  Motor  libtitangfx   :", r1)
r2 = guard(test_path2);  print("  Companion path2      :", r2)
if r1 != "OK": fails.append("motor")
if r2 != "OK": fails.append("path2")
print("=" * 56)
if not fails:
    print("  TOT VERDE — stiva grafică TVW merge pe %s." % platform.system())
    print("  Poți marca TVW ca „Windows verificat”.")
else:
    print("  A PICAT: %s. Trimite-i lui Clodel tot ce scrie mai sus." % ", ".join(fails))
print("=" * 56)
