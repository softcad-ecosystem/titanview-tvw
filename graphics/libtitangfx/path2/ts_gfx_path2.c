/* ts_gfx_path2 — motor vectorial 2D autonom (canvas RGBA8, CTM, cai Bezier/arc,
 * umplere AA nonzero/evenodd, contur cu caps/joins/dash, surse solid/gradient/
 * imagine, text TTF-ca-cale, salvare PNG). Implementare independenta, fara
 * dependinte in afara de libc. Compatibil ABI cu interfata Context2 a TitanView.
 *
 * Copyright (c) 2026 Crinel Popa / SoftCAD C+C TOPO.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Aceasta biblioteca este software liber: o puteti redistribui si/sau modifica
 * sub termenii GNU Lesser General Public License, versiunea 3 sau (la alegere)
 * orice versiune ulterioara. Distribuita in speranta ca este utila, dar FARA
 * NICIO GARANTIE. Vedeti COPYING.LESSER pentru detalii.
 *
 * Scris de la zero dupa contractul public de interfata (titanview/runtime/
 * context2.py) si semnaturile din antet. Nu contine cod derivat din motorul
 * C99 proprietar al SoftCAD.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#if defined(_WIN32)
#  define TS_API __declspec(dllexport)
#else
#  define TS_API __attribute__((visibility("default")))
#endif

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* ----------------------------------------------------------------- culoare */
/* rgba() din titan_canvas_lib: uint32 LE, byte0=R byte1=G byte2=B byte3=A */
static inline uint32_t pack_rgba(int r,int g,int b,int a){
    return ((uint32_t)(a&0xFF)<<24)|((uint32_t)(b&0xFF)<<16)|
           ((uint32_t)(g&0xFF)<<8)|((uint32_t)(r&0xFF));
}
#define CR(c) ((int)((c)&0xFF))
#define CG(c) ((int)(((c)>>8)&0xFF))
#define CB(c) ((int)(((c)>>16)&0xFF))
#define CA(c) ((int)(((c)>>24)&0xFF))

/* ----------------------------------------------------------------- canvas */
/* Oglinda exacta a TS_Canvas (ts_gfx_canvas.h / titan_canvas_lib.py). */
typedef struct {
    void*    pixels;
    int      width;
    int      height;
    uint32_t stroke_color;
    uint32_t fill_color;
    float    line_width;
    float    opacity;
    float    tf[9];
    int      tf_active;
    int      clip_x, clip_y, clip_w, clip_h, clip_enabled;
} TS_Canvas;

TS_API TS_Canvas *ts_canvas_new(int width,int height){
    if(width<=0||height<=0) return NULL;
    TS_Canvas *c=(TS_Canvas*)calloc(1,sizeof(TS_Canvas));
    if(!c) return NULL;
    c->pixels=calloc((size_t)width*height,4);
    if(!c->pixels){ free(c); return NULL; }
    c->width=width; c->height=height;
    c->fill_color=pack_rgba(0,0,0,255);
    c->stroke_color=pack_rgba(0,0,0,255);
    c->line_width=1.0f; c->opacity=1.0f;
    c->tf[0]=1;c->tf[4]=1;c->tf[8]=1; c->tf_active=0;
    return c;
}
TS_API void ts_canvas_free(TS_Canvas *c){ if(c){ free(c->pixels); free(c);} }
TS_API void ts_canvas_resize(TS_Canvas *c,int w,int h){
    if(!c||w<=0||h<=0) return;
    void *np=calloc((size_t)w*h,4); if(!np) return;
    free(c->pixels); c->pixels=np; c->width=w; c->height=h;
}
TS_API int ts_canvas_width(const TS_Canvas *c){ return c?c->width:0; }
TS_API int ts_canvas_height(const TS_Canvas *c){ return c?c->height:0; }
TS_API void ts_canvas_clear(TS_Canvas *c,uint32_t color){
    if(!c||!c->pixels) return;
    uint32_t *p=(uint32_t*)c->pixels; size_t n=(size_t)c->width*c->height;
    for(size_t i=0;i<n;i++) p[i]=color;
}
TS_API void ts_pixel_set(TS_Canvas *c,int x,int y,uint32_t color){
    if(!c||x<0||y<0||x>=c->width||y>=c->height) return;
    ((uint32_t*)c->pixels)[(size_t)y*c->width+x]=color;
}
TS_API uint32_t ts_pixel_get(const TS_Canvas *c,int x,int y){
    if(!c||x<0||y<0||x>=c->width||y>=c->height) return 0;
    return ((uint32_t*)c->pixels)[(size_t)y*c->width+x];
}
/* src-over cu alpha pe pixel (nepremultiplicat in buffer) */
static inline void blend_px(TS_Canvas *c,int x,int y,int r,int g,int b,float a){
    if(a<=0.f||x<0||y<0||x>=c->width||y>=c->height) return;
    if(a>1.f) a=1.f;
    uint32_t *dp=&((uint32_t*)c->pixels)[(size_t)y*c->width+x];
    uint32_t d=*dp;
    float da=CA(d)/255.f;
    float oa=a+da*(1.f-a);
    if(oa<=1e-6f){ *dp=0; return; }
    float inv=1.f/oa;
    int rr=(int)((r*a+CR(d)*da*(1.f-a))*inv+0.5f);
    int gg=(int)((g*a+CG(d)*da*(1.f-a))*inv+0.5f);
    int bb=(int)((b*a+CB(d)*da*(1.f-a))*inv+0.5f);
    int aa=(int)(oa*255.f+0.5f);
    *dp=pack_rgba(rr,gg,bb,aa);
}
TS_API void ts_pixel_blend(TS_Canvas *c,int x,int y,uint32_t color){
    if(!c) return;
    blend_px(c,x,y,CR(color),CG(color),CB(color),CA(color)/255.f);
}
TS_API void ts_canvas_blit(TS_Canvas *dst,int dx,int dy,TS_Canvas *src,
                           int sx,int sy,int sw,int sh){
    if(!dst||!src) return;
    for(int j=0;j<sh;j++){
        int syy=sy+j; if(syy<0||syy>=src->height) continue;
        for(int i=0;i<sw;i++){
            int sxx=sx+i; if(sxx<0||sxx>=src->width) continue;
            uint32_t s=((uint32_t*)src->pixels)[(size_t)syy*src->width+sxx];
            blend_px(dst,dx+i,dy+j,CR(s),CG(s),CB(s),CA(s)/255.f);
        }
    }
}

/* ----------------------------------------------------------------- CTM */
/* tf = 3x3 rand-major, afin: xd=tf0*x+tf1*y+tf2, yd=tf3*x+tf4*y+tf5 */
static void mat_mul(float *M,const float *A){ /* M = M * A (3x3) */
    float R[9];
    for(int r=0;r<3;r++)for(int col=0;col<3;col++){
        R[r*3+col]=M[r*3+0]*A[0*3+col]+M[r*3+1]*A[1*3+col]+M[r*3+2]*A[2*3+col];
    }
    memcpy(M,R,sizeof R);
}
TS_API void ts_ctm_identity(TS_Canvas *c){
    if(!c) return;
    memset(c->tf,0,sizeof c->tf);
    c->tf[0]=1;c->tf[4]=1;c->tf[8]=1; c->tf_active=1;
}
TS_API void ts_ctm_translate(TS_Canvas *c,float tx,float ty){
    if(!c) return; if(!c->tf_active) ts_ctm_identity(c);
    float A[9]={1,0,tx, 0,1,ty, 0,0,1}; mat_mul(c->tf,A); c->tf_active=1;
}
TS_API void ts_ctm_scale(TS_Canvas *c,float sx,float sy){
    if(!c) return; if(!c->tf_active) ts_ctm_identity(c);
    float A[9]={sx,0,0, 0,sy,0, 0,0,1}; mat_mul(c->tf,A); c->tf_active=1;
}
TS_API void ts_ctm_rotate(TS_Canvas *c,float ang){
    if(!c) return; if(!c->tf_active) ts_ctm_identity(c);
    float s=sinf(ang),co=cosf(ang);
    float A[9]={co,-s,0, s,co,0, 0,0,1}; mat_mul(c->tf,A); c->tf_active=1;
}
static void ctm_apply(const TS_Canvas *c,float x,float y,float *ox,float *oy){
    if(c->tf_active){
        *ox=c->tf[0]*x+c->tf[1]*y+c->tf[2];
        *oy=c->tf[3]*x+c->tf[4]*y+c->tf[5];
    } else { *ox=x; *oy=y; }
}
static int ctm_inv(const TS_Canvas *c,float *inv /*6: a b c d e f -> ux=a*X+c*Y+e*/){
    float a=c->tf_active?c->tf[0]:1, b=c->tf_active?c->tf[1]:0, e=c->tf_active?c->tf[2]:0;
    float d=c->tf_active?c->tf[3]:0, ee=c->tf_active?c->tf[4]:1, f=c->tf_active?c->tf[5]:0;
    float det=a*ee-b*d; if(fabsf(det)<1e-12f) return 0;
    float id=1.f/det;
    inv[0]= ee*id; inv[1]=-b*id; inv[2]=-d*id; inv[3]= a*id;
    inv[4]=-(inv[0]*e+inv[1]*f); inv[5]=-(inv[2]*e+inv[3]*f);
    return 1;
}
static float ctm_scale_factor(const TS_Canvas *c){
    if(!c->tf_active) return 1.f;
    float det=c->tf[0]*c->tf[4]-c->tf[1]*c->tf[3];
    det=fabsf(det); return det>1e-12f?sqrtf(det):1.f;
}

/* ----------------------------------------------------------------- path */
typedef struct { float x,y; } Pt;
typedef struct {
    Pt   *pts;  int npts,cap;
    int  *cs;   int *cl; int *cc; int ncont,ccap; /* contur start/len/closed */
    int   cur_open;      /* contur curent are puncte? */
    float tol_sq;        /* toleranta aplatizare (dist^2, spatiu-user) */
    float cx,cy;         /* ultimul punct */
} Path;

TS_API Path *ts_path2_new(void){
    Path *p=(Path*)calloc(1,sizeof(Path));
    if(p) p->tol_sq=0.09f;
    return p;
}
TS_API void ts_path2_free(Path *p){ if(p){ free(p->pts); free(p->cs); free(p->cl); free(p->cc); free(p);} }
TS_API void ts_path2_reset(Path *p){ if(p){ p->npts=0; p->ncont=0; p->cur_open=0; } }
TS_API void ts_path2_set_tolerance(Path *p,float tol_sq){ if(p){ p->tol_sq=(tol_sq>1e-7f)?tol_sq:1e-7f; } }

static void path_push_pt(Path *p,float x,float y){
    if(p->npts>=p->cap){ p->cap=p->cap?p->cap*2:64; p->pts=(Pt*)realloc(p->pts,p->cap*sizeof(Pt)); }
    p->pts[p->npts].x=x; p->pts[p->npts].y=y; p->npts++;
    p->cl[p->ncont-1]++; p->cx=x; p->cy=y;
}
static void path_new_contour(Path *p,float x,float y){
    if(p->ncont>=p->ccap){ p->ccap=p->ccap?p->ccap*2:16;
        p->cs=(int*)realloc(p->cs,p->ccap*sizeof(int));
        p->cl=(int*)realloc(p->cl,p->ccap*sizeof(int));
        p->cc=(int*)realloc(p->cc,p->ccap*sizeof(int)); }
    p->cs[p->ncont]=p->npts; p->cl[p->ncont]=0; p->cc[p->ncont]=0; p->ncont++;
    p->cur_open=1;
    path_push_pt(p,x,y);
}
TS_API void ts_path2_move_to(Path *p,float x,float y){ if(p) path_new_contour(p,x,y); }
TS_API void ts_path2_line_to(Path *p,float x,float y){
    if(!p) return;
    if(!p->cur_open){ path_new_contour(p,x,y); return; }
    path_push_pt(p,x,y);
}
TS_API void ts_path2_close(Path *p){
    if(!p||!p->ncont) return;
    p->cc[p->ncont-1]=1; p->cur_open=0;
}
/* cubic adaptiv */
static void flatten_cubic(Path *p,float x0,float y0,float x1,float y1,
                          float x2,float y2,float x3,float y3,int depth){
    /* platitudine: distanta puncte de control la coarda */
    float dx=x3-x0, dy=y3-y0;
    float d1=fabsf((x1-x3)*dy-(y1-y3)*dx);
    float d2=fabsf((x2-x3)*dy-(y2-y3)*dx);
    float len2=dx*dx+dy*dy;
    if(depth>=24 || (d1+d2)*(d1+d2) <= p->tol_sq*(len2>1e-12f?len2:1e-12f)){
        path_push_pt(p,x3,y3); return;
    }
    float x01=(x0+x1)*.5f,y01=(y0+y1)*.5f, x12=(x1+x2)*.5f,y12=(y1+y2)*.5f, x23=(x2+x3)*.5f,y23=(y2+y3)*.5f;
    float xa=(x01+x12)*.5f,ya=(y01+y12)*.5f, xb=(x12+x23)*.5f,yb=(y12+y23)*.5f;
    float xm=(xa+xb)*.5f,ym=(ya+yb)*.5f;
    flatten_cubic(p,x0,y0,x01,y01,xa,ya,xm,ym,depth+1);
    flatten_cubic(p,xm,ym,xb,yb,x23,y23,x3,y3,depth+1);
}
TS_API void ts_path2_curve_to(Path *p,float x1,float y1,float x2,float y2,float x3,float y3){
    if(!p) return; if(!p->cur_open){ path_new_contour(p,x1,y1); }
    flatten_cubic(p,p->cx,p->cy,x1,y1,x2,y2,x3,y3,0);
}
static void flatten_quad(Path *p,float x0,float y0,float x1,float y1,float x2,float y2,int depth){
    float dx=x2-x0,dy=y2-y0;
    float d=fabsf((x1-x2)*dy-(y1-y2)*dx);
    float len2=dx*dx+dy*dy;
    if(depth>=24 || d*d <= p->tol_sq*(len2>1e-12f?len2:1e-12f)){ path_push_pt(p,x2,y2); return; }
    float x01=(x0+x1)*.5f,y01=(y0+y1)*.5f,x12=(x1+x2)*.5f,y12=(y1+y2)*.5f;
    float xm=(x01+x12)*.5f,ym=(y01+y12)*.5f;
    flatten_quad(p,x0,y0,x01,y01,xm,ym,depth+1);
    flatten_quad(p,xm,ym,x12,y12,x2,y2,depth+1);
}
TS_API void ts_path2_quad_to(Path *p,float x1,float y1,float x2,float y2){
    if(!p) return; if(!p->cur_open){ path_new_contour(p,x1,y1); }
    flatten_quad(p,p->cx,p->cy,x1,y1,x2,y2,0);
}
TS_API void ts_path2_arc(Path *p,float cx,float cy,float r,float a0,float a1){
    if(!p) return;
    float sx=cx+r*cosf(a0), sy=cy+r*sinf(a0);
    if(!p->cur_open) path_new_contour(p,sx,sy); else path_push_pt(p,sx,sy);
    if(a1<a0) a1+=(float)(2*M_PI);
    float sweep=a1-a0;
    float tol=sqrtf(p->tol_sq); if(tol<1e-3f) tol=1e-3f;
    /* pas unghiular din toleranta pe raza */
    float step=2.f*acosf(fmaxf(0.f,1.f-tol/(r>1e-3f?r:1e-3f)));
    if(!(step>1e-3f)) step=0.2f;
    int n=(int)ceilf(sweep/step); if(n<2) n=2; if(n>4096) n=4096;
    for(int i=1;i<=n;i++){
        float a=a0+sweep*((float)i/n);
        path_push_pt(p,cx+r*cosf(a),cy+r*sinf(a));
    }
}

/* ----------------------------------------------------------------- surse */
typedef struct {
    int type; uint32_t solid;        /* 0=solid 1=linear 2=radial 3=image */
    float x0,y0,x1,y1,r1; int nstops;
    float stop_off[16]; uint32_t stop_col[16];
    int extend; void* img;
    float ix,iy; float pm[6]; int pm_active;
} TSSource;
enum { SRC_PAD=0, SRC_REPEAT=1, SRC_REFLECT=2 };

static float extend_t(float t,int ex){
    if(ex==SRC_REPEAT){ t=t-floorf(t); }
    else if(ex==SRC_REFLECT){ t=fabsf(t); float m=fmodf(t,2.f); t=(m<=1.f)?m:2.f-m; }
    else { if(t<0)t=0; if(t>1)t=1; }
    return t;
}
static uint32_t grad_color(const TSSource*s,float t){
    t=extend_t(t,s->extend);
    if(s->nstops<=0) return pack_rgba(0,0,0,255);
    if(t<=s->stop_off[0]) return s->stop_col[0];
    if(t>=s->stop_off[s->nstops-1]) return s->stop_col[s->nstops-1];
    for(int i=1;i<s->nstops;i++){
        if(t<=s->stop_off[i]){
            float a=s->stop_off[i-1],b=s->stop_off[i];
            float u=(b>a)?(t-a)/(b-a):0.f;
            uint32_t c0=s->stop_col[i-1],c1=s->stop_col[i];
            int r=(int)(CR(c0)+(CR(c1)-CR(c0))*u+.5f);
            int g=(int)(CG(c0)+(CG(c1)-CG(c0))*u+.5f);
            int bch=(int)(CB(c0)+(CB(c1)-CB(c0))*u+.5f);
            int al=(int)(CA(c0)+(CA(c1)-CA(c0))*u+.5f);
            return pack_rgba(r,g,bch,al);
        }
    }
    return s->stop_col[s->nstops-1];
}
/* evalueaza sursa la coord USER (gx,gy) */
static uint32_t src_eval(const TSSource*s,float gx,float gy){
    if(!s||s->type==0) return s?s->solid:0;
    float px=gx,py=gy;
    if(s->pm_active){ float x=gx,y=gy;
        px=s->pm[0]*x+s->pm[2]*y+s->pm[4];
        py=s->pm[1]*x+s->pm[3]*y+s->pm[5]; }
    if(s->type==1){
        float dx=s->x1-s->x0,dy=s->y1-s->y0; float dd=dx*dx+dy*dy;
        float t=(dd>1e-12f)?((px-s->x0)*dx+(py-s->y0)*dy)/dd:0.f;
        return grad_color(s,t);
    } else if(s->type==2){
        float dx=px-s->x0,dy=py-s->y0; float d=sqrtf(dx*dx+dy*dy);
        float t=(s->r1>1e-6f)?d/s->r1:0.f;
        return grad_color(s,t);
    } else if(s->type==3 && s->img){
        TS_Canvas*im=(TS_Canvas*)s->img;
        float fx=px-s->ix, fy=py-s->iy;
        /* esantionare biliniara cu extend */
        float gxx=fx,gyy=fy;
        int W=im->width,H=im->height;
        if(s->extend==SRC_REPEAT){ gxx=fmodf(fx,(float)W); if(gxx<0)gxx+=W; gyy=fmodf(fy,(float)H); if(gyy<0)gyy+=H; }
        else { if(gxx<0)gxx=0; if(gxx>W-1)gxx=W-1; if(gyy<0)gyy=0; if(gyy>H-1)gyy=H-1; }
        int x0=(int)floorf(gxx), y0=(int)floorf(gyy);
        float fxp=gxx-x0, fyp=gyy-y0;
        int x1=x0+1,y1=y0+1;
        if(s->extend==SRC_REPEAT){ x1%=W; y1%=H; if(x1<0)x1+=W; if(y1<0)y1+=H; }
        else { if(x1>W-1)x1=W-1; if(y1>H-1)y1=H-1; }
        if(x0<0)x0=0; if(y0<0)y0=0; if(x0>W-1)x0=W-1; if(y0>H-1)y0=H-1;
        uint32_t*ip=(uint32_t*)im->pixels;
        uint32_t c00=ip[(size_t)y0*W+x0],c10=ip[(size_t)y0*W+x1];
        uint32_t c01=ip[(size_t)y1*W+x0],c11=ip[(size_t)y1*W+x1];
        #define LERP4(ch) ((int)((ch(c00)*(1-fxp)+ch(c10)*fxp)*(1-fyp)+(ch(c01)*(1-fxp)+ch(c11)*fxp)*fyp+.5f))
        return pack_rgba(LERP4(CR),LERP4(CG),LERP4(CB),LERP4(CA));
        #undef LERP4
    }
    return s->solid;
}

/* ------------------------------------------------- rasterizer AA scanline */
/* acumuleaza acoperire [0..1] intr-un buffer peste regiunea (ox,oy,aw,ah).
 * contururi in spatiu-DISPOZITIV. combine: max in buffer. */
#define SS 4
typedef struct { float x0,y0,x1,y1; } Seg;

static void raster_region(float*acc,int ox,int oy,int aw,int ah,
                          const Pt*pts,const int*cs,const int*cl,const int*cc,int nc,
                          int rule){
    /* construieste muchii (inchide fiecare contur) */
    int maxE=0; for(int k=0;k<nc;k++) maxE+=cl[k]+1;
    if(maxE<=0) return;
    Seg*E=(Seg*)malloc(sizeof(Seg)*maxE); int ne=0;
    for(int k=0;k<nc;k++){
        int st=cs[k],ln=cl[k]; if(ln<2) continue;
        for(int i=0;i<ln;i++){
            Pt a=pts[st+i], b=pts[st+(i+1)%ln];
            if(a.y!=b.y){ E[ne].x0=a.x;E[ne].y0=a.y;E[ne].x1=b.x;E[ne].y1=b.y; ne++; }
        }
    }
    if(!ne){ free(E); return; }
    float*xs=(float*)malloc(sizeof(float)*ne);
    int  *wd=(int*)malloc(sizeof(int)*ne);
    for(int row=0;row<ah;row++){
        int py=oy+row;
        float*cov=acc+(size_t)row*aw;
        for(int s=0;s<SS;s++){
            float sy=py+(s+0.5f)/SS;
            int m=0;
            for(int e=0;e<ne;e++){
                float ya=E[e].y0,yb=E[e].y1; int w=1;
                float xa=E[e].x0,xb=E[e].x1;
                if(ya>yb){ float t; t=ya;ya=yb;yb=t; t=xa;xa=xb;xb=t; w=-1; }
                if(sy<ya||sy>=yb) continue;
                float x=xa+(xb-xa)*(sy-ya)/(yb-ya);
                xs[m]=x; wd[m]=w; m++;
            }
            if(m<2) continue;
            /* sort crossings by x (insertion) */
            for(int i=1;i<m;i++){ float kx=xs[i]; int kw=wd[i]; int j=i-1;
                while(j>=0&&xs[j]>kx){ xs[j+1]=xs[j]; wd[j+1]=wd[j]; j--; } xs[j+1]=kx; wd[j+1]=kw; }
            int wind=0;
            for(int i=0;i<m-1;i++){
                wind+=wd[i];
                int inside = (rule==1)? (wind&1) : (wind!=0);
                if(!inside) continue;
                float xa2=xs[i]-ox, xb2=xs[i+1]-ox;
                if(xb2<=0||xa2>=aw) continue;
                if(xa2<0)xa2=0; if(xb2>aw)xb2=aw;
                if(xb2<=xa2) continue;
                int ixa=(int)floorf(xa2), ixb=(int)floorf(xb2);
                float wgt=1.f/SS;
                if(ixa==ixb){ cov[ixa]+=wgt*(xb2-xa2); }
                else {
                    cov[ixa]+=wgt*((ixa+1)-xa2);
                    for(int px=ixa+1;px<ixb;px++) cov[px]+=wgt;
                    if(ixb<aw) cov[ixb]+=wgt*(xb2-ixb);
                }
            }
        }
    }
    free(xs); free(wd); free(E);
}

/* bbox dispozitiv al contururilor (dupa CTM aplicat deja in pts) */
static void dev_bbox(const Pt*pts,const int*cs,const int*cl,int nc,
                     float*x0,float*y0,float*x1,float*y1){
    *x0=1e30f;*y0=1e30f;*x1=-1e30f;*y1=-1e30f;
    for(int k=0;k<nc;k++){ int st=cs[k],ln=cl[k];
        for(int i=0;i<ln;i++){ Pt q=pts[st+i];
            if(q.x<*x0)*x0=q.x; if(q.y<*y0)*y0=q.y; if(q.x>*x1)*x1=q.x; if(q.y>*y1)*y1=q.y; } }
}

/* transforma path user->device intr-un buffer de puncte */
static Pt* path_to_device(const TS_Canvas*cv,const Path*p,int**cs,int**cl,int**cc,int*nc){
    *nc=p->ncont;
    Pt*out=(Pt*)malloc(sizeof(Pt)*(p->npts>0?p->npts:1));
    *cs=(int*)malloc(sizeof(int)*(p->ncont>0?p->ncont:1));
    *cl=(int*)malloc(sizeof(int)*(p->ncont>0?p->ncont:1));
    *cc=(int*)malloc(sizeof(int)*(p->ncont>0?p->ncont:1));
    for(int k=0;k<p->ncont;k++){ (*cs)[k]=p->cs[k]; (*cl)[k]=p->cl[k]; (*cc)[k]=p->cc[k]; }
    for(int i=0;i<p->npts;i++){ float ox,oy; ctm_apply(cv,p->pts[i].x,p->pts[i].y,&ox,&oy); out[i].x=ox;out[i].y=oy; }
    return out;
}

/* paint pass: blend acc peste canvas folosind sursa (NULL => solid col) */
static void blend_region(TS_Canvas*cv,const float*acc,int ox,int oy,int aw,int ah,
                         uint32_t solid,const TSSource*src){
    float inv[6]; int have_inv=ctm_inv(cv,inv);
    int cx0=0,cy0=0,cx1=cv->width,cy1=cv->height;
    if(cv->clip_enabled){ if(cv->clip_x>cx0)cx0=cv->clip_x; if(cv->clip_y>cy0)cy0=cv->clip_y;
        if(cv->clip_x+cv->clip_w<cx1)cx1=cv->clip_x+cv->clip_w; if(cv->clip_y+cv->clip_h<cy1)cy1=cv->clip_y+cv->clip_h; }
    float op=cv->opacity; if(op<0)op=0; if(op>1)op=1;
    int sr=CR(solid),sg=CG(solid),sb=CB(solid); float sa=CA(solid)/255.f;
    for(int row=0;row<ah;row++){
        int py=oy+row; if(py<cy0||py>=cy1) continue;
        const float*cov=acc+(size_t)row*aw;
        for(int col=0;col<aw;col++){
            float c=cov[col]; if(c<=0.001f) continue; if(c>1.f)c=1.f;
            int px=ox+col; if(px<cx0||px>=cx1) continue;
            int r,g,b; float a;
            if(src){
                float ux=px+0.5f, uy=py+0.5f, gx,gy;
                if(have_inv){ gx=inv[0]*ux+inv[1]*uy+inv[4]; gy=inv[2]*ux+inv[3]*uy+inv[5]; }
                else { gx=ux; gy=uy; }
                uint32_t pc=src_eval(src,gx,gy);
                r=CR(pc);g=CG(pc);b=CB(pc);a=CA(pc)/255.f;
            } else { r=sr;g=sg;b=sb;a=sa; }
            blend_px(cv,px,py,r,g,b,a*c*op);
        }
    }
}

/* umplere comuna */
static void do_fill(TS_Canvas*cv,const Path*p,int rule,uint32_t solid,const TSSource*src){
    if(!cv||!p||p->npts<3) return;
    int *cs,*cl,*cc,nc; Pt*dp=path_to_device(cv,p,&cs,&cl,&cc,&nc);
    float bx0,by0,bx1,by1; dev_bbox(dp,cs,cl,nc,&bx0,&by0,&bx1,&by1);
    int ox=(int)floorf(bx0)-1, oy=(int)floorf(by0)-1;
    int ex=(int)ceilf(bx1)+1, ey=(int)ceilf(by1)+1;
    if(ox<0)ox=0; if(oy<0)oy=0; if(ex>cv->width)ex=cv->width; if(ey>cv->height)ey=cv->height;
    int aw=ex-ox, ah=ey-oy;
    if(aw<=0||ah<=0){ free(dp);free(cs);free(cl);free(cc); return; }
    float*acc=(float*)calloc((size_t)aw*ah,sizeof(float));
    raster_region(acc,ox,oy,aw,ah,dp,cs,cl,cc,nc,rule);
    blend_region(cv,acc,ox,oy,aw,ah,solid,src);
    free(acc); free(dp); free(cs); free(cl); free(cc);
}
TS_API void ts_path2_fill(TS_Canvas*cv,Path*p,int rule){
    if(cv) do_fill(cv,p,rule,cv->fill_color,NULL);
}
TS_API void ts_path2_fill_src(TS_Canvas*cv,Path*p,int rule,TSSource*src){
    if(cv) do_fill(cv,p,rule,cv->fill_color,src);
}

/* ------------------------------------------------- contur (stroke) */
/* adauga un poligon convex (device) in masca prin rasterizer (max) */
static void mask_add_poly(float*mask,int W,int H,const Pt*poly,int n){
    if(n<3) return;
    float x0=1e30f,y0=1e30f,x1=-1e30f,y1=-1e30f;
    for(int i=0;i<n;i++){ if(poly[i].x<x0)x0=poly[i].x; if(poly[i].y<y0)y0=poly[i].y;
        if(poly[i].x>x1)x1=poly[i].x; if(poly[i].y>y1)y1=poly[i].y; }
    int ox=(int)floorf(x0)-1,oy=(int)floorf(y0)-1,ex=(int)ceilf(x1)+1,ey=(int)ceilf(y1)+1;
    if(ox<0)ox=0; if(oy<0)oy=0; if(ex>W)ex=W; if(ey>H)ey=H;
    int aw=ex-ox,ah=ey-oy; if(aw<=0||ah<=0) return;
    float*acc=(float*)calloc((size_t)aw*ah,sizeof(float));
    int cs=0, cl=n, cc=1;
    raster_region(acc,ox,oy,aw,ah,poly,&cs,&cl,&cc,1,0);
    for(int r=0;r<ah;r++)for(int c=0;c<aw;c++){
        float v=acc[(size_t)r*aw+c]; if(v>1.f)v=1.f;
        float*m=&mask[(size_t)(oy+r)*W+(ox+c)]; if(v>*m)*m=v;
    }
    free(acc);
}
static void add_disc(float*mask,int W,int H,float cx,float cy,float r){
    int n=(int)(r*1.2f); if(n<10)n=10; if(n>180)n=180;
    Pt*poly=(Pt*)malloc(sizeof(Pt)*n);
    for(int i=0;i<n;i++){ float a=(float)(2*M_PI)*i/n; poly[i].x=cx+r*cosf(a); poly[i].y=cy+r*sinf(a); }
    mask_add_poly(mask,W,H,poly,n); free(poly);
}
/* un segment gros ca dreptunghi + capete */
static void add_segment(float*mask,int W,int H,float ax,float ay,float bx,float by,
                        float h,int cap){
    float dx=bx-ax,dy=by-ay,len=sqrtf(dx*dx+dy*dy);
    if(len<1e-6f){ if(cap==1) add_disc(mask,W,H,ax,ay,h); return; }
    float nx=-dy/len, ny=dx/len;      /* normala */
    float ux=dx/len, uy=dy/len;       /* directie */
    float ex=0,ey=0;
    if(cap==2){ ex=ux*h; ey=uy*h; }   /* square: extinde */
    Pt poly[4];
    poly[0].x=ax+nx*h-ex; poly[0].y=ay+ny*h-ey;
    poly[1].x=bx+nx*h+ex; poly[1].y=by+ny*h+ey;
    poly[2].x=bx-nx*h+ex; poly[2].y=by-ny*h+ey;
    poly[3].x=ax-nx*h-ex; poly[3].y=ay-ny*h-ey;
    mask_add_poly(mask,W,H,poly,4);
    if(cap==1){ add_disc(mask,W,H,ax,ay,h); add_disc(mask,W,H,bx,by,h); }
}
static void add_join(float*mask,int W,int H,float px,float py,
                     float ax,float ay,float bx,float by,float h,int join,float miter){
    if(join==1){ add_disc(mask,W,H,px,py,h); return; } /* round */
    /* vectori directii in/afara */
    float d0x=px-ax,d0y=py-ay,l0=sqrtf(d0x*d0x+d0y*d0y);
    float d1x=bx-px,d1y=by-py,l1=sqrtf(d1x*d1x+d1y*d1y);
    if(l0<1e-6f||l1<1e-6f) return;
    float n0x=-d0y/l0,n0y=d0x/l0, n1x=-d1y/l1,n1y=d1x/l1;
    /* bevel: triunghiuri intre cele doua laturi, ambele semne */
    for(int sgn=-1;sgn<=1;sgn+=2){
        Pt tri[3];
        tri[0].x=px+n0x*h*sgn; tri[0].y=py+n0y*h*sgn;
        tri[1].x=px+n1x*h*sgn; tri[1].y=py+n1y*h*sgn;
        tri[2].x=px; tri[2].y=py;
        mask_add_poly(mask,W,H,tri,3);
        if(join==0){ /* miter: varful */
            float mx=(n0x+n1x),my=(n0y+n1y); float ml=sqrtf(mx*mx+my*my);
            if(ml>1e-4f){ mx/=ml;my/=ml;
                float cosh2=(n0x*n1x+n0y*n1y); float denom=sqrtf(fmaxf(0.f,(1.f+cosh2)*0.5f));
                if(denom>1e-4f){ float mlen=h/denom;
                    if(mlen<=miter*h){ Pt mt[3];
                        mt[0].x=px+n0x*h*sgn; mt[0].y=py+n0y*h*sgn;
                        mt[1].x=px+n1x*h*sgn; mt[1].y=py+n1y*h*sgn;
                        mt[2].x=px+mx*mlen*sgn; mt[2].y=py+my*mlen*sgn;
                        mask_add_poly(mask,W,H,mt,3); } } }
        }
    }
}

/* cap patrat: dreptunghi care extinde capatul cu h in directia data */
static void add_square_cap(float*mask,int W,int H,float ex_,float ey_,float ux,float uy,float h){
    Pt poly[4]; float nx=-uy,ny=ux;
    poly[0].x=ex_+nx*h;          poly[0].y=ey_+ny*h;
    poly[1].x=ex_+nx*h+ux*h;     poly[1].y=ey_+ny*h+uy*h;
    poly[2].x=ex_-nx*h+ux*h;     poly[2].y=ey_-ny*h+uy*h;
    poly[3].x=ex_-nx*h;          poly[3].y=ey_-ny*h;
    mask_add_poly(mask,W,H,poly,4);
}
/* contur in spatiu-dispozitiv peste o polilinie device */
static void stroke_polyline_dev(float*mask,int W,int H,const Pt*q,int n,int closed,
                                float h,int cap,int join,float miter){
    if(n<1) return;
    if(n==1){ if(cap==1) add_disc(mask,W,H,q[0].x,q[0].y,h); return; }
    int segN = closed? n : n-1;
    /* segmente butt */
    for(int i=0;i<segN;i++){ Pt a=q[i], b=q[(i+1)%n]; add_segment(mask,W,H,a.x,a.y,b.x,b.y,h,0); }
    /* capete deschise */
    if(!closed){
        if(cap==1){ add_disc(mask,W,H,q[0].x,q[0].y,h); add_disc(mask,W,H,q[n-1].x,q[n-1].y,h); }
        else if(cap==2){
            float dx=q[0].x-q[1].x,dy=q[0].y-q[1].y,l=sqrtf(dx*dx+dy*dy);
            if(l>1e-6f) add_square_cap(mask,W,H,q[0].x,q[0].y,dx/l,dy/l,h);
            dx=q[n-1].x-q[n-2].x;dy=q[n-1].y-q[n-2].y;l=sqrtf(dx*dx+dy*dy);
            if(l>1e-6f) add_square_cap(mask,W,H,q[n-1].x,q[n-1].y,dx/l,dy/l,h);
        }
    }
    /* joins la varfuri interioare */
    int jN = closed? n : n-1;
    for(int i=1;i<jN;i++){ Pt pa=q[i-1], pp=q[i], pb=q[(i+1)%n];
        add_join(mask,W,H,pp.x,pp.y,pa.x,pa.y,pb.x,pb.y,h,join,miter); }
    if(closed){ Pt pa=q[n-1],pp=q[0],pb=q[1]; add_join(mask,W,H,pp.x,pp.y,pa.x,pa.y,pb.x,pb.y,h,join,miter); }
}

/* core stroke: construieste masca si o picteaza o data */
static void do_stroke(TS_Canvas*cv,const Path*p,float width,int cap,int join,float miter,
                      const float*dashes,int ndash,float offset,int device,
                      uint32_t solid,const TSSource*src){
    if(!cv||!p||p->npts<1||width<=0.f) return;
    int W=cv->width,H=cv->height;
    float scale = device?1.f:ctm_scale_factor(cv);
    float h=(device?width:width*scale)*0.5f; if(h<0.05f)h=0.05f;
    float*mask=(float*)calloc((size_t)W*H,sizeof(float));
    if(!mask) return;
    /* pentru fiecare contur: transforma la device si traseaza */
    for(int k=0;k<p->ncont;k++){
        int st=p->cs[k],ln=p->cl[k]; if(ln<1) continue;
        Pt*q=(Pt*)malloc(sizeof(Pt)*ln);
        for(int i=0;i<ln;i++){ float ox,oy; ctm_apply(cv,p->pts[st+i].x,p->pts[st+i].y,&ox,&oy); q[i].x=ox;q[i].y=oy; }
        int closed=p->cc[k];
        if(ndash>0 && dashes){
            /* imparte in bucati "on" dupa pattern (lungimi scalate ca latimea) */
            float sum=0; for(int d=0;d<ndash;d++) sum+=dashes[d]; if(sum<=1e-4f){ free(q); continue; }
            float sc = device?1.f:scale;
            int npl = closed? ln+1 : ln; /* polilinie, inchisa => repeta primul */
            int di=0; float drem=dashes[0]*sc; int on=1;
            /* aplica offset */
            float off=offset*sc; while(off>0){ if(off<drem){ drem-=off; off=0; } else { off-=drem; di=(di+1)%ndash; drem=dashes[di]*sc; on=!on; } }
            Pt*cur=(Pt*)malloc(sizeof(Pt)*(npl+2)); int ncur=0;
            float px=q[0].x,py=q[0].y; if(on){ cur[ncur++]=q[0]; }
            for(int i=1;i<npl;i++){
                Pt a; a=q[(i-1)%ln]; Pt b=q[i%ln];
                float segx=b.x-a.x,segy=b.y-a.y,seglen=sqrtf(segx*segy*0+segx*segx+segy*segy);
                float t0=0;
                while(seglen-t0>drem){
                    t0+=drem; float tx=a.x+segx*(t0/seglen),ty=a.y+segy*(t0/seglen);
                    if(on){ cur[ncur++]=(Pt){tx,ty}; if(ncur>=2) stroke_polyline_dev(mask,W,H,cur,ncur,0,h,cap,join,miter); ncur=0; }
                    else { cur[ncur++]=(Pt){tx,ty}; }
                    on=!on; di=(di+1)%ndash; drem=dashes[di]*sc;
                    if(on){ ncur=0; cur[ncur++]=(Pt){tx,ty}; } else { ncur=0; }
                }
                drem-=(seglen-t0);
                if(on){ cur[ncur++]=b; }
            }
            if(on&&ncur>=2) stroke_polyline_dev(mask,W,H,cur,ncur,0,h,cap,join,miter);
            free(cur);
        } else {
            stroke_polyline_dev(mask,W,H,q,ln,closed,h,cap,join,miter);
        }
        free(q);
    }
    /* blend masca peste canvas */
    int cx0=0,cy0=0,cx1=W,cy1=H;
    if(cv->clip_enabled){ if(cv->clip_x>cx0)cx0=cv->clip_x; if(cv->clip_y>cy0)cy0=cv->clip_y;
        if(cv->clip_x+cv->clip_w<cx1)cx1=cv->clip_x+cv->clip_w; if(cv->clip_y+cv->clip_h<cy1)cy1=cv->clip_y+cv->clip_h; }
    float inv[6]; int have_inv=ctm_inv(cv,inv);
    float op=cv->opacity; if(op<0)op=0; if(op>1)op=1;
    int sr=CR(solid),sg=CG(solid),sb=CB(solid); float sa=CA(solid)/255.f;
    for(int y=cy0;y<cy1;y++)for(int x=cx0;x<cx1;x++){
        float c=mask[(size_t)y*W+x]; if(c<=0.001f) continue; if(c>1.f)c=1.f;
        int r,g,b; float a;
        if(src){ float ux=x+0.5f,uy=y+0.5f,gx,gy;
            if(have_inv){ gx=inv[0]*ux+inv[1]*uy+inv[4]; gy=inv[2]*ux+inv[3]*uy+inv[5]; } else {gx=ux;gy=uy;}
            uint32_t pc=src_eval(src,gx,gy); r=CR(pc);g=CG(pc);b=CB(pc);a=CA(pc)/255.f;
        } else { r=sr;g=sg;b=sb;a=sa; }
        blend_px(cv,x,y,r,g,b,a*c*op);
    }
    free(mask);
}
TS_API void ts_path2_stroke(TS_Canvas*cv,Path*p,float width,int cap,int join,float miter){
    if(cv) do_stroke(cv,p,width,cap,join,miter,NULL,0,0.f,0,cv->fill_color,NULL);
}
TS_API void ts_path2_stroke_device(TS_Canvas*cv,Path*p,float width,int cap,int join,float miter){
    if(cv) do_stroke(cv,p,width,cap,join,miter,NULL,0,0.f,1,cv->fill_color,NULL);
}
TS_API void ts_path2_stroke_src(TS_Canvas*cv,Path*p,float width,int cap,int join,float miter,TSSource*src){
    if(cv) do_stroke(cv,p,width,cap,join,miter,NULL,0,0.f,0,cv->fill_color,src);
}
TS_API void ts_path2_stroke_dashed(TS_Canvas*cv,Path*p,float width,int cap,int join,float miter,
                                   const float*dashes,int ndash,float offset){
    if(cv) do_stroke(cv,p,width,cap,join,miter,dashes,ndash,offset,0,cv->fill_color,NULL);
}
TS_API void ts_path2_stroke_dashed_device(TS_Canvas*cv,Path*p,float width,int cap,int join,float miter,
                                          const float*dashes,int ndash,float offset){
    if(cv) do_stroke(cv,p,width,cap,join,miter,dashes,ndash,offset,1,cv->fill_color,NULL);
}

/* ------------------------------------------------- TrueType text-as-path */
static uint16_t be16(const uint8_t*d){ return (uint16_t)((d[0]<<8)|d[1]); }
static int16_t  bei16(const uint8_t*d){ return (int16_t)((d[0]<<8)|d[1]); }
static uint32_t be32(const uint8_t*d){ return ((uint32_t)d[0]<<24)|((uint32_t)d[1]<<16)|((uint32_t)d[2]<<8)|d[3]; }

typedef struct {
    const uint8_t*d;
    uint32_t glyf,loca,head,maxp,hhea,hmtx,cmap;
    int unitsPerEm, locLong, numGlyphs, numHMetrics;
} TTF;

static uint32_t ttf_table(const uint8_t*d,const char*tag){
    uint32_t off=0; uint32_t ver=be32(d);
    if(ver==0x74746366u){ off=be32(d+12); } /* ttcf -> prima fonta */
    uint16_t num=be16(d+off+4);
    const uint8_t*rec=d+off+12;
    for(int i=0;i<num;i++,rec+=16){
        if(rec[0]==tag[0]&&rec[1]==tag[1]&&rec[2]==tag[2]&&rec[3]==tag[3]) return be32(rec+8);
    }
    return 0;
}
static int ttf_init(TTF*t,const uint8_t*d){
    t->d=d; t->head=ttf_table(d,"head"); t->maxp=ttf_table(d,"maxp");
    t->loca=ttf_table(d,"loca"); t->glyf=ttf_table(d,"glyf");
    t->hhea=ttf_table(d,"hhea"); t->hmtx=ttf_table(d,"hmtx"); t->cmap=ttf_table(d,"cmap");
    if(!t->head||!t->maxp||!t->loca||!t->glyf) return 0;
    t->unitsPerEm=be16(d+t->head+18); if(t->unitsPerEm<=0)t->unitsPerEm=1000;
    t->locLong=bei16(d+t->head+50);
    t->numGlyphs=be16(d+t->maxp+4);
    t->numHMetrics=t->hhea?be16(d+t->hhea+34):0;
    return 1;
}
static uint32_t loca_off(const TTF*t,int gid){
    if(t->locLong) return be32(t->d+t->loca+4*gid);
    return 2u*be16(t->d+t->loca+2*gid);
}
static int ttf_advance(const TTF*t,int gid){
    if(!t->hmtx||t->numHMetrics<=0) return t->unitsPerEm/2;
    int i=gid<t->numHMetrics?gid:t->numHMetrics-1;
    return be16(t->d+t->hmtx+4*i);
}
static int cmap_lookup(const TTF*t,uint32_t cp){
    if(!t->cmap) return cp<t->numGlyphs?cp:0;
    const uint8_t*c=t->d+t->cmap; uint16_t ntab=be16(c+2);
    uint32_t best=0; int bestScore=-1;
    for(int i=0;i<ntab;i++){
        uint16_t pid=be16(c+4+i*8), eid=be16(c+4+i*8+2); uint32_t off=be32(c+4+i*8+4);
        int score=0;
        if(pid==3&&eid==10)score=5; else if(pid==3&&eid==1)score=4;
        else if(pid==0)score=3; else if(pid==3&&eid==0)score=1; else score=0;
        if(score>bestScore){ bestScore=score; best=off; }
    }
    if(!best) return cp<t->numGlyphs?cp:0;
    const uint8_t*s=c+best; uint16_t fmt=be16(s);
    if(fmt==4){
        uint16_t segX2=be16(s+6); int segc=segX2/2;
        const uint8_t*endC=s+14; const uint8_t*startC=endC+segX2+2;
        const uint8_t*idD=startC+segX2; const uint8_t*idR=idD+segX2;
        for(int i=0;i<segc;i++){
            uint16_t end=be16(endC+2*i);
            if(cp<=end){
                uint16_t start=be16(startC+2*i);
                if(cp<start) return 0;
                int16_t delta=bei16(idD+2*i); uint16_t ro=be16(idR+2*i);
                if(ro==0) return (uint16_t)(cp+delta);
                const uint8_t*gp=idR+2*i+ro+2*(cp-start);
                uint16_t g=be16(gp); if(g==0)return 0; return (uint16_t)(g+delta);
            }
        }
        return 0;
    } else if(fmt==12){
        uint32_t ng=be32(s+12); const uint8_t*grp=s+16;
        for(uint32_t i=0;i<ng;i++,grp+=12){
            uint32_t sc=be32(grp),ec=be32(grp+4),sg=be32(grp+8);
            if(cp>=sc&&cp<=ec) return sg+(cp-sc);
        }
        return 0;
    } else if(fmt==6){
        uint16_t first=be16(s+6),cnt=be16(s+8);
        if(cp>=first&&cp<first+cnt) return be16(s+10+2*(cp-first));
        return 0;
    } else if(fmt==0){
        if(cp<256) return s[6+cp];
        return 0;
    }
    return cp<t->numGlyphs?cp:0;
}

/* emite conturul glyph-ului (recursiv pt composite) in path, scalat + translatat */
static void glyph_outline(const TTF*t,int gid,Path*p,float penx,float basey,float sc,int depth){
    if(gid<0||gid>=t->numGlyphs||depth>6) return;
    uint32_t o0=loca_off(t,gid),o1=loca_off(t,gid+1); if(o1<=o0) return;
    const uint8_t*g=t->d+t->glyf+o0; int16_t nc=bei16(g);
    if(nc<0){ /* composite */
        const uint8_t*cp=g+10;
        for(;;){
            uint16_t flags=be16(cp); uint16_t cgid=be16(cp+2); cp+=4;
            float dx,dy;
            if(flags&0x0001){ dx=bei16(cp); dy=bei16(cp+2); cp+=4; }
            else { dx=(int8_t)cp[0]; dy=(int8_t)cp[1]; cp+=2; }
            float a=1,b2=0,c2=0,d2=1;
            if(flags&0x0008){ a=d2=bei16(cp)/16384.f; cp+=2; }
            else if(flags&0x0040){ a=bei16(cp)/16384.f; d2=bei16(cp+2)/16384.f; cp+=4; }
            else if(flags&0x0080){ a=bei16(cp)/16384.f; b2=bei16(cp+2)/16384.f; c2=bei16(cp+4)/16384.f; d2=bei16(cp+6)/16384.f; cp+=8; }
            /* doar offset XY (ARGS_ARE_XY_VALUES) suportat pentru pozitie */
            float addx = (flags&0x0002)? dx : 0;
            float addy = (flags&0x0002)? dy : 0;
            (void)a;(void)b2;(void)c2;(void)d2;
            glyph_outline(t,cgid,p,penx+addx*sc,basey-addy*sc*0 ,sc,depth+1);
            /* nota: offset-ul pe y se aplica in spatiul glyph; basey ramane */
            if(!(flags&0x0020)) break; /* MORE_COMPONENTS */
        }
        return;
    }
    int numPts=be16(g+10+2*(nc-1))+1;
    const uint8_t*pp=g+10+2*nc;
    uint16_t instr=be16(pp); pp+=2+instr;
    uint8_t*flags=(uint8_t*)malloc(numPts);
    for(int i=0;i<numPts;){ uint8_t f=*pp++; flags[i++]=f; if(f&0x08){ uint8_t r=*pp++; while(r--&&i<numPts) flags[i++]=f; } }
    int*xs=(int*)malloc(sizeof(int)*numPts); int*ys=(int*)malloc(sizeof(int)*numPts);
    int xv=0;
    for(int i=0;i<numPts;i++){ uint8_t f=flags[i];
        if(f&0x02){ uint8_t v=*pp++; xv+= (f&0x10)? v : -v; }
        else if(!(f&0x10)){ xv+=bei16(pp); pp+=2; }
        xs[i]=xv; }
    int yv=0;
    for(int i=0;i<numPts;i++){ uint8_t f=flags[i];
        if(f&0x04){ uint8_t v=*pp++; yv+= (f&0x20)? v : -v; }
        else if(!(f&0x20)){ yv+=bei16(pp); pp+=2; }
        ys[i]=yv; }
    /* parcurge contururi */
    int start=0;
    for(int c=0;c<nc;c++){
        int end=be16(g+10+2*c);
        int n=end-start+1; if(n<=0){ start=end+1; continue; }
        #define GX(i) (penx + xs[start+((i)%n)]*sc)
        #define GY(i) (basey - ys[start+((i)%n)]*sc)
        #define ON(i) (flags[start+((i)%n)]&1)
        /* gaseste primul on-curve */
        int s0=-1; for(int i=0;i<n;i++) if(ON(i)){ s0=i; break; }
        float sx,sy;
        if(s0<0){ /* toate off: start = mijloc intre 0 si 1 */
            sx=(GX(0)+GX(1))*.5f; sy=(GY(0)+GY(1))*.5f; s0=0;
            ts_path2_move_to(p,sx,sy);
            float cxp=GX(0),cyp=GY(0); float ex=(GX(0)+GX(1))*.5f,ey=(GY(0)+GY(1))*.5f;
            (void)cxp;(void)cyp;(void)ex;(void)ey;
        } else {
            sx=GX(s0); sy=GY(s0); ts_path2_move_to(p,sx,sy);
        }
        float prevx=sx,prevy=sy; int haveOff=0; float offx=0,offy=0;
        for(int k=1;k<=n;k++){
            int idx=s0+k; int on=ON(idx); float px2=GX(idx),py2=GY(idx);
            if(on){
                if(haveOff){ ts_path2_quad_to(p,offx,offy,px2,py2); haveOff=0; }
                else ts_path2_line_to(p,px2,py2);
                prevx=px2;prevy=py2;
            } else {
                if(haveOff){ float mx=(offx+px2)*.5f,my=(offy+py2)*.5f; ts_path2_quad_to(p,offx,offy,mx,my); prevx=mx;prevy=my; }
                offx=px2;offy=py2; haveOff=1;
            }
        }
        if(haveOff){ ts_path2_quad_to(p,offx,offy,sx,sy); }
        ts_path2_close(p);
        #undef GX
        #undef GY
        #undef ON
        start=end+1;
    }
    free(flags);free(xs);free(ys);
}

TS_API float ts_text_path_char(Path*p,const uint8_t*ttf,uint32_t cp,float x,float y,float size){
    if(!p||!ttf) return 0.f; TTF t; if(!ttf_init(&t,ttf)) return 0.f;
    int gid=cmap_lookup(&t,cp); float sc=size/(float)t.unitsPerEm;
    glyph_outline(&t,gid,p,x,y,sc,0);
    return ttf_advance(&t,gid)*sc;
}
TS_API float ts_text_path_string(Path*p,const uint8_t*ttf,const uint8_t*utf8,float x,float y,float size){
    if(!p||!ttf||!utf8) return 0.f; TTF t; if(!ttf_init(&t,ttf)) return 0.f;
    float sc=size/(float)t.unitsPerEm; float penx=x;
    const uint8_t*s=utf8;
    while(*s){
        uint32_t cp; const uint8_t*c=s;
        if(c[0]<0x80){ cp=c[0]; s+=1; }
        else if((c[0]&0xE0)==0xC0 && c[1]){ cp=((c[0]&0x1F)<<6)|(c[1]&0x3F); s+=2; }
        else if((c[0]&0xF0)==0xE0 && c[1]&&c[2]){ cp=((c[0]&0x0F)<<12)|((c[1]&0x3F)<<6)|(c[2]&0x3F); s+=3; }
        else if((c[0]&0xF8)==0xF0 && c[1]&&c[2]&&c[3]){ cp=((uint32_t)(c[0]&0x07)<<18)|((c[1]&0x3F)<<12)|((c[2]&0x3F)<<6)|(c[3]&0x3F); s+=4; }
        else { cp=c[0]; s+=1; }
        int gid=cmap_lookup(&t,cp);
        glyph_outline(&t,gid,p,penx,y,sc,0);
        penx+=ttf_advance(&t,gid)*sc;
    }
    return penx-x;
}

/* ------------------------------------------------- PNG (zlib stored, libc-only) */
static uint32_t crc_table[256]; static int crc_ready=0;
static void crc_init(void){ for(uint32_t n=0;n<256;n++){ uint32_t c=n; for(int k=0;k<8;k++) c=(c&1)?0xEDB88320u^(c>>1):(c>>1); crc_table[n]=c; } crc_ready=1; }
static uint32_t crc32_upd(uint32_t crc,const uint8_t*b,size_t n){ if(!crc_ready)crc_init(); crc^=0xFFFFFFFFu; for(size_t i=0;i<n;i++) crc=crc_table[(crc^b[i])&0xFF]^(crc>>8); return crc^0xFFFFFFFFu; }
static void put32(uint8_t*d,uint32_t v){ d[0]=v>>24;d[1]=v>>16;d[2]=v>>8;d[3]=v; }
static void png_chunk(FILE*f,const char*type,const uint8_t*data,uint32_t len){
    uint8_t hdr[8]; put32(hdr,len); memcpy(hdr+4,type,4); fwrite(hdr,1,8,f);
    if(len) fwrite(data,1,len,f);
    uint8_t*tmp=(uint8_t*)malloc(4+len); memcpy(tmp,type,4); if(len)memcpy(tmp+4,data,len);
    uint32_t c=crc32_upd(0,tmp,4+len); free(tmp);
    uint8_t cb[4]; put32(cb,c); fwrite(cb,1,4,f);
}
TS_API int ts_png_save(TS_Canvas*cv,const char*path){
    if(!cv||!cv->pixels||!path) return 1;
    FILE*f=fopen(path,"wb"); if(!f) return 1;
    int W=cv->width,H=cv->height;
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10}; fwrite(sig,1,8,f);
    uint8_t ihdr[13]; put32(ihdr,W); put32(ihdr+4,H); ihdr[8]=8; ihdr[9]=6; ihdr[10]=0;ihdr[11]=0;ihdr[12]=0;
    png_chunk(f,"IHDR",ihdr,13);
    /* raw = per rand: filtru(0) + W*4 bytes (RGBA = exact ordinea din buffer LE) */
    size_t rowbytes=(size_t)W*4; size_t rawlen=(rowbytes+1)*H;
    uint8_t*raw=(uint8_t*)malloc(rawlen); size_t ri=0;
    uint8_t*px=(uint8_t*)cv->pixels;
    for(int y=0;y<H;y++){ raw[ri++]=0; memcpy(raw+ri,px+(size_t)y*rowbytes,rowbytes); ri+=rowbytes; }
    /* zlib: CMF/FLG + blocuri stored + adler32 */
    uint32_t a1=1,a2=0; for(size_t i=0;i<rawlen;i++){ a1=(a1+raw[i])%65521; a2=(a2+a1)%65521; }
    uint32_t adler=(a2<<16)|a1;
    size_t maxz=2+rawlen+5*((rawlen/65535)+1)+4+16;
    uint8_t*z=(uint8_t*)malloc(maxz); size_t zi=0;
    z[zi++]=0x78; z[zi++]=0x01;
    size_t pos=0;
    while(pos<rawlen){ size_t chunk=rawlen-pos; if(chunk>65535)chunk=65535; int last=(pos+chunk>=rawlen);
        z[zi++]=last?1:0; z[zi++]=chunk&0xFF; z[zi++]=(chunk>>8)&0xFF; uint16_t nl=~(uint16_t)chunk; z[zi++]=nl&0xFF; z[zi++]=(nl>>8)&0xFF;
        memcpy(z+zi,raw+pos,chunk); zi+=chunk; pos+=chunk; }
    put32(z+zi,adler); zi+=4;
    png_chunk(f,"IDAT",z,(uint32_t)zi);
    png_chunk(f,"IEND",NULL,0);
    free(raw);free(z);fclose(f);
    return 0;
}
