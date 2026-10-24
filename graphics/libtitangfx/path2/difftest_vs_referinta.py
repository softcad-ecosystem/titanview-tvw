#!/usr/bin/env python3
# Test diferential: compara .so-ul meu (clean-room) cu binarul de referinta.
import ctypes as C, sys, glob, os

REF = sys.argv[1]
MINE = sys.argv[2]
TTF = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
if not os.path.exists(TTF):
    TTF = sorted(glob.glob("/usr/share/fonts/truetype/liberation/*Regular*.ttf"))[0]

vp, f, i, u32 = C.c_void_p, C.c_float, C.c_int, C.c_uint32

class TS_Canvas(C.Structure):
    _fields_ = [("pixels",vp),("width",i),("height",i),("stroke_color",u32),
        ("fill_color",u32),("line_width",f),("opacity",f),("tf",f*9),("tf_active",i),
        ("clip_x",i),("clip_y",i),("clip_w",i),("clip_h",i),("clip_enabled",i)]
class TSSource(C.Structure):
    _fields_=[("type",i),("solid",u32),("x0",f),("y0",f),("x1",f),("y1",f),("r1",f),
        ("nstops",i),("stop_off",f*16),("stop_col",u32*16),("extend",i),("img",vp),
        ("ix",f),("iy",f),("pm",f*6),("pm_active",i)]

def rgba(r,g,b,a=255): return ((a&255)<<24)|((b&255)<<16)|((g&255)<<8)|(r&255)

def bind(path):
    lib=C.CDLL(path)
    lib.ts_canvas_new.argtypes=[i,i]; lib.ts_canvas_new.restype=vp
    lib.ts_canvas_clear.argtypes=[vp,u32]
    for nm2 in ("ts_canvas_free",): getattr(lib,nm2).argtypes=[vp]
    lib.ts_path2_new.restype=vp; lib.ts_path2_free.argtypes=[vp]
    lib.ts_path2_move_to.argtypes=[vp,f,f]; lib.ts_path2_line_to.argtypes=[vp,f,f]
    lib.ts_path2_curve_to.argtypes=[vp,f,f,f,f,f,f]; lib.ts_path2_quad_to.argtypes=[vp,f,f,f,f]
    lib.ts_path2_arc.argtypes=[vp,f,f,f,f,f]; lib.ts_path2_close.argtypes=[vp]
    lib.ts_path2_set_tolerance.argtypes=[vp,f]
    lib.ts_path2_fill.argtypes=[vp,vp,i]; lib.ts_path2_fill_src.argtypes=[vp,vp,i,C.POINTER(TSSource)]
    lib.ts_path2_stroke.argtypes=[vp,vp,f,i,i,f]; lib.ts_path2_stroke_device.argtypes=[vp,vp,f,i,i,f]
    lib.ts_path2_stroke_dashed.argtypes=[vp,vp,f,i,i,f,C.POINTER(f),i,f]
    lib.ts_ctm_identity.argtypes=[vp]; lib.ts_ctm_translate.argtypes=[vp,f,f]
    lib.ts_ctm_scale.argtypes=[vp,f,f]; lib.ts_ctm_rotate.argtypes=[vp,f]
    lib.ts_text_path_string.argtypes=[vp,C.c_char_p,C.c_char_p,f,f,f]; lib.ts_text_path_string.restype=f
    lib.ts_png_save.argtypes=[vp,C.c_char_p]; lib.ts_png_save.restype=i
    return lib

W,H=320,240
def buf(lib,cv):
    s=C.cast(cv,C.POINTER(TS_Canvas)).contents
    n=W*H*4; return bytes((C.c_uint8*n).from_address(s.pixels))

def scene(lib, which):
    cv=lib.ts_canvas_new(W,H)
    s=C.cast(cv,C.POINTER(TS_Canvas)).contents; s.opacity=1.0
    lib.ts_canvas_clear(cv, rgba(255,255,255,255))
    p=lib.ts_path2_new()
    if which=="fill_tri":
        s.fill_color=rgba(200,40,40,255)
        lib.ts_path2_move_to(p,40,30); lib.ts_path2_line_to(p,280,50); lib.ts_path2_line_to(p,160,210); lib.ts_path2_close(p)
        lib.ts_path2_fill(cv,p,0)
    elif which=="star_eo":
        s.fill_color=rgba(30,80,200,255)
        import math
        for k in range(5):
            a=-math.pi/2+k*4*math.pi/5; x=160+120*math.cos(a); y=120+120*math.sin(a)
            (lib.ts_path2_move_to if k==0 else lib.ts_path2_line_to)(p,x,y)
        lib.ts_path2_close(p); lib.ts_path2_fill(cv,p,1)
    elif which=="curve_fill":
        s.fill_color=rgba(20,150,90,255)
        lib.ts_path2_set_tolerance(p,0.09)
        lib.ts_path2_move_to(p,30,120); lib.ts_path2_curve_to(p,90,10,230,10,290,120)
        lib.ts_path2_curve_to(p,230,230,90,230,30,120); lib.ts_path2_close(p); lib.ts_path2_fill(cv,p,0)
    elif which=="stroke_round":
        s.fill_color=rgba(0,0,0,255)
        lib.ts_path2_move_to(p,30,60); lib.ts_path2_line_to(p,150,200); lib.ts_path2_line_to(p,290,60)
        lib.ts_path2_stroke(cv,p,14.0,1,1,10.0)  # round cap/join
    elif which=="stroke_miter":
        s.fill_color=rgba(0,0,0,255)
        lib.ts_path2_move_to(p,40,200); lib.ts_path2_line_to(p,100,40); lib.ts_path2_line_to(p,160,200)
        lib.ts_path2_line_to(p,220,40); lib.ts_path2_line_to(p,280,200)
        lib.ts_path2_stroke(cv,p,10.0,0,0,10.0)  # butt/miter
    elif which=="dashed":
        s.fill_color=rgba(0,0,0,255)
        lib.ts_path2_move_to(p,20,120); lib.ts_path2_line_to(p,300,120)
        arr=(C.c_float*2)(16.0,10.0)
        lib.ts_path2_stroke_dashed(cv,p,6.0,0,0,10.0,arr,2,0.0)
    elif which=="linear":
        src=TSSource(); src.type=1; src.x0=20; src.y0=0; src.x1=300; src.y1=0; src.extend=0
        src.nstops=2; src.stop_off[0]=0.0; src.stop_col[0]=rgba(220,30,30); src.stop_off[1]=1.0; src.stop_col[1]=rgba(30,30,220)
        lib.ts_path2_move_to(p,20,30); lib.ts_path2_line_to(p,300,30); lib.ts_path2_line_to(p,300,210); lib.ts_path2_line_to(p,20,210); lib.ts_path2_close(p)
        lib.ts_path2_fill_src(cv,p,0,C.byref(src))
    elif which=="radial":
        src=TSSource(); src.type=2; src.x0=160; src.y0=120; src.r1=120; src.extend=0
        src.nstops=2; src.stop_off[0]=0.0; src.stop_col[0]=rgba(255,240,180); src.stop_off[1]=1.0; src.stop_col[1]=rgba(40,40,120)
        lib.ts_path2_move_to(p,20,20); lib.ts_path2_line_to(p,300,20); lib.ts_path2_line_to(p,300,220); lib.ts_path2_line_to(p,20,220); lib.ts_path2_close(p)
        lib.ts_path2_fill_src(cv,p,0,C.byref(src))
    elif which=="ctm":
        s.fill_color=rgba(150,40,160,255)
        lib.ts_ctm_identity(cv); lib.ts_ctm_translate(cv,160,120); lib.ts_ctm_rotate(cv,0.5); lib.ts_ctm_scale(cv,1.4,0.8)
        lib.ts_path2_move_to(p,-70,-50); lib.ts_path2_line_to(p,70,-50); lib.ts_path2_line_to(p,70,50); lib.ts_path2_line_to(p,-70,50); lib.ts_path2_close(p)
        lib.ts_path2_fill(cv,p,0); lib.ts_ctm_identity(cv)
    elif which=="text":
        s.fill_color=rgba(0,0,0,255)
        ttf=open(TTF,"rb").read()
        adv=lib.ts_text_path_string(p,ttf,"Agăță!".encode("utf-8"),20.0,120.0,64.0)
        lib.ts_path2_fill(cv,p,1)
        s._adv=adv
    out=buf(lib,cv)
    adv=getattr(s,"_adv",None)
    lib.ts_path2_free(p); lib.ts_canvas_free(cv)
    return out, adv

def compare(a,b):
    n=len(a); tot=0; mx=0; big=0
    # doar canalele RGB (alpha e 255 peste tot aici)
    for k in range(0,n,4):
        for ch in range(3):
            d=abs(a[k+ch]-b[k+ch]); tot+=d
            if d>mx:mx=d
            if d>32:big+=1
    px=n//4
    return tot/(px*3), mx, big, px

libR=bind(REF); libM=bind(MINE)
scenes=["fill_tri","star_eo","curve_fill","stroke_round","stroke_miter","dashed","linear","radial","ctm","text"]
print(f"{'scena':14} {'medie_abs':>10} {'max':>5} {'px_div>32':>10} {'%div':>7}")
worst=0.0
for sc in scenes:
    ra,radv=scene(libR,sc); ma,madv=scene(libM,sc)
    mean,mx,big,px=compare(ra,ma)
    worst=max(worst,mean)
    extra=""
    if sc=="text" and radv is not None and madv is not None:
        extra=f"  advance ref={radv:.1f} mine={madv:.1f} (Δ={abs(radv-madv):.1f})"
    print(f"{sc:14} {mean:10.3f} {mx:5d} {big:10d} {100*big/px:6.2f}%{extra}")
print(f"\nMEDIA ABS cea mai mare pe scene: {worst:.3f} / 255")

# test PNG: salveaza cu lib-ul meu, reciteste si compara cu bufferul
cv=libM.ts_canvas_new(W,H); s=C.cast(cv,C.POINTER(TS_Canvas)).contents
libM.ts_canvas_clear(cv,rgba(123,200,77,255))
import struct,zlib
libM.ts_png_save(cv,b"/tmp/_mytest.png")
raw=open("/tmp/_mytest.png","rb").read()
assert raw[:8]==b"\x89PNG\r\n\x1a\n", "semnatura PNG"
# parse minimal pentru a valida IHDR si ca zlib decomprima
pos=8; okpng=True; wpng=hpng=0
import binascii
idat=b""
while pos<len(raw):
    ln=struct.unpack(">I",raw[pos:pos+4])[0]; typ=raw[pos+4:pos+8]; data=raw[pos+8:pos+8+ln]
    crc=struct.unpack(">I",raw[pos+8+ln:pos+12+ln])[0]
    if binascii.crc32(typ+data)&0xffffffff != crc: okpng=False
    if typ==b"IHDR": wpng,hpng=struct.unpack(">II",data[:8])
    if typ==b"IDAT": idat+=data
    pos+=12+ln
dec=zlib.decompress(idat)
print(f"\nPNG: sig OK, CRC {'OK' if okpng else 'GRESIT'}, {wpng}x{hpng}, zlib decomp {len(dec)} bytes (astept {(W*4+1)*H})")
libM.ts_canvas_free(cv)
