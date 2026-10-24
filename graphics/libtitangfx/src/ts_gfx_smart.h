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
/* ── ts_gfx_smart.h — Cross-engine pipelines, smart features ──────
 * Features that NOBODY else has in a single-header C99 engine.
 * SoftCAD C+C TOPO © 2026
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════════ */
/*  COLOR BLINDNESS SIMULATION (accessibility)                         */
/*  Simulate how colorblind users see your rendering.                  */
/*  Nobody offers this in a rendering engine.                          */
/* ═══════════════════════════════════════════════════════════════════ */

/* Protanopia (no red cones) */
void ts_canvas_simulate_protanopia(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        float r = ((px>>16)&0xFF)/255.0f, g = ((px>>8)&0xFF)/255.0f, b = (px&0xFF)/255.0f;
        float nr = 0.567f*r + 0.433f*g + 0.000f*b;
        float ng = 0.558f*r + 0.442f*g + 0.000f*b;
        float nb = 0.000f*r + 0.242f*g + 0.758f*b;
        int ri=(int)(nr*255), gi=(int)(ng*255), bi=(int)(nb*255);
        if(ri>255)ri=255; if(gi>255)gi=255; if(bi>255)bi=255;
        c->pixels[i] = (px & 0xFF000000) | (uint32_t)((ri<<16)|(gi<<8)|bi);
    }
}

/* Deuteranopia (no green cones) */
void ts_canvas_simulate_deuteranopia(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        float r = ((px>>16)&0xFF)/255.0f, g = ((px>>8)&0xFF)/255.0f, b = (px&0xFF)/255.0f;
        float nr = 0.625f*r + 0.375f*g + 0.000f*b;
        float ng = 0.700f*r + 0.300f*g + 0.000f*b;
        float nb = 0.000f*r + 0.300f*g + 0.700f*b;
        int ri=(int)(nr*255), gi=(int)(ng*255), bi=(int)(nb*255);
        if(ri>255)ri=255; if(gi>255)gi=255; if(bi>255)bi=255;
        c->pixels[i] = (px & 0xFF000000) | (uint32_t)((ri<<16)|(gi<<8)|bi);
    }
}

/* Tritanopia (no blue cones) */
void ts_canvas_simulate_tritanopia(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        float r = ((px>>16)&0xFF)/255.0f, g = ((px>>8)&0xFF)/255.0f, b = (px&0xFF)/255.0f;
        float nr = 0.950f*r + 0.050f*g + 0.000f*b;
        float ng = 0.000f*r + 0.433f*g + 0.567f*b;
        float nb = 0.000f*r + 0.475f*g + 0.525f*b;
        int ri=(int)(nr*255), gi=(int)(ng*255), bi=(int)(nb*255);
        if(ri>255)ri=255; if(gi>255)gi=255; if(bi>255)bi=255;
        c->pixels[i] = (px & 0xFF000000) | (uint32_t)((ri<<16)|(gi<<8)|bi);
    }
}

/* Achromatopsia (total color blindness — grayscale) */
void ts_canvas_simulate_achromatopsia(TS_Canvas *c) {
    ts_canvas_grayscale(c);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  CANVAS HISTOGRAM (for CI/CD image regression testing)              */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_canvas_histogram(const TS_Canvas *c, int *hist_r, int *hist_g, int *hist_b) {
    memset(hist_r, 0, 256 * sizeof(int));
    memset(hist_g, 0, 256 * sizeof(int));
    memset(hist_b, 0, 256 * sizeof(int));
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        hist_r[(px >> 16) & 0xFF]++;
        hist_g[(px >> 8) & 0xFF]++;
        hist_b[px & 0xFF]++;
    }
}

/* Average pixel value (0.0 = black, 1.0 = white) — for diff scoring */
float ts_canvas_avg_luminance(const TS_Canvas *c) {
    double sum = 0;
    int n = c->width * c->height;
    for (int i = 0; i < n; i++) {
        uint32_t px = c->pixels[i];
        sum += (((px>>16)&0xFF)*299 + ((px>>8)&0xFF)*587 + (px&0xFF)*114) / 1000.0;
    }
    return (float)(sum / (n * 255.0));
}

/* Diff score between two canvases (0.0 = identical, 1.0 = completely different) */
float ts_canvas_diff_score(const TS_Canvas *a, const TS_Canvas *b) {
    int w = a->width < b->width ? a->width : b->width;
    int h = a->height < b->height ? a->height : b->height;
    double sum = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pa = a->pixels[y * a->width + x];
            uint32_t pb = b->pixels[y * b->width + x];
            int dr = abs((int)((pa>>16)&0xFFu) - (int)((pb>>16)&0xFFu));
            int dg = abs((int)((pa>>8)&0xFFu) - (int)((pb>>8)&0xFFu));
            int db = abs((int)(pa&0xFFu) - (int)(pb&0xFFu));
            sum += (dr + dg + db) / 3.0;
        }
    }
    return (float)(sum / (w * h * 255.0));
}

/* Count pixels that differ by more than threshold */
int ts_canvas_diff_pixel_count(const TS_Canvas *a, const TS_Canvas *b, int threshold) {
    int w = a->width < b->width ? a->width : b->width;
    int h = a->height < b->height ? a->height : b->height;
    int count = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pa = a->pixels[y * a->width + x];
            uint32_t pb = b->pixels[y * b->width + x];
            int dr = abs((int)((pa>>16)&0xFFu) - (int)((pb>>16)&0xFFu));
            int dg = abs((int)((pa>>8)&0xFFu) - (int)((pb>>8)&0xFFu));
            int db = abs((int)(pa&0xFFu) - (int)(pb&0xFFu));
            if (dr > threshold || dg > threshold || db > threshold) count++;
        }
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEOMETRY: Delaunay triangulation (Bowyer-Watson)                   */
/*  For TIN terrain models. Nobody has this in single-header C99.      */
/* ═══════════════════════════════════════════════════════════════════ */

typedef struct { int v0, v1, v2; } TS_DelaunayTri;

static int _circumcircle_contains(double px, double py,
    double ax, double ay, double bx, double by, double cx, double cy) {
    double d = 2*(ax*(by-cy)+bx*(cy-ay)+cx*(ay-by));
    if (fabs(d) < 1e-10) return 0;
    double ux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by)) / d;
    double uy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax)) / d;
    double r2 = (ax-ux)*(ax-ux)+(ay-uy)*(ay-uy);
    return (px-ux)*(px-ux)+(py-uy)*(py-uy) < r2;
}

int ts_geo_delaunay(const double *pts, int npts, TS_DelaunayTri *tris, int max_tris) {
    if (npts < 3 || !tris) return 0;
    /* Super triangle */
    double xmin=pts[0],xmax=pts[0],ymin=pts[1],ymax=pts[1];
    for (int i=1;i<npts;i++){
        if(pts[i*2]<xmin)xmin=pts[i*2]; if(pts[i*2]>xmax)xmax=pts[i*2];
        if(pts[i*2+1]<ymin)ymin=pts[i*2+1]; if(pts[i*2+1]>ymax)ymax=pts[i*2+1];
    }
    double dx=xmax-xmin, dy=ymax-ymin, dmax=dx>dy?dx:dy;
    double cx=(xmin+xmax)/2, cy=(ymin+ymax)/2;
    /* Extra points for super triangle */
    double all_pts[(npts+3)*2];
    memcpy(all_pts, pts, (size_t)(npts*2)*sizeof(double));
    all_pts[npts*2]=cx-20*dmax; all_pts[npts*2+1]=cy-dmax;
    all_pts[(npts+1)*2]=cx+20*dmax; all_pts[(npts+1)*2+1]=cy-dmax;
    all_pts[(npts+2)*2]=cx; all_pts[(npts+2)*2+1]=cy+20*dmax;

    TS_DelaunayTri *work = (TS_DelaunayTri*)malloc(sizeof(TS_DelaunayTri)*(size_t)(max_tris*3));
    if (!work) return 0;
    int ntri = 0;
    work[ntri++] = (TS_DelaunayTri){npts, npts+1, npts+2};

    for (int p = 0; p < npts; p++) {
        double px=all_pts[p*2], py=all_pts[p*2+1];
        /* Find bad triangles */
        int *bad = (int*)calloc((size_t)ntri, sizeof(int));
        int nbad = 0;
        for (int t = 0; t < ntri; t++) {
            TS_DelaunayTri *tri = &work[t];
            if (_circumcircle_contains(px, py,
                all_pts[tri->v0*2], all_pts[tri->v0*2+1],
                all_pts[tri->v1*2], all_pts[tri->v1*2+1],
                all_pts[tri->v2*2], all_pts[tri->v2*2+1])) {
                bad[t] = 1; nbad++;
            }
        }
        /* Find boundary polygon edges */
        typedef struct { int a, b; } _Edge;
        _Edge *edges = (_Edge*)malloc(sizeof(_Edge)*(size_t)(nbad*3));
        int nedges = 0;
        for (int t = 0; t < ntri; t++) {
            if (!bad[t]) continue;
            int e[3][2] = {{work[t].v0,work[t].v1},{work[t].v1,work[t].v2},{work[t].v2,work[t].v0}};
            for (int i = 0; i < 3; i++) {
                int shared = 0;
                for (int t2 = 0; t2 < ntri; t2++) {
                    if (t2 == t || !bad[t2]) continue;
                    int e2[3][2] = {{work[t2].v0,work[t2].v1},{work[t2].v1,work[t2].v2},{work[t2].v2,work[t2].v0}};
                    for (int j = 0; j < 3; j++)
                        if ((e[i][0]==e2[j][0]&&e[i][1]==e2[j][1])||(e[i][0]==e2[j][1]&&e[i][1]==e2[j][0]))
                            shared = 1;
                }
                if (!shared) edges[nedges++] = (_Edge){e[i][0], e[i][1]};
            }
        }
        /* Remove bad triangles */
        int new_ntri = 0;
        for (int t = 0; t < ntri; t++)
            if (!bad[t]) work[new_ntri++] = work[t];
        ntri = new_ntri;
        /* Add new triangles */
        for (int i = 0; i < nedges && ntri < max_tris*3-1; i++)
            work[ntri++] = (TS_DelaunayTri){edges[i].a, edges[i].b, p};
        free(bad); free(edges);
    }
    /* Remove super triangle vertices */
    int out = 0;
    for (int t = 0; t < ntri && out < max_tris; t++) {
        if (work[t].v0 >= npts || work[t].v1 >= npts || work[t].v2 >= npts) continue;
        tris[out++] = work[t];
    }
    free(work);
    return out;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEOMETRY: Voronoi diagram (from Delaunay dual)                     */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_geo_voronoi_edges(const double *pts, int npts,
                           double *edges_out, int max_edges) {
    /* Voronoi = dual of Delaunay: connect circumcenters of adjacent triangles */
    int max_tris = npts * 3;
    TS_DelaunayTri *tris = (TS_DelaunayTri*)malloc(sizeof(TS_DelaunayTri)*(size_t)max_tris);
    if (!tris) return 0;
    int ntri = ts_geo_delaunay(pts, npts, tris, max_tris);

    int nedges = 0;
    for (int i = 0; i < ntri && nedges < max_edges; i++) {
        double ax=pts[tris[i].v0*2],ay=pts[tris[i].v0*2+1];
        double bx=pts[tris[i].v1*2],by=pts[tris[i].v1*2+1];
        double cx=pts[tris[i].v2*2],cy=pts[tris[i].v2*2+1];
        double d = 2*(ax*(by-cy)+bx*(cy-ay)+cx*(ay-by));
        if (fabs(d) < 1e-10) continue;
        double ux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/d;
        double uy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/d;

        /* Find adjacent triangles sharing an edge */
        for (int j = i+1; j < ntri && nedges < max_edges; j++) {
            int shared = 0;
            int vi[3]={tris[i].v0,tris[i].v1,tris[i].v2};
            int vj[3]={tris[j].v0,tris[j].v1,tris[j].v2};
            for (int a=0;a<3;a++) for (int b=0;b<3;b++) if(vi[a]==vj[b]) shared++;
            if (shared >= 2) {
                double ax2=pts[tris[j].v0*2],ay2=pts[tris[j].v0*2+1];
                double bx2=pts[tris[j].v1*2],by2=pts[tris[j].v1*2+1];
                double cx2=pts[tris[j].v2*2],cy2=pts[tris[j].v2*2+1];
                double d2 = 2*(ax2*(by2-cy2)+bx2*(cy2-ay2)+cx2*(ay2-by2));
                if (fabs(d2) < 1e-10) continue;
                double ux2 = ((ax2*ax2+ay2*ay2)*(by2-cy2)+(bx2*bx2+by2*by2)*(cy2-ay2)+(cx2*cx2+cy2*cy2)*(ay2-by2))/d2;
                double uy2 = ((ax2*ax2+ay2*ay2)*(cx2-bx2)+(bx2*bx2+by2*by2)*(ax2-cx2)+(cx2*cx2+cy2*cy2)*(bx2-ax2))/d2;
                edges_out[nedges*4+0]=ux; edges_out[nedges*4+1]=uy;
                edges_out[nedges*4+2]=ux2; edges_out[nedges*4+3]=uy2;
                nedges++;
            }
        }
    }
    free(tris);
    return nedges;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEOMETRY: Minimum Bounding Rectangle (rotated)                     */
/* ═══════════════════════════════════════════════════════════════════ */

double ts_geo_min_bounding_rect(const double *hull, int n,
                                  double *out_cx, double *out_cy,
                                  double *out_w, double *out_h, double *out_angle) {
    if (n < 3) return 0;
    double min_area = 1e30;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double edx = hull[j*2]-hull[i*2], edy = hull[j*2+1]-hull[i*2+1];
        double len = sqrt(edx*edx+edy*edy);
        if (len < 1e-10) continue;
        double ux = edx/len, uy = edy/len;
        double vx = -uy, vy = ux;
        double umin=1e30,umax=-1e30,vmin=1e30,vmax=-1e30;
        for (int k = 0; k < n; k++) {
            double pu = hull[k*2]*ux + hull[k*2+1]*uy;
            double pv = hull[k*2]*vx + hull[k*2+1]*vy;
            if(pu<umin)umin=pu; if(pu>umax)umax=pu;
            if(pv<vmin)vmin=pv; if(pv>vmax)vmax=pv;
        }
        double area = (umax-umin)*(vmax-vmin);
        if (area < min_area) {
            min_area = area;
            *out_w = umax - umin;
            *out_h = vmax - vmin;
            *out_angle = atan2(uy, ux) * 180.0 / M_PI;
            double cu = (umin+umax)/2, cv = (vmin+vmax)/2;
            *out_cx = cu*ux + cv*vx;
            *out_cy = cu*uy + cv*vy;
        }
    }
    return min_area;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEOMETRY: Nearest point on ring                                    */
/* ═══════════════════════════════════════════════════════════════════ */

double ts_geo_nearest_on_ring(const double *ring, int n,
                                double px, double py,
                                double *out_x, double *out_y) {
    double min_dist = 1e30;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double ax=ring[i*2],ay=ring[i*2+1],bx=ring[j*2],by=ring[j*2+1];
        double dx=bx-ax, dy=by-ay, len2=dx*dx+dy*dy;
        double t = len2 < 1e-10 ? 0 : ((px-ax)*dx+(py-ay)*dy)/len2;
        if (t<0) t=0; if (t>1) t=1;
        double nx=ax+t*dx, ny=ay+t*dy;
        double d = sqrt((px-nx)*(px-nx)+(py-ny)*(py-ny));
        if (d < min_dist) { min_dist = d; *out_x = nx; *out_y = ny; }
    }
    return min_dist;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  COORDINATE AUTO-DETECT                                             */
/*  Guess format: decimal degrees, DMS, UTM, Stereo70                  */
/* ═══════════════════════════════════════════════════════════════════ */

/* Returns: 0=unknown, 1=decimal_latlon, 2=stereo70, 3=utm, 4=web_mercator */
int ts_geo_coord_detect(double x, double y) {
    /* Stereo70: X ~300000-800000, Y ~200000-700000 */
    if (x > 200000 && x < 900000 && y > 100000 && y < 800000) return 2;
    /* Web Mercator: X ~-20M to 20M, Y ~-20M to 20M */
    if (fabs(x) > 100000 && fabs(x) < 25000000 && fabs(y) > 100000 && fabs(y) < 25000000) return 4;
    /* UTM: X ~100000-900000, Y ~0-10000000 */
    if (x > 100000 && x < 1000000 && y > 0 && y < 10000000) return 3;
    /* Decimal lat/lon: -180 to 180, -90 to 90 */
    if (fabs(x) <= 180 && fabs(y) <= 90) return 1;
    return 0;
}

const char *ts_geo_coord_detect_name(double x, double y) {
    switch (ts_geo_coord_detect(x, y)) {
        case 1: return "WGS84 (decimal degrees)";
        case 2: return "Stereo70 (EPSG:31700)";
        case 3: return "UTM";
        case 4: return "Web Mercator (EPSG:3857)";
        default: return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  HASH: File manifest (hash all files, output as checksum file)      */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_hash_file_sha256(const char *path, char *hex_out) {
    unsigned char hash[32];
    int ok = ts_sha256_file(path, hash);
    if (!ok) return 0;
    ts_sha256_hex(hash, hex_out);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  TEXT: Auto-size text to fit rectangle                               */
/* ═══════════════════════════════════════════════════════════════════ */

/* Find largest font size (6-72) that fits text in given width */
int ts_text_auto_size(const char *text, int max_width, int min_size, int max_size) {
    int len = (int)strlen(text);
    for (int sz = max_size; sz >= min_size; sz--) {
        int tw = len * sz * 6 / 10; /* approximate: char width ≈ 0.6 × font size */
        if (tw <= max_width) return sz;
    }
    return min_size;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEO: DMS string parse ("44°26'15.3\"N" → decimal degrees)         */
/*  NOBODY parses DMS strings in a C99 engine.                         */
/* ═══════════════════════════════════════════════════════════════════ */

double ts_geo_parse_dms(const char *dms_str) {
    /* Parse DMS in many formats: "44 26 15.3", "44d26m15.3s", "44°26'15.3\"N" */
    double vals[3] = {0, 0, 0};
    int vi = 0, neg = 0;
    const char *p = dms_str;
    while (*p == ' ') p++;
    if (*p == '-') { neg = 1; p++; }
    while (*p && vi < 3) {
        if ((*p >= '0' && *p <= '9') || *p == '.' || *p == ',') {
            vals[vi] = atof(p);
            /* Skip past number */
            while (*p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ',')) p++;
            vi++;
        } else {
            /* Skip non-digit separators (d, m, s, °, ', ") */
            p++;
        }
    }
    /* Check remaining for S/W */
    while (*p) {
        /* S/W = South/West. Only uppercase, not 's' for seconds */
        if ((*p == 'S' || *p == 'W') && (p == dms_str || *(p-1) == ' ' || (*(p-1) >= '0' && *(p-1) <= '9'))) neg = 1;
        p++;
    }
    double result = vals[0] + vals[1]/60.0 + vals[2]/3600.0;
    return neg ? -result : result;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GFX: Rounded rectangle on canvas                                   */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_draw_rect_rounded(TS_Canvas *c, int x, int y, int w, int h, int r) {
    /* Draw 4 straight edges + 4 quarter circle corners */
    ts_draw_line(c, x+r, y, x+w-r, y);         /* top */
    ts_draw_line(c, x+w, y+r, x+w, y+h-r);     /* right */
    ts_draw_line(c, x+w-r, y+h, x+r, y+h);     /* bottom */
    ts_draw_line(c, x, y+h-r, x, y+r);         /* left */
    /* Quarter circles at corners */
    for (int a = 0; a <= 90; a++) {
        float rad = a * (float)M_PI / 180.0f;
        float rad2 = (a+1) * (float)M_PI / 180.0f;
        /* Top-right */
        int x1 = x+w-r+(int)(r*cosf(rad)), y1 = y+r-(int)(r*sinf(rad));
        int x2 = x+w-r+(int)(r*cosf(rad2)), y2 = y+r-(int)(r*sinf(rad2));
        ts_draw_line(c, x1, y1, x2, y2);
        /* Top-left */
        x1 = x+r-(int)(r*cosf(rad)); y1 = y+r-(int)(r*sinf(rad));
        x2 = x+r-(int)(r*cosf(rad2)); y2 = y+r-(int)(r*sinf(rad2));
        ts_draw_line(c, x1, y1, x2, y2);
        /* Bottom-right */
        x1 = x+w-r+(int)(r*cosf(rad)); y1 = y+h-r+(int)(r*sinf(rad));
        x2 = x+w-r+(int)(r*cosf(rad2)); y2 = y+h-r+(int)(r*sinf(rad2));
        ts_draw_line(c, x1, y1, x2, y2);
        /* Bottom-left */
        x1 = x+r-(int)(r*cosf(rad)); y1 = y+h-r+(int)(r*sinf(rad));
        x2 = x+r-(int)(r*cosf(rad2)); y2 = y+h-r+(int)(r*sinf(rad2));
        ts_draw_line(c, x1, y1, x2, y2);
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GFX: Dashed/dotted line patterns                                   */
/* ═══════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════ */
/*  GFX: PNG load from file (roundtrip: save→load→edit→save)           */
/*  Uses the existing miniz zlib. Parses PNG chunks manually.          */
/* ═══════════════════════════════════════════════════════════════════ */

/* Note: ts_canvas_load_png already exists in ts_gfx_export.h */
/* Adding: load from memory buffer */

TS_Canvas *ts_canvas_from_pixels(const uint32_t *pixels, int w, int h) {
    TS_Canvas *c = ts_canvas_new(w, h);
    if (!c) return NULL;
    memcpy(c->pixels, pixels, (size_t)(w * h) * 4);
    return c;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  SVG vector export — proper SVG with drawing commands               */
/* ═══════════════════════════════════════════════════════════════════ */

/* SVG path builder for vector export */
typedef struct {
    FILE *f;
    int   w, h;
    int   element_count;
} TS_SvgWriter;

TS_SvgWriter *ts_svg_open(const char *path, int w, int h) {
    TS_SvgWriter *s = (TS_SvgWriter *)calloc(1, sizeof(TS_SvgWriter));
    if (!s) return NULL;
    s->f = fopen(path, "w");
    if (!s->f) { free(s); return NULL; }
    s->w = w; s->h = h;
    fprintf(s->f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(s->f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\">\n", w, h);
    return s;
}

void ts_svg_rect(TS_SvgWriter *s, double x, double y, double w, double h,
                  const char *fill, const char *stroke) {
    fprintf(s->f, "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\"", x, y, w, h);
    if (fill) fprintf(s->f, " fill=\"%s\"", fill);
    if (stroke) fprintf(s->f, " stroke=\"%s\" stroke-width=\"1\"", stroke);
    fprintf(s->f, "/>\n");
    s->element_count++;
}

void ts_svg_circle(TS_SvgWriter *s, double cx, double cy, double r,
                    const char *fill, const char *stroke) {
    fprintf(s->f, "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\"", cx, cy, r);
    if (fill) fprintf(s->f, " fill=\"%s\"", fill);
    if (stroke) fprintf(s->f, " stroke=\"%s\"", stroke);
    fprintf(s->f, "/>\n");
    s->element_count++;
}

void ts_svg_line(TS_SvgWriter *s, double x1, double y1, double x2, double y2,
                  const char *stroke, double width) {
    fprintf(s->f, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\"", x1, y1, x2, y2);
    fprintf(s->f, " stroke=\"%s\" stroke-width=\"%.1f\"/>\n", stroke ? stroke : "#000", width);
    s->element_count++;
}

void ts_svg_polygon(TS_SvgWriter *s, const double *pts, int npts,
                      const char *fill, const char *stroke) {
    fprintf(s->f, "  <polygon points=\"");
    for (int i = 0; i < npts; i++)
        fprintf(s->f, "%.1f,%.1f%s", pts[i*2], pts[i*2+1], i<npts-1?" ":"");
    fprintf(s->f, "\"");
    if (fill) fprintf(s->f, " fill=\"%s\"", fill);
    if (stroke) fprintf(s->f, " stroke=\"%s\"", stroke);
    fprintf(s->f, "/>\n");
    s->element_count++;
}

void ts_svg_text(TS_SvgWriter *s, double x, double y, const char *text,
                  int font_size, const char *fill) {
    fprintf(s->f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"%d\"", x, y, font_size);
    if (fill) fprintf(s->f, " fill=\"%s\"", fill);
    fprintf(s->f, ">%s</text>\n", text);
    s->element_count++;
}

void ts_svg_path(TS_SvgWriter *s, const char *d, const char *fill, const char *stroke) {
    fprintf(s->f, "  <path d=\"%s\"", d);
    if (fill) fprintf(s->f, " fill=\"%s\"", fill);
    if (stroke) fprintf(s->f, " stroke=\"%s\"", stroke);
    fprintf(s->f, "/>\n");
    s->element_count++;
}

int ts_svg_close(TS_SvgWriter *s) {
    if (!s) return 0;
    int n = s->element_count;
    fprintf(s->f, "</svg>\n");
    fclose(s->f);
    free(s);
    return n;
}


/* ═══════════════════════════════════════════════════════════════════ */
/*  FLOOD FILL (paint bucket tool)                                     */
/*  Nobody has this in a software renderer single-header.              */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_canvas_flood_fill(TS_Canvas *c, int x, int y, uint32_t fill_color) {
    if (x < 0 || x >= c->width || y < 0 || y >= c->height) return;
    uint32_t target = c->pixels[y * c->width + x];
    if (target == fill_color) return;

    /* Stack-based flood fill (no recursion = no stack overflow) */
    int *stack = (int *)malloc((size_t)(c->width * c->height) * 2 * sizeof(int));
    if (!stack) return;
    int sp = 0;
    stack[sp++] = x; stack[sp++] = y;

    int max_sp = c->width * c->height * 2;
    while (sp > 0) {
        int cy = stack[--sp], cx = stack[--sp];
        if (cx < 0 || cx >= c->width || cy < 0 || cy >= c->height) continue;
        if (c->pixels[cy * c->width + cx] != target) continue;
        c->pixels[cy * c->width + cx] = fill_color;
        if (sp + 8 < max_sp) { /* bounds check prevents overflow */
            stack[sp++] = cx+1; stack[sp++] = cy;
            stack[sp++] = cx-1; stack[sp++] = cy;
            stack[sp++] = cx; stack[sp++] = cy+1;
            stack[sp++] = cx; stack[sp++] = cy-1;
        }
    }
    free(stack);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  CANVAS CLONE (deep copy)                                           */
/* ═══════════════════════════════════════════════════════════════════ */

TS_Canvas *ts_canvas_clone(const TS_Canvas *src) {
    TS_Canvas *dst = ts_canvas_new(src->width, src->height);
    if (!dst) return NULL;
    memcpy(dst->pixels, src->pixels, (size_t)(src->width * src->height) * 4);
    return dst;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  THICK POLYLINE with miter join                                     */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_draw_polyline_thick(TS_Canvas *c, const float *pts, int npts, float width) {
    if (npts < 2 || width < 1) return;
    float hw = width / 2.0f;
    for (int i = 0; i < npts - 1; i++) {
        float x0=pts[i*2],y0=pts[i*2+1],x1=pts[(i+1)*2],y1=pts[(i+1)*2+1];
        float dx=x1-x0, dy=y1-y0;
        float len = sqrtf(dx*dx+dy*dy);
        if (len < 0.01f) continue;
        float nx = -dy/len * hw, ny = dx/len * hw;
        /* Draw as filled quad */
        float quad[8] = {x0+nx,y0+ny, x1+nx,y1+ny, x1-nx,y1-ny, x0-nx,y0-ny};
        ts_draw_polygon_filled(c, quad, 4);
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  DRAW DOTTED LINE                                                   */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_draw_line_dotted(TS_Canvas *c, int x0, int y0, int x1, int y1, int spacing) {
    float dx = (float)(x1-x0), dy = (float)(y1-y0);
    float len = sqrtf(dx*dx+dy*dy);
    if (len < 1 || spacing < 1) return;
    float ux = dx/len, uy = dy/len;
    for (float pos = 0; pos < len; pos += spacing) {
        int px = x0 + (int)(ux * pos), py = y0 + (int)(uy * pos);
        if (px >= 0 && px < c->width && py >= 0 && py < c->height)
            c->pixels[py * c->width + px] = c->fill_color;
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  DRAW STAR / REGULAR POLYGON                                        */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_draw_star(TS_Canvas *c, int cx, int cy, int outer_r, int inner_r, int points) {
    if (points < 3) return;
    float *pts = (float*)malloc((size_t)(points * 4) * sizeof(float));
    if (!pts) return;
    int np = 0;
    for (int i = 0; i < points; i++) {
        float a1 = (float)(2 * M_PI * i / points) - (float)(M_PI / 2);
        float a2 = a1 + (float)(M_PI / points);
        pts[np++] = cx + outer_r * cosf(a1);
        pts[np++] = cy + outer_r * sinf(a1);
        pts[np++] = cx + inner_r * cosf(a2);
        pts[np++] = cy + inner_r * sinf(a2);
    }
    ts_draw_polygon(c, pts, np / 2);
    free(pts);
}

void ts_draw_regular_polygon(TS_Canvas *c, int cx, int cy, int r, int sides) {
    if (sides < 3) return;
    float *pts = (float*)malloc((size_t)(sides * 2) * sizeof(float));
    if (!pts) return;
    for (int i = 0; i < sides; i++) {
        float a = (float)(2 * M_PI * i / sides) - (float)(M_PI / 2);
        pts[i*2] = cx + r * cosf(a);
        pts[i*2+1] = cy + r * sinf(a);
    }
    ts_draw_polygon(c, pts, sides);
    free(pts);
}


/* ═══════════════════════════════════════════════════════════════════ */
/*  COLOR PALETTE GENERATORS                                           */
/*  For data visualization — auto-generate N distinct colors.          */
/* ═══════════════════════════════════════════════════════════════════ */

/* Generate N evenly-spaced hue colors (rainbow palette) */
void ts_color_palette_rainbow(uint32_t *out, int n) {
    for (int i = 0; i < n; i++) {
        float h = (float)i / (float)n;
        out[i] = ts_color_hsv(h, 0.75f, 0.9f);
    }
}

/* Generate N pastel colors (softer, for fills) */
void ts_color_palette_pastel(uint32_t *out, int n) {
    for (int i = 0; i < n; i++) {
        float h = (float)i / (float)n;
        out[i] = ts_color_hsl(h, 0.6f, 0.8f);
    }
}

/* Generate N earth-tone colors (for GIS/terrain) */
void ts_color_palette_earth(uint32_t *out, int n) {
    static const uint32_t earth[] = {
        0xFF8B4513, 0xFF228B22, 0xFF6B8E23, 0xFFDAA520,
        0xFFA0522D, 0xFF2E8B57, 0xFF556B2F, 0xFFCD853F,
        0xFF8FBC8F, 0xFFD2B48C, 0xFF9ACD32, 0xFFBDB76B
    };
    for (int i = 0; i < n; i++) out[i] = earth[i % 12];
}

/* Cadastral standard colors (Romanian ANCPI convention) */
void ts_color_palette_cadastral(uint32_t *out, int n) {
    static const uint32_t cad[] = {
        0xFFFFF8DC, /* Parcel fill - cream */
        0xFFE0FFE0, /* Green space - light green */
        0xFFFFE4E1, /* Construction - light red */
        0xFFE6E6FA, /* Road - light lavender */
        0xFFB0E0E6, /* Water - powder blue */
        0xFFADD8E6, /* River - light blue */
        0xFFFFDAB9, /* Agriculture - peach */
        0xFFD3D3D3, /* Boundary - light gray */
    };
    for (int i = 0; i < n; i++) out[i] = cad[i % 8];
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  CUSTOM CONVOLUTION KERNEL                                          */
/*  Apply any 3x3 or 5x5 kernel — generic image filter pipeline.      */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_canvas_convolve3x3(TS_Canvas *c, const float kernel[9], float divisor) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w*h)*4);
    if (!tmp) return;
    if (divisor == 0) divisor = 1;
    for (int y = 1; y < h-1; y++) for (int x = 1; x < w-1; x++) {
        float sr=0,sg=0,sb=0;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++) {
            uint32_t px = c->pixels[(y+dy)*w+(x+dx)];
            float k = kernel[(dy+1)*3+(dx+1)];
            sr += ((px>>16)&0xFF)*k; sg += ((px>>8)&0xFF)*k; sb += (px&0xFF)*k;
        }
        int r=(int)(sr/divisor),g=(int)(sg/divisor),b=(int)(sb/divisor);
        if(r<0)r=0; if(r>255)r=255; if(g<0)g=0; if(g>255)g=255; if(b<0)b=0; if(b>255)b=255;
        tmp[y*w+x] = 0xFF000000|(uint32_t)((r<<16)|(g<<8)|b);
    }
    memcpy(c->pixels+w, tmp+w, (size_t)((h-2)*w)*4);
    free(tmp);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  DRAW CROSS / PLUS MARKER                                           */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_draw_cross(TS_Canvas *c, int cx, int cy, int size) {
    ts_draw_line(c, cx-size, cy, cx+size, cy);
    ts_draw_line(c, cx, cy-size, cx, cy+size);
}

void ts_draw_diamond(TS_Canvas *c, int cx, int cy, int size) {
    ts_draw_line(c, cx, cy-size, cx+size, cy);
    ts_draw_line(c, cx+size, cy, cx, cy+size);
    ts_draw_line(c, cx, cy+size, cx-size, cy);
    ts_draw_line(c, cx-size, cy, cx, cy-size);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  CANVAS TO RAW RGBA BUFFER (for web/API/framebuffer integration)    */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_canvas_to_rgba(const TS_Canvas *c, unsigned char *out, int cap) {
    int needed = c->width * c->height * 4;
    if (cap < needed) return 0;
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        out[i*4+0] = (px >> 16) & 0xFF; /* R */
        out[i*4+1] = (px >> 8) & 0xFF;  /* G */
        out[i*4+2] = px & 0xFF;          /* B */
        out[i*4+3] = (px >> 24) & 0xFF; /* A */
    }
    return needed;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  MEASURE TEXT WIDTH (approximate, for layout calculations)          */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_text_measure_width(const char *text, int font_size) {
    /* Approximate: average character width ≈ 0.6 × font size for monospace,
       0.5 for proportional. We use 0.55 as middle ground. */
    return (int)(strlen(text) * font_size * 55 / 100);
}

int ts_text_measure_height(int font_size) {
    return font_size + 2; /* font size + 2px padding */
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEO: Point along ring at fractional distance                       */
/* ═══════════════════════════════════════════════════════════════════ */

void ts_geo_point_along_ring(const double *ring, int n, double fraction,
                               double *out_x, double *out_y) {
    if (n < 2 || fraction <= 0) { *out_x=ring[0]; *out_y=ring[1]; return; }
    /* Total perimeter */
    double total = 0;
    for (int i = 0; i < n-1; i++) {
        double dx=ring[(i+1)*2]-ring[i*2], dy=ring[(i+1)*2+1]-ring[i*2+1];
        total += sqrt(dx*dx+dy*dy);
    }
    double target = fraction * total;
    double accum = 0;
    for (int i = 0; i < n-1; i++) {
        double dx=ring[(i+1)*2]-ring[i*2], dy=ring[(i+1)*2+1]-ring[i*2+1];
        double seg = sqrt(dx*dx+dy*dy);
        if (accum + seg >= target) {
            double t = (target - accum) / seg;
            *out_x = ring[i*2] + t * dx;
            *out_y = ring[i*2+1] + t * dy;
            return;
        }
        accum += seg;
    }
    *out_x = ring[(n-1)*2]; *out_y = ring[(n-1)*2+1];
}


/* ═══════════════════════════════════════════════════════════════════ */
/*  HASH: SHA-256 file hash for integrity verification                 */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_hash_files_match(const char *path_a, const char *path_b) {
    unsigned char ha[32], hb[32];
    if (!ts_sha256_file(path_a, ha)) return -1;
    if (!ts_sha256_file(path_b, hb)) return -1;
    return ts_hash_constant_compare(ha, hb, 32);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEO: Parse DMS string "44°26'15.3\"N" → decimal                   */
/* ═══════════════════════════════════════════════════════════════════ */

double ts_geo_parse_dms_string(const char *dms) {
    int d = 0, m = 0; double s = 0;
    char dir = 0;
    /* Try formats: 44°26'15.3"N, 44 26 15.3 N, 44:26:15.3N */
    if (sscanf(dms, "%d%*[°]%d%*[']%lf%*[\"]%c", &d, &m, &s, &dir) >= 3 ||
        sscanf(dms, "%d %d %lf %c", &d, &m, &s, &dir) >= 3 ||
        sscanf(dms, "%d:%d:%lf%c", &d, &m, &s, &dir) >= 3) {
        double dec = abs(d) + m / 60.0 + s / 3600.0;
        if (dir == 'S' || dir == 's' || dir == 'W' || dir == 'w') dec = -dec;
        if (d < 0) dec = -dec;
        return dec;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  XML: DOM builder — create from scratch (not just parse)            */
/* ═══════════════════════════════════════════════════════════════════ */

/* These are already in ts_titan_xml.h: ts_xml_create_dom, ts_xml_add_child,
   ts_xml_set_attr, ts_xml_set_text. But let me add a convenience: */

/* ═══════════════════════════════════════════════════════════════════ */
/*  GEO: Azimuth between two points (bearing in degrees)               */
/* ═══════════════════════════════════════════════════════════════════ */

/* Already exists as ts_geo_bearing, but add convenience for formatted output */

int ts_geo_bearing_formatted(double lat1, double lon1, double lat2, double lon2,
                               char *out, int cap) {
    double phi1 = lat1 * M_PI / 180, phi2 = lat2 * M_PI / 180;
    double dl = (lon2 - lon1) * M_PI / 180;
    double y = sin(dl) * cos(phi2);
    double x = cos(phi1)*sin(phi2) - sin(phi1)*cos(phi2)*cos(dl);
    double bearing = atan2(y, x) * 180 / M_PI;
    if (bearing < 0) bearing += 360;

    const char *cardinal;
    if (bearing < 22.5 || bearing >= 337.5) cardinal = "N";
    else if (bearing < 67.5) cardinal = "NE";
    else if (bearing < 112.5) cardinal = "E";
    else if (bearing < 157.5) cardinal = "SE";
    else if (bearing < 202.5) cardinal = "S";
    else if (bearing < 247.5) cardinal = "SV";
    else if (bearing < 292.5) cardinal = "V";
    else cardinal = "NV";

    return snprintf(out, (size_t)cap, "%.1f° %s", bearing, cardinal);
}


/* ═══════════════════════════════════════════════════════════════════ */
/*  RASTER BACKGROUND — blit sub-region of canvas (for orto overlay)  */
/*  Opus #6: "Imaginea în spate — raster fundal cu TitanCanvas"       */
/* ═══════════════════════════════════════════════════════════════════ */

/* Blit a rectangular region from src canvas to dst at position (dx,dy).
   src_x,src_y,src_w,src_h = source rectangle to copy.
   This is the core primitive for panning ortophoto backgrounds. */
void ts_canvas_blit_region(TS_Canvas *dst, const TS_Canvas *src,
                             int src_x, int src_y, int src_w, int src_h,
                             int dst_x, int dst_y) {
    for (int y = 0; y < src_h; y++) {
        int sy = src_y + y, dy = dst_y + y;
        if (sy < 0 || sy >= src->height || dy < 0 || dy >= dst->height) continue;
        for (int x = 0; x < src_w; x++) {
            int sx = src_x + x, dx2 = dst_x + x;
            if (sx < 0 || sx >= src->width || dx2 < 0 || dx2 >= dst->width) continue;
            dst->pixels[dy * dst->width + dx2] = src->pixels[sy * src->width + sx];
        }
    }
}

/* Blit with scaling (for zoom levels) — nearest neighbor */
void ts_canvas_blit_scaled(TS_Canvas *dst, const TS_Canvas *src,
                             int src_x, int src_y, int src_w, int src_h,
                             int dst_x, int dst_y, int dst_w, int dst_h) {
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) return;
    for (int y = 0; y < dst_h; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= dst->height) continue;
        int sy = src_y + y * src_h / dst_h;
        if (sy < 0 || sy >= src->height) continue;
        for (int x = 0; x < dst_w; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= dst->width) continue;
            int sx = src_x + x * src_w / dst_w;
            if (sx < 0 || sx >= src->width) continue;
            dst->pixels[dy * dst->width + dx] = src->pixels[sy * src->width + sx];
        }
    }
}

/* Blit with alpha blending (for semi-transparent overlays) */
void ts_canvas_blit_alpha(TS_Canvas *dst, const TS_Canvas *src,
                            int dst_x, int dst_y, float alpha) {
    int a = (int)(alpha * 255);
    if (a <= 0) return;
    if (a >= 255) { ts_canvas_composite(dst, src, dst_x, dst_y); return; }
    for (int y = 0; y < src->height; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= dst->height) continue;
        for (int x = 0; x < src->width; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= dst->width) continue;
            uint32_t s = src->pixels[y * src->width + x];
            uint32_t d = dst->pixels[dy * dst->width + dx];
            int sr=(s>>16)&0xFF,sg=(s>>8)&0xFF,sb=s&0xFF;
            int dr=(d>>16)&0xFF,dg=(d>>8)&0xFF,db=d&0xFF;
            int r = (sr*a + dr*(255-a)) / 255;
            int g = (sg*a + dg*(255-a)) / 255;
            int b = (sb*a + db*(255-a)) / 255;
            dst->pixels[dy * dst->width + dx] = 0xFF000000|(uint32_t)((r<<16)|(g<<8)|b);
        }
    }
}


/* ═══════════════════════════════════════════════════════════════════ */
/*  SVG EXPORT — generate SVG from canvas drawing commands             */
/*  Nobody has this: software renderer that outputs BOTH raster+vector */
/* ═══════════════════════════════════════════════════════════════════ */

/* Export canvas as SVG (pixel-accurate representation) */
int ts_canvas_export_svg(const TS_Canvas *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\">\n",
            c->width, c->height);
    fprintf(f, "<rect width=\"%d\" height=\"%d\" fill=\"white\"/>\n", c->width, c->height);

    /* Scan for colored regions using run-length encoding */
    for (int y = 0; y < c->height; y++) {
        int x = 0;
        while (x < c->width) {
            uint32_t px = c->pixels[y * c->width + x];
            if (px == 0xFFFFFFFF || (px & 0xFF000000) == 0) { x++; continue; }
            /* Run of same color */
            int run = 1;
            while (x + run < c->width && c->pixels[y * c->width + x + run] == px) run++;
            int r = (px>>16)&0xFF, g2 = (px>>8)&0xFF, b = px&0xFF;
            int alpha = (px>>24)&0xFF;
            if (alpha >= 255) {
                fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"1\" fill=\"#%02x%02x%02x\"/>\n",
                        x, y, run, r, g2, b);
            } else if (alpha > 0) {
                fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"1\" fill=\"#%02x%02x%02x\" fill-opacity=\"%.2f\"/>\n",
                        x, y, run, r, g2, b, alpha / 255.0);
            }
            /* alpha=0 → skip (fully transparent) */
            x += run;
        }
    }
    fprintf(f, "</svg>\n");
    fclose(f);
    return 1;
}

