/*
 * libtitangfx — motorul grafic deschis al ecosistemului SoftCAD (TVW/TitanView).
 * Copyright (C) 2026 SoftCAD C+C TOPO S.R.L.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details. You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */
/* ts_geo_extra2.c — implementare extensie, SELF-CONTAINED.
 * SHA-256 inline ca static (zero extern conflicts), shoelace inline pe int64.
 * Pentru amalgamare: #include "ts_geo_extra2.c" la sfîrșitul ts_engines_all.c
 */
#ifndef TS_BUILDING
#define TS_BUILDING
#endif
#include "ts_geo_extra2.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define SQRT3 1.7320508075688772
#define UM_PER_M 1000000.0

/* ── SHA-256 inline static (public domain FIPS 180-4) ── */
typedef struct { uint32_t st[8]; uint64_t bits; uint8_t buf[64]; int n; } sha2_ctx;
static const uint32_t K[64] = {
0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
static void sha2_compress(sha2_ctx* c, const uint8_t* b){
    uint32_t W[64], a,b_,cc,d,e,f,g,h,t1,t2;
    for(int i=0;i<16;i++) W[i]=(uint32_t)b[i*4]<<24|(uint32_t)b[i*4+1]<<16|(uint32_t)b[i*4+2]<<8|(uint32_t)b[i*4+3];
    for(int i=16;i<64;i++){uint32_t s0=((W[i-15]>>7)|(W[i-15]<<25))^((W[i-15]>>18)|(W[i-15]<<14))^(W[i-15]>>3);
        uint32_t s1=((W[i-2]>>17)|(W[i-2]<<15))^((W[i-2]>>19)|(W[i-2]<<13))^(W[i-2]>>10); W[i]=W[i-16]+s0+W[i-7]+s1;}
    a=c->st[0];b_=c->st[1];cc=c->st[2];d=c->st[3];e=c->st[4];f=c->st[5];g=c->st[6];h=c->st[7];
    for(int i=0;i<64;i++){uint32_t S1=((e>>6)|(e<<26))^((e>>11)|(e<<21))^((e>>25)|(e<<7));
        uint32_t ch=(e&f)^((~e)&g); t1=h+S1+ch+K[i]+W[i];
        uint32_t S0=((a>>2)|(a<<30))^((a>>13)|(a<<19))^((a>>22)|(a<<10));
        uint32_t mj=(a&b_)^(a&cc)^(b_&cc); t2=S0+mj;
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b_;b_=a;a=t1+t2;}
    c->st[0]+=a;c->st[1]+=b_;c->st[2]+=cc;c->st[3]+=d;c->st[4]+=e;c->st[5]+=f;c->st[6]+=g;c->st[7]+=h;
}
static void sha2_init(sha2_ctx* c){
    static const uint32_t H0[8]={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    memcpy(c->st,H0,32); c->bits=0; c->n=0;
}
static void sha2_update(sha2_ctx* c,const uint8_t* d,size_t n){
    c->bits+=(uint64_t)n*8;
    while(n){size_t k=64-c->n; if(k>n)k=n; memcpy(c->buf+c->n,d,k); c->n+=(int)k; d+=k; n-=k;
        if(c->n==64){sha2_compress(c,c->buf); c->n=0;}}
}
static void sha2_final(sha2_ctx* c,uint8_t out[32]){
    c->buf[c->n++]=0x80;
    if(c->n>56){while(c->n<64) c->buf[c->n++]=0; sha2_compress(c,c->buf); c->n=0;}
    while(c->n<56) c->buf[c->n++]=0;
    uint64_t b=c->bits;
    for(int i=7;i>=0;i--) c->buf[c->n++]=(uint8_t)(b>>(i*8));
    sha2_compress(c,c->buf);
    for(int i=0;i<8;i++){out[i*4]=(uint8_t)(c->st[i]>>24);out[i*4+1]=(uint8_t)(c->st[i]>>16);
        out[i*4+2]=(uint8_t)(c->st[i]>>8);out[i*4+3]=(uint8_t)c->st[i];}
}

static int _cmp32(const void* a, const void* b){ return memcmp(a, b, 32); }

int ts_geo_canonical_ring(const double* xs,const double* ys,int n,int64_t* ox,int64_t* oy){
    if(!xs||!ys||!ox||!oy||n<3) return -1;
    int64_t* qx=(int64_t*)malloc((size_t)n*sizeof(int64_t));
    int64_t* qy=(int64_t*)malloc((size_t)n*sizeof(int64_t));
    if(!qx||!qy){free(qx);free(qy);return -1;}
    for(int i=0;i<n;++i){qx[i]=(int64_t)llround(xs[i]*UM_PER_M); qy[i]=(int64_t)llround(ys[i]*UM_PER_M);}
    int64_t s=0;
    for(int i=0;i<n;++i){int j=(i+1)%n; s+=qx[i]*qy[j]-qx[j]*qy[i];}
    if(s<0){for(int i=0,j=n-1;i<j;++i,--j){int64_t tx=qx[i],ty=qy[i]; qx[i]=qx[j];qy[i]=qy[j]; qx[j]=tx;qy[j]=ty;}}
    int start=0;
    for(int i=1;i<n;++i) if(qx[i]<qx[start]||(qx[i]==qx[start]&&qy[i]<qy[start])) start=i;
    for(int i=0;i<n;++i){ox[i]=qx[(start+i)%n]; oy[i]=qy[(start+i)%n];}
    free(qx); free(qy); return 0;
}

int ts_geo_ring_hash(const double* xs,const double* ys,int n,uint8_t out_hash[32]){
    int64_t* cx=(int64_t*)malloc((size_t)n*sizeof(int64_t));
    int64_t* cy=(int64_t*)malloc((size_t)n*sizeof(int64_t));
    if(!cx||!cy){free(cx);free(cy);return -1;}
    if(ts_geo_canonical_ring(xs,ys,n,cx,cy)!=0){free(cx);free(cy);return -1;}
    sha2_ctx ctx; sha2_init(&ctx);
    for(int i=0;i<n;++i){sha2_update(&ctx,(uint8_t*)&cx[i],8); sha2_update(&ctx,(uint8_t*)&cy[i],8);}
    sha2_final(&ctx,out_hash);
    free(cx); free(cy); return 0;
}

void ts_merkle_root(const uint8_t* leaves,int n_leaves,uint8_t out_root[32]){
    if(n_leaves<=0){sha2_ctx ctx;sha2_init(&ctx);sha2_update(&ctx,(uint8_t*)"EMPTY",5);sha2_final(&ctx,out_root);return;}
    uint8_t* cur=(uint8_t*)malloc((size_t)n_leaves*32);
    memcpy(cur,leaves,(size_t)n_leaves*32);
    qsort(cur,(size_t)n_leaves,32,_cmp32);
    int n=n_leaves;
    while(n>1){
        uint8_t* nxt=(uint8_t*)malloc((size_t)((n+1)/2)*32);
        int wp=0;
        for(int i=0;i<n;i+=2){sha2_ctx ctx;sha2_init(&ctx);
            sha2_update(&ctx,cur+i*32,32);
            sha2_update(&ctx,cur+((i+1<n)?(i+1):i)*32,32);
            sha2_final(&ctx,nxt+wp*32); wp++;}
        free(cur); cur=nxt; n=wp;
    }
    memcpy(out_root,cur,32); free(cur);
}

int ts_geo_plan_signature(const double* xs,const double* ys,const int* sizes,int n_rings,uint8_t out_root[32]){
    if(n_rings<=0){sha2_ctx ctx;sha2_init(&ctx);sha2_update(&ctx,(uint8_t*)"EMPTY",5);sha2_final(&ctx,out_root);return 0;}
    uint8_t* sigs=(uint8_t*)malloc((size_t)n_rings*32);
    if(!sigs) return -1;
    int off=0;
    for(int r=0;r<n_rings;++r){
        int sz=sizes[r];
        if(ts_geo_ring_hash(xs+off,ys+off,sz,sigs+r*32)!=0){free(sigs);return -1;}
        off+=sz;
    }
    qsort(sigs,(size_t)n_rings,32,_cmp32);
    ts_merkle_root(sigs,n_rings,out_root);
    free(sigs); return 0;
}

int ts_merkle_proof(const uint8_t* leaves,int n,const uint8_t target[32],uint8_t* op,uint8_t* os){
    uint8_t* cur=(uint8_t*)malloc((size_t)n*32);
    memcpy(cur,leaves,(size_t)n*32); qsort(cur,(size_t)n,32,_cmp32);
    int idx=-1;
    for(int i=0;i<n;++i) if(memcmp(cur+i*32,target,32)==0){idx=i;break;}
    if(idx<0){free(cur);return -1;}
    int pl=0;
    while(n>1){
        int sib=idx^1; int eff=(sib<n)?sib:idx;
        memcpy(op+pl*32,cur+eff*32,32);
        os[pl]=(idx%2==0)?1:0; pl++;
        uint8_t* nxt=(uint8_t*)malloc((size_t)((n+1)/2)*32);
        int wp=0;
        for(int i=0;i<n;i+=2){sha2_ctx ctx;sha2_init(&ctx);
            sha2_update(&ctx,cur+i*32,32);
            sha2_update(&ctx,cur+((i+1<n)?(i+1):i)*32,32);
            sha2_final(&ctx,nxt+wp*32); wp++;}
        free(cur); cur=nxt; idx/=2; n=wp;
    }
    free(cur); return pl;
}

int ts_merkle_verify(const uint8_t target[32],const uint8_t* proof,const uint8_t* sides,int pl,const uint8_t root[32]){
    uint8_t h[32]; memcpy(h,target,32);
    for(int i=0;i<pl;++i){sha2_ctx ctx;sha2_init(&ctx);
        if(sides[i]==1){sha2_update(&ctx,h,32);sha2_update(&ctx,proof+i*32,32);}
        else{sha2_update(&ctx,proof+i*32,32);sha2_update(&ctx,h,32);}
        sha2_final(&ctx,h);}
    return memcmp(h,root,32)==0?1:0;
}

/* ── ts_hex_* ── */
void ts_hex_cart_to_axial(double x,double y,double hs,int32_t* q,int32_t* r){
    double qf=(SQRT3/3.0*x-1.0/3.0*y)/hs, rf=(2.0/3.0*y)/hs;
    double xf=qf,zf=rf,yf=-xf-zf;
    double rx=round(xf),ry=round(yf),rz=round(zf);
    double dx=fabs(rx-xf),dy=fabs(ry-yf),dz=fabs(rz-zf);
    if(dx>dy&&dx>dz) rx=-ry-rz; else if(dy>dz) ry=-rx-rz; else rz=-rx-ry;
    *q=(int32_t)rx; *r=(int32_t)rz;
}
void ts_hex_axial_to_cart(int32_t q,int32_t r,double hs,double* x,double* y){
    *x=hs*SQRT3*(q+r/2.0); *y=hs*1.5*r;
}
void ts_hex_vertices(int32_t q,int32_t r,double hs,double* ox,double* oy){
    double cx,cy; ts_hex_axial_to_cart(q,r,hs,&cx,&cy);
    for(int k=0;k<6;++k){double a=M_PI/180.0*(60.0*k+30.0);
        ox[k]=cx+hs*cos(a); oy[k]=cy+hs*sin(a);}
}
static inline int _il2(double px,double py,double ax,double ay,double bx,double by){
    return ((bx-ax)*(py-ay)-(by-ay)*(px-ax))>=0.0;}
static int _isct2(double x1,double y1,double x2,double y2,double x3,double y3,double x4,double y4,double* ix,double* iy){
    double d=(x1-x2)*(y3-y4)-(y1-y2)*(x3-x4);
    if(fabs(d)<1e-15) return 0;
    double t=((x1-x3)*(y3-y4)-(y1-y3)*(x3-x4))/d;
    *ix=x1+t*(x2-x1); *iy=y1+t*(y2-y1); return 1;
}
static double _aa2(const double* xs,const double* ys,int n){
    double s=0; for(int i=0;i<n;++i){int j=(i+1)%n; s+=xs[i]*ys[j]-xs[j]*ys[i];} return fabs(s)*0.5;
}
int ts_hex_polygon_coverage(const double* pxs,const double* pys,int n,double hs,int32_t* oq,int32_t* or_,int mx){
    if(n<3) return 0;
    double xmin=pxs[0],xmax=pxs[0],ymin=pys[0],ymax=pys[0];
    for(int i=1;i<n;++i){if(pxs[i]<xmin)xmin=pxs[i];if(pxs[i]>xmax)xmax=pxs[i];
        if(pys[i]<ymin)ymin=pys[i];if(pys[i]>ymax)ymax=pys[i];}
    int q0,r0,q1,r1;
    ts_hex_cart_to_axial(xmin-hs,ymin-hs,hs,&q0,&r0);
    ts_hex_cart_to_axial(xmax+hs,ymax+hs,hs,&q1,&r1);
    int qmn=(q0<q1)?q0:q1,qmx=(q0<q1)?q1:q0;
    int rmn=(r0<r1)?r0:r1,rmx=(r0<r1)?r1:r0;
    qmn-=2;qmx+=2;rmn-=2;rmx+=2;
    int no=0; double hx[6],hy[6];
    enum{MC=64}; double cx[MC],cy[MC],nx[MC],ny[MC];
    for(int r=rmn;r<=rmx&&no<mx;++r)for(int q=qmn;q<=qmx&&no<mx;++q){
        ts_hex_vertices(q,r,hs,hx,hy);
        int nc=(n>MC)?MC:n;
        memcpy(cx,pxs,(size_t)nc*sizeof(double));
        memcpy(cy,pys,(size_t)nc*sizeof(double));
        int alive=1;
        for(int e=0;e<6&&alive;++e){
            double e1x=hx[e],e1y=hy[e],e2x=hx[(e+1)%6],e2y=hy[(e+1)%6];
            int nn=0;
            for(int j=0;j<nc;++j){
                double ccx=cx[j],ccy=cy[j];
                int pi=(j==0)?nc-1:j-1;
                double ppx=cx[pi],ppy=cy[pi];
                int cn=_il2(ccx,ccy,e1x,e1y,e2x,e2y),pn=_il2(ppx,ppy,e1x,e1y,e2x,e2y);
                if(cn){if(!pn){double ix,iy;
                    if(_isct2(ppx,ppy,ccx,ccy,e1x,e1y,e2x,e2y,&ix,&iy)&&nn<MC){nx[nn]=ix;ny[nn]=iy;nn++;}}
                    if(nn<MC){nx[nn]=ccx;ny[nn]=ccy;nn++;}
                }else if(pn){double ix,iy;
                    if(_isct2(ppx,ppy,ccx,ccy,e1x,e1y,e2x,e2y,&ix,&iy)&&nn<MC){nx[nn]=ix;ny[nn]=iy;nn++;}}
            }
            if(nn==0){alive=0;break;}
            memcpy(cx,nx,(size_t)nn*sizeof(double)); memcpy(cy,ny,(size_t)nn*sizeof(double)); nc=nn;
        }
        if(alive&&nc>=3){double a=_aa2(cx,cy,nc); if(a>1e-6){oq[no]=q;or_[no]=r;no++;}}
    }
    return no;
}
