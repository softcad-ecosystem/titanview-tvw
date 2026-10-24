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
#ifndef TS_PLOT_RASTER_H
#define TS_PLOT_RASTER_H
/*
 * ts_plot_raster.h — rasterizatorul planșelor (DXF -> TIFF) al motorului TITAN
 * SoftCAD C+C TOPO © 2026
 *
 * O listă de desenare (umpleri și linii cu grosime, în pixeli, originea în
 * colțul stânga-sus, y în jos) se randează în benzi orizontale, cu
 * antialiasing analitic pe orizontală și 16 sub-linii pe verticală:
 *
 *   - umplere: inele multiple, regula nonzero sau par-impar (găuri, insule,
 *     glife TrueType);
 *   - linie: grosime în pixeli, capete butt/round/square, îmbinări
 *     miter/round/bevel — conturul se construiește din bucăți orientate la fel
 *     (dreptunghiuri, cercuri, pane), reunite exact de regula nonzero;
 *   - compunere în ordinea desenării, cu alfa (source-over pe RGB 8 biți);
 *   - TIFF RGB 8 biți, LZW (ca libtiff: schimbarea timpurie a lățimii codului)
 *     sau necomprimat, câte o bandă pe fâșie: un A0 la 300 dpi nu stă
 *     niciodată întreg în memorie.
 *
 * Culorile: 0xRRGGBBAA. Funcțiile întorc >= 0 la reușită, < 0 la eroare.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TS_PLOT_CAP_BUTT    0
#define TS_PLOT_CAP_ROUND   1
#define TS_PLOT_CAP_SQUARE  2
#define TS_PLOT_JOIN_MITER  0
#define TS_PLOT_JOIN_ROUND  1
#define TS_PLOT_JOIN_BEVEL  2

typedef struct TS_Plot TS_Plot;

const char *ts_plot_version(void);
TS_Plot *ts_plot_new(int width, int height, uint32_t background_rgb);
void     ts_plot_free(TS_Plot *p);
int      ts_plot_count(const TS_Plot *p);
int      ts_plot_fill(TS_Plot *p, const double *xy, const int *ring_sizes, int n_rings,
                      uint32_t rgba, int even_odd);
int      ts_plot_stroke(TS_Plot *p, const double *xy, int n_points, int closed, double width,
                        uint32_t rgba, int cap, int join, double miter_limit);
int      ts_plot_render_rgb(TS_Plot *p, int y0, int rows, uint8_t *out, int stride);
int      ts_plot_write_tiff(TS_Plot *p, const char *path, double dpi, int rows_per_strip,
                            int compression);
long     ts_tiff_lzw_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);

#ifdef __cplusplus
}
#endif

#ifdef TS_PLOT_RASTER_IMPLEMENTATION
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TSP_SUB 16                     /* sub-linii pe pixel (antialiasing vertical) */
#define TSP_PI 3.14159265358979323846

typedef struct { float x0, y0, y1, dxdy; int8_t dir; } TSP_Edge;
typedef struct { int e0, ne; float bx0, by0, bx1, by1; uint32_t rgba; uint8_t even_odd; } TSP_Prim;

struct TS_Plot {
    int w, h;
    uint32_t bg;
    TSP_Edge *edges; int ne, cap_e;
    TSP_Prim *prims; int np, cap_p;
    /* primitiva în construcție */
    int cur_e0; float cbx0, cby0, cbx1, cby1;
    /* zone de lucru ale randării */
    float *cov, *acc;
    double *xs; int8_t *ds; int cap_x;
    int *act; int cap_act;
    double *tmp; int cap_tmp;          /* inelele unei linii */
};

const char *ts_plot_version(void) { return "ts_plot_raster 1.0 (4.77)"; }

/* finit (nu NaN, nu infinit) fără macroul isfinite: același cod mașină cu
   anteturile TSW și cu cele mingw-w64 (principiul 3 din tsw/README.md) */
static int tsp_finite(double v) { return (v - v) == 0.0; }

TS_Plot *ts_plot_new(int width, int height, uint32_t background_rgb) {
    if (width <= 0 || height <= 0 || width > 1000000 || height > 1000000) return NULL;
    TS_Plot *p = (TS_Plot *)calloc(1, sizeof(TS_Plot));
    if (!p) return NULL;
    p->w = width; p->h = height; p->bg = background_rgb & 0xFFFFFFu;
    p->cov = (float *)calloc((size_t)width + 4, sizeof(float));
    p->acc = (float *)calloc((size_t)width + 4, sizeof(float));
    if (!p->cov || !p->acc) { ts_plot_free(p); return NULL; }
    return p;
}

void ts_plot_free(TS_Plot *p) {
    if (!p) return;
    free(p->edges); free(p->prims); free(p->cov); free(p->acc);
    free(p->xs); free(p->ds); free(p->act); free(p->tmp);
    free(p);
}

int ts_plot_count(const TS_Plot *p) { return p ? p->np : -1; }

/* ── construirea primitivelor ─────────────────────────────────────────── */

static int tsp_reserve_edges(TS_Plot *p, int more) {
    if (p->ne + more <= p->cap_e) return 0;
    int nc = p->cap_e ? p->cap_e : 4096;
    while (nc < p->ne + more) {
        if (nc > (1 << 29)) return -1;
        nc *= 2;
    }
    TSP_Edge *e = (TSP_Edge *)realloc(p->edges, (size_t)nc * sizeof(TSP_Edge));
    if (!e) return -1;
    p->edges = e; p->cap_e = nc;
    return 0;
}

static void tsp_begin(TS_Plot *p) {
    p->cur_e0 = p->ne;
    p->cbx0 = p->cby0 = 1e30f; p->cbx1 = p->cby1 = -1e30f;
}

static int tsp_edge(TS_Plot *p, double xa, double ya, double xb, double yb) {
    if (!(tsp_finite(xa) && tsp_finite(ya) && tsp_finite(xb) && tsp_finite(yb))) return 0;
    if (xa < p->cbx0) p->cbx0 = (float)xa;
    if (xb < p->cbx0) p->cbx0 = (float)xb;
    if (xa > p->cbx1) p->cbx1 = (float)xa;
    if (xb > p->cbx1) p->cbx1 = (float)xb;
    if (ya < p->cby0) p->cby0 = (float)ya;
    if (yb < p->cby0) p->cby0 = (float)yb;
    if (ya > p->cby1) p->cby1 = (float)ya;
    if (yb > p->cby1) p->cby1 = (float)yb;
    if (ya == yb) return 0;                        /* orizontală: nu taie sub-liniile */
    if (tsp_reserve_edges(p, 1)) return -1;
    TSP_Edge *e = &p->edges[p->ne++];
    if (ya < yb) { e->x0 = (float)xa; e->y0 = (float)ya; e->y1 = (float)yb; e->dir = 1; }
    else         { e->x0 = (float)xb; e->y0 = (float)yb; e->y1 = (float)ya; e->dir = -1; }
    e->dxdy = (float)((xb - xa) / (yb - ya));
    return 0;
}

static int tsp_ring(TS_Plot *p, const double *xy, int n) {
    if (n < 2) return 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        if (tsp_edge(p, xy[2 * i], xy[2 * i + 1], xy[2 * j], xy[2 * j + 1])) return -1;
    }
    return 0;
}

static int tsp_cmp_edge(const void *a, const void *b) {
    float ya = ((const TSP_Edge *)a)->y0, yb = ((const TSP_Edge *)b)->y0;
    return (ya > yb) - (ya < yb);
}

static int tsp_end(TS_Plot *p, uint32_t rgba, int even_odd) {
    int ne = p->ne - p->cur_e0;
    if (ne <= 0 || (rgba & 0xFFu) == 0 || p->cbx1 < 0 || p->cby1 < 0 ||
        p->cbx0 >= (float)p->w || p->cby0 >= (float)p->h) {
        p->ne = p->cur_e0;                          /* nimic vizibil */
        return 0;
    }
    if (p->np >= p->cap_p) {
        int nc = p->cap_p ? p->cap_p * 2 : 1024;
        TSP_Prim *q = (TSP_Prim *)realloc(p->prims, (size_t)nc * sizeof(TSP_Prim));
        if (!q) return -1;
        p->prims = q; p->cap_p = nc;
    }
    qsort(p->edges + p->cur_e0, (size_t)ne, sizeof(TSP_Edge), tsp_cmp_edge);
    TSP_Prim *pr = &p->prims[p->np++];
    pr->e0 = p->cur_e0; pr->ne = ne;
    pr->bx0 = p->cbx0; pr->by0 = p->cby0; pr->bx1 = p->cbx1; pr->by1 = p->cby1;
    pr->rgba = rgba; pr->even_odd = (uint8_t)(even_odd ? 1 : 0);
    return 1;
}

int ts_plot_fill(TS_Plot *p, const double *xy, const int *ring_sizes, int n_rings,
                 uint32_t rgba, int even_odd) {
    if (!p || !xy || !ring_sizes || n_rings <= 0) return -1;
    tsp_begin(p);
    const double *q = xy;
    for (int r = 0; r < n_rings; r++) {
        int n = ring_sizes[r];
        if (n < 0) { p->ne = p->cur_e0; return -1; }
        if (tsp_ring(p, q, n)) { p->ne = p->cur_e0; return -1; }
        q += 2 * (size_t)n;
    }
    return tsp_end(p, rgba, even_odd);
}

/* ── liniile cu grosime: bucăți orientate la fel, reunite prin nonzero ── */

static int tsp_tmp(TS_Plot *p, int n) {
    if (n <= p->cap_tmp) return 0;
    int nc = p->cap_tmp ? p->cap_tmp : 256;
    while (nc < n) nc *= 2;
    double *t = (double *)realloc(p->tmp, (size_t)nc * 2 * sizeof(double));
    if (!t) return -1;
    p->tmp = t; p->cap_tmp = nc;
    return 0;
}

/* inel cu orientarea normalizată (aria pozitivă), ca toate bucățile să se adune */
static int tsp_piece(TS_Plot *p, const double *xy, int n) {
    double a = 0.0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
    }
    if (a == 0.0) return 0;
    if (a > 0.0) return tsp_ring(p, xy, n);
    for (int i = n - 1; i >= 0; i--) {
        int j = (i + n - 1) % n;
        if (tsp_edge(p, xy[2 * i], xy[2 * i + 1], xy[2 * j], xy[2 * j + 1])) return -1;
    }
    return 0;
}

static int tsp_circle(TS_Plot *p, double cx, double cy, double r) {
    if (r <= 0.0) return 0;
    int n = (int)ceil(2.0 * TSP_PI * r / 0.75);
    if (n < 8) n = 8;
    if (n > 512) n = 512;
    if (tsp_tmp(p, n)) return -1;
    for (int i = 0; i < n; i++) {
        double t = 2.0 * TSP_PI * i / n;
        p->tmp[2 * i] = cx + r * cos(t);
        p->tmp[2 * i + 1] = cy + r * sin(t);
    }
    return tsp_piece(p, p->tmp, n);
}

static int tsp_join(TS_Plot *p, double vx, double vy, double ax, double ay, double bx, double by,
                    double hw, int join, double miter_limit) {
    /* a = direcția segmentului care intră, b = a celui care iese (unitare) */
    double cross = ax * by - ay * bx;
    double dot = ax * bx + ay * by;
    if (fabs(cross) < 1e-12 && dot > 0.0) return 0;            /* coliniar, fără cot */
    if (join == TS_PLOT_JOIN_ROUND) return tsp_circle(p, vx, vy, hw);
    /* partea exterioară a cotului */
    double s = cross > 0.0 ? -1.0 : 1.0;
    double n1x = -ay * hw * s, n1y = ax * hw * s;              /* normala exterioară, intrare */
    double n2x = -by * hw * s, n2y = bx * hw * s;              /* normala exterioară, ieșire */
    double pts[8];
    int n = 0;
    pts[n++] = vx; pts[n++] = vy;
    pts[n++] = vx + n1x; pts[n++] = vy + n1y;
    if (join == TS_PLOT_JOIN_MITER) {
        double c = 1.0 + dot;                                   /* 2 cos²(θ/2) */
        if (c > 1e-12) {
            double m = sqrt(2.0 / c);                           /* raportul miter = 1/cos(θ/2) */
            if (m <= miter_limit) {
                double mx = (n1x + n2x), my = (n1y + n2y);
                double ml = sqrt(mx * mx + my * my);
                if (ml > 1e-12) {
                    double k = hw * m / ml;
                    pts[n++] = vx + mx * k; pts[n++] = vy + my * k;
                }
            }
        }
    }
    pts[n++] = vx + n2x; pts[n++] = vy + n2y;
    return tsp_piece(p, pts, n / 2);
}

int ts_plot_stroke(TS_Plot *p, const double *xy, int n_points, int closed, double width,
                   uint32_t rgba, int cap, int join, double miter_limit) {
    if (!p || !xy || n_points <= 0 || !(width > 0.0) || !tsp_finite(width)) return -1;
    if (miter_limit < 1.0) miter_limit = 1.0;
    double hw = width / 2.0;
    /* vârfurile fără dubluri consecutive */
    if (tsp_tmp(p, n_points + 1)) return -1;
    double *v = (double *)malloc((size_t)(n_points + 1) * 2 * sizeof(double));
    if (!v) return -1;
    int n = 0;
    for (int i = 0; i < n_points; i++) {
        double x = xy[2 * i], y = xy[2 * i + 1];
        if (!(tsp_finite(x) && tsp_finite(y))) continue;
        if (n > 0 && fabs(x - v[2 * n - 2]) < 1e-9 && fabs(y - v[2 * n - 1]) < 1e-9) continue;
        v[2 * n] = x; v[2 * n + 1] = y; n++;
    }
    if (closed && n > 2 && fabs(v[0] - v[2 * n - 2]) < 1e-9 && fabs(v[1] - v[2 * n - 1]) < 1e-9) n--;
    tsp_begin(p);
    int rc = 0;
    if (n == 1) {                                              /* segment de lungime zero */
        if (cap == TS_PLOT_CAP_ROUND) rc = tsp_circle(p, v[0], v[1], hw);
        else if (cap == TS_PLOT_CAP_SQUARE) {
            double q[8] = {v[0] - hw, v[1] - hw, v[0] + hw, v[1] - hw, v[0] + hw, v[1] + hw, v[0] - hw, v[1] + hw};
            rc = tsp_piece(p, q, 4);
        }
        free(v);
        if (rc) { p->ne = p->cur_e0; return -1; }
        return tsp_end(p, rgba, 0);
    }
    int nseg = closed && n > 2 ? n : n - 1;
    for (int i = 0; i < nseg && !rc; i++) {
        double ax = v[2 * i], ay = v[2 * i + 1];
        double bx = v[2 * ((i + 1) % n)], by = v[2 * ((i + 1) % n) + 1];
        double dx = bx - ax, dy = by - ay;
        double len = sqrt(dx * dx + dy * dy);
        if (len <= 0.0) continue;
        dx /= len; dy /= len;
        double es = 0.0, ee = 0.0;
        if (!(closed && n > 2) && cap == TS_PLOT_CAP_SQUARE) {
            if (i == 0) es = hw;
            if (i == nseg - 1) ee = hw;
        }
        double nx = -dy * hw, ny = dx * hw;
        double q[8] = {
            ax - dx * es + nx, ay - dy * es + ny,
            bx + dx * ee + nx, by + dy * ee + ny,
            bx + dx * ee - nx, by + dy * ee - ny,
            ax - dx * es - nx, ay - dy * es - ny,
        };
        rc = tsp_piece(p, q, 4);
        /* îmbinarea la capătul segmentului (dacă urmează altul) */
        int has_next = (closed && n > 2) || i < nseg - 1;
        if (!rc && has_next) {
            int k = (i + 2) % n;
            double cx = v[2 * k], cy = v[2 * k + 1];
            double ex = cx - bx, ey = cy - by;
            double l2 = sqrt(ex * ex + ey * ey);
            if (l2 > 0.0) rc = tsp_join(p, bx, by, dx, dy, ex / l2, ey / l2, hw, join, miter_limit);
        }
    }
    if (!rc && !(closed && n > 2) && cap == TS_PLOT_CAP_ROUND) {
        rc = tsp_circle(p, v[0], v[1], hw);
        if (!rc) rc = tsp_circle(p, v[2 * n - 2], v[2 * n - 1], hw);
    }
    free(v);
    if (rc) { p->ne = p->cur_e0; return -1; }
    return tsp_end(p, rgba, 0);
}

/* ── randarea pe benzi ─────────────────────────────────────────────────── */

static int tsp_scratch(TS_Plot *p, int n) {
    if (n <= p->cap_x) return 0;
    int nc = p->cap_x ? p->cap_x : 256;
    while (nc < n) nc *= 2;
    double *xs = (double *)realloc(p->xs, (size_t)nc * sizeof(double));
    if (!xs) return -1;
    p->xs = xs;
    int8_t *ds = (int8_t *)realloc(p->ds, (size_t)nc);
    if (!ds) return -1;
    p->ds = ds;
    int *act = (int *)realloc(p->act, (size_t)nc * sizeof(int));
    if (!act) return -1;
    p->act = act;
    p->cap_x = nc;
    return 0;
}

static void tsp_sort_x(double *xs, int8_t *ds, int n) {
    for (int i = 1; i < n; i++) {                           /* puține, aproape sortate */
        double x = xs[i]; int8_t d = ds[i];
        int j = i - 1;
        while (j >= 0 && xs[j] > x) { xs[j + 1] = xs[j]; ds[j + 1] = ds[j]; j--; }
        xs[j + 1] = x; ds[j + 1] = d;
    }
}

/* acumulează un interval [xa, xb) pe sub-linia curentă; cov/acc rămân zero în afara [tx0, tx1+1] */
static void tsp_span(TS_Plot *p, double xa, double xb, float wgt, int *tx0, int *tx1) {
    if (xa < 0.0) xa = 0.0;
    if (xb > (double)p->w) xb = (double)p->w;
    if (!(xb > xa)) return;
    int ia = (int)xa, ib = (int)xb;
    if (ia < *tx0) *tx0 = ia;
    if (ib > *tx1) *tx1 = ib;
    if (ia == ib) { p->cov[ia] += (float)(xb - xa) * wgt; return; }
    p->cov[ia] += (float)((double)(ia + 1) - xa) * wgt;
    p->acc[ia + 1] += wgt;
    p->acc[ib] -= wgt;
    if (ib < p->w) p->cov[ib] += (float)(xb - (double)ib) * wgt;
}

static int tsp_draw_prim(TS_Plot *p, const TSP_Prim *pr, int band0, int band1, uint8_t *out, int stride) {
    int r0 = (int)floor(pr->by0), r1 = (int)floor(pr->by1) + 1;   /* ceil fără ceilf (importuri fixate) */
    if (r0 < band0) r0 = band0;
    if (r1 > band1) r1 = band1;
    if (r0 >= r1) return 0;
    if (pr->bx1 < 0.0f || pr->bx0 >= (float)p->w) return 0;
    if (tsp_scratch(p, pr->ne + 2)) return -1;
    const TSP_Edge *E = p->edges + pr->e0;
    int next = 0, nact = 0;
    const float wgt = 1.0f / TSP_SUB;
    const double alpha = (double)(pr->rgba & 0xFFu) / 255.0;
    const double cr = (double)((pr->rgba >> 24) & 0xFFu);
    const double cg = (double)((pr->rgba >> 16) & 0xFFu);
    const double cb = (double)((pr->rgba >> 8) & 0xFFu);
    for (int row = r0; row < r1; row++) {
        int tx0 = p->w + 1, tx1 = -1;
        for (int s = 0; s < TSP_SUB; s++) {
            double y = (double)row + ((double)s + 0.5) / TSP_SUB;
            while (next < pr->ne && (double)E[next].y0 <= y) {
                if ((double)E[next].y1 > y) p->act[nact++] = next;
                next++;
            }
            int k = 0;
            for (int i = 0; i < nact; i++)
                if ((double)E[p->act[i]].y1 > y) p->act[k++] = p->act[i];
            nact = k;
            if (nact < 2) continue;
            for (int i = 0; i < nact; i++) {
                const TSP_Edge *e = &E[p->act[i]];
                p->xs[i] = (double)e->x0 + (y - (double)e->y0) * (double)e->dxdy;
                p->ds[i] = e->dir;
            }
            tsp_sort_x(p->xs, p->ds, nact);
            int wind = 0;
            for (int i = 0; i < nact - 1; i++) {
                wind += p->ds[i];
                int inside = pr->even_odd ? (wind & 1) : (wind != 0);
                if (inside) tsp_span(p, p->xs[i], p->xs[i + 1], wgt, &tx0, &tx1);
            }
        }
        if (tx1 < 0) continue;
        uint8_t *line = out + (size_t)(row - band0) * (size_t)stride;
        int xe = tx1 < p->w - 1 ? tx1 : p->w - 1;
        float run = 0.0f;
        for (int x = tx0; x <= xe; x++) {
            run += p->acc[x];
            float c = p->cov[x] + run;
            if (c <= 0.0005f) continue;
            if (c > 1.0f) c = 1.0f;
            double a = alpha * (double)c;
            uint8_t *px = line + 3 * (size_t)x;
            px[0] = (uint8_t)((double)px[0] + (cr - (double)px[0]) * a + 0.5);
            px[1] = (uint8_t)((double)px[1] + (cg - (double)px[1]) * a + 0.5);
            px[2] = (uint8_t)((double)px[2] + (cb - (double)px[2]) * a + 0.5);
        }
        int ze = tx1 + 1 <= p->w + 1 ? tx1 + 1 : p->w + 1;
        for (int x = tx0; x <= ze; x++) { p->cov[x] = 0.0f; p->acc[x] = 0.0f; }
    }
    return 0;
}

int ts_plot_render_rgb(TS_Plot *p, int y0, int rows, uint8_t *out, int stride) {
    if (!p || !out || rows <= 0 || y0 < 0 || y0 + rows > p->h || stride < 3 * p->w) return -1;
    uint8_t br = (uint8_t)((p->bg >> 16) & 0xFF), bg = (uint8_t)((p->bg >> 8) & 0xFF), bb = (uint8_t)(p->bg & 0xFF);
    for (int r = 0; r < rows; r++) {
        uint8_t *line = out + (size_t)r * (size_t)stride;
        for (int x = 0; x < p->w; x++) { line[3 * x] = br; line[3 * x + 1] = bg; line[3 * x + 2] = bb; }
    }
    int y1 = y0 + rows;
    for (int i = 0; i < p->np; i++) {
        const TSP_Prim *pr = &p->prims[i];
        if (pr->by1 <= (float)y0 || pr->by0 >= (float)y1) continue;
        if (tsp_draw_prim(p, pr, y0, y1, out, stride)) return -1;
    }
    return rows;
}

/* ── LZW pentru TIFF (ca libtiff: coduri 9..12 biți, MSB întâi) ─────────── */

#define TSP_LZW_CLEAR 256
#define TSP_LZW_EOI   257
#define TSP_LZW_FIRST 258
#define TSP_LZW_MAX   4095
#define TSP_LZW_HSIZE 9001

typedef struct {
    uint8_t *dst; size_t cap, n; int overflow;
    uint32_t data; int bits;
} TSP_Bits;

static void tsp_put(TSP_Bits *b, int code, int nbits) {
    b->data = (b->data << nbits) | (uint32_t)code;
    b->bits += nbits;
    while (b->bits >= 8) {
        if (b->n < b->cap) b->dst[b->n] = (uint8_t)((b->data >> (b->bits - 8)) & 0xFF);
        else b->overflow = 1;
        b->n++;
        b->bits -= 8;
    }
}

long ts_tiff_lzw_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    if (!dst) return -1;
    static const int32_t EMPTY = -1;
    int32_t *hkey = (int32_t *)malloc(TSP_LZW_HSIZE * sizeof(int32_t));
    int16_t *hcode = (int16_t *)malloc(TSP_LZW_HSIZE * sizeof(int16_t));
    if (!hkey || !hcode) { free(hkey); free(hcode); return -1; }
    for (int i = 0; i < TSP_LZW_HSIZE; i++) hkey[i] = EMPTY;
    TSP_Bits b = {dst, cap, 0, 0, 0, 0};
    int nbits = 9, maxcode = 511, free_ent = TSP_LZW_FIRST;
    tsp_put(&b, TSP_LZW_CLEAR, nbits);
    if (n > 0) {
        int ent = src[0];
        for (size_t i = 1; i < n; i++) {
            int c = src[i];
            int32_t key = (int32_t)((c << 12) | ent);
            uint32_t h = ((uint32_t)c << 5 ^ (uint32_t)ent) % TSP_LZW_HSIZE;
            int found = -1;
            while (hkey[h] != EMPTY) {
                if (hkey[h] == key) { found = hcode[h]; break; }
                h = (h + 1) % TSP_LZW_HSIZE;
            }
            if (found >= 0) { ent = found; continue; }
            tsp_put(&b, ent, nbits);
            ent = c;
            hkey[h] = key; hcode[h] = (int16_t)free_ent;
            free_ent++;
            if (free_ent == TSP_LZW_MAX - 1) {                    /* tabela plină: o luăm de la capăt */
                for (int k = 0; k < TSP_LZW_HSIZE; k++) hkey[k] = EMPTY;
                tsp_put(&b, TSP_LZW_CLEAR, nbits);
                free_ent = TSP_LZW_FIRST; nbits = 9; maxcode = 511;
            } else if (free_ent > maxcode) {
                nbits++;
                maxcode = (1 << nbits) - 1;
            }
        }
        tsp_put(&b, ent, nbits);
        free_ent++;
        if (free_ent == TSP_LZW_MAX - 1) {
            tsp_put(&b, TSP_LZW_CLEAR, nbits);
            nbits = 9;
        } else if (free_ent > maxcode) {
            nbits++;
        }
    }
    tsp_put(&b, TSP_LZW_EOI, nbits);
    if (b.bits > 0) {
        if (b.n < b.cap) b.dst[b.n] = (uint8_t)((b.data << (8 - b.bits)) & 0xFF);
        else b.overflow = 1;
        b.n++;
    }
    free(hkey); free(hcode);
    return b.overflow ? -2 : (long)b.n;
}

/* ── TIFF RGB pe fâșii ─────────────────────────────────────────────────── */

static int tsp_w16(FILE *f, uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; return fwrite(b, 1, 2, f) == 2 ? 0 : -1; }
static int tsp_w32(FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}
static int tsp_entry(FILE *f, uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
    if (tsp_w16(f, tag) || tsp_w16(f, type) || tsp_w32(f, count)) return -1;
    if (type == 3 && count == 1) { if (tsp_w16(f, (uint16_t)value) || tsp_w16(f, 0)) return -1; return 0; }
    return tsp_w32(f, value);
}

int ts_plot_write_tiff(TS_Plot *p, const char *path, double dpi, int rows_per_strip, int compression) {
    if (!p || !path) return -1;
    if (compression != 1 && compression != 5) return -1;
    if (!(dpi > 0.0) || !tsp_finite(dpi)) dpi = 300.0;
    int w = p->w, h = p->h;
    int rps = rows_per_strip > 0 ? rows_per_strip : 64;
    if (rps > h) rps = h;
    int nstrips = (h + rps - 1) / rps;
    size_t row_bytes = (size_t)w * 3;
    size_t band_bytes = row_bytes * (size_t)rps;
    size_t zcap = band_bytes + band_bytes / 2 + 4096;
    uint8_t *band = (uint8_t *)malloc(band_bytes);
    uint8_t *z = compression == 5 ? (uint8_t *)malloc(zcap) : NULL;
    uint32_t *offs = (uint32_t *)calloc((size_t)nstrips, sizeof(uint32_t));
    uint32_t *cnts = (uint32_t *)calloc((size_t)nstrips, sizeof(uint32_t));
    FILE *f = fopen(path, "wb");
    int rc = -1;
    if (!band || (compression == 5 && !z) || !offs || !cnts || !f) goto done;
    /* antetul; offsetul IFD se scrie la sfârșit */
    if (fwrite("II*\0", 1, 4, f) != 4 || tsp_w32(f, 0)) goto done;
    uint64_t pos = 8;
    for (int s = 0; s < nstrips; s++) {
        int y0 = s * rps;
        int rows = h - y0 < rps ? h - y0 : rps;
        if (ts_plot_render_rgb(p, y0, rows, band, (int)row_bytes) < 0) goto done;
        const uint8_t *data = band;
        size_t len = row_bytes * (size_t)rows;
        if (compression == 5) {
            long zl = ts_tiff_lzw_encode(band, len, z, zcap);
            if (zl < 0) goto done;
            data = z; len = (size_t)zl;
        }
        if (pos + len > 0xFFFFFFF0ull) goto done;                  /* TIFF clasic: sub 4 GB */
        offs[s] = (uint32_t)pos; cnts[s] = (uint32_t)len;
        if (fwrite(data, 1, len, f) != len) goto done;
        pos += len;
    }
    if (pos & 1) { if (fputc(0, f) == EOF) goto done; pos++; }
    /* valorile care nu încap în intrări */
    static const char software[] = "SoftCAD C+C TOPO";
    uint32_t sw_len = (uint32_t)sizeof(software);                   /* cu NUL */
    uint32_t bps_off = (uint32_t)pos;
    if (tsp_w16(f, 8) || tsp_w16(f, 8) || tsp_w16(f, 8)) goto done;
    pos += 6;
    uint32_t dpi_num = (uint32_t)(dpi * 1000.0 + 0.5), dpi_den = 1000;
    uint32_t xres_off = (uint32_t)pos;
    if (tsp_w32(f, dpi_num) || tsp_w32(f, dpi_den)) goto done;
    pos += 8;
    uint32_t yres_off = (uint32_t)pos;
    if (tsp_w32(f, dpi_num) || tsp_w32(f, dpi_den)) goto done;
    pos += 8;
    uint32_t sw_off = (uint32_t)pos;
    if (fwrite(software, 1, sw_len, f) != sw_len) goto done;
    pos += sw_len;
    if (pos & 1) { if (fputc(0, f) == EOF) goto done; pos++; }
    uint32_t offs_off = 0, cnts_off = 0;
    if (nstrips > 1) {
        offs_off = (uint32_t)pos;
        for (int s = 0; s < nstrips; s++) if (tsp_w32(f, offs[s])) goto done;
        pos += 4ull * (uint64_t)nstrips;
        cnts_off = (uint32_t)pos;
        for (int s = 0; s < nstrips; s++) if (tsp_w32(f, cnts[s])) goto done;
        pos += 4ull * (uint64_t)nstrips;
    }
    uint32_t ifd = (uint32_t)pos;
    const uint16_t nent = 14;
    if (tsp_w16(f, nent)) goto done;
    if (tsp_entry(f, 256, 4, 1, (uint32_t)w) ||                     /* ImageWidth */
        tsp_entry(f, 257, 4, 1, (uint32_t)h) ||                     /* ImageLength */
        tsp_entry(f, 258, 3, 3, bps_off) ||                         /* BitsPerSample 8,8,8 */
        tsp_entry(f, 259, 3, 1, (uint32_t)compression) ||           /* 1 sau 5 (LZW) */
        tsp_entry(f, 262, 3, 1, 2) ||                               /* RGB */
        tsp_entry(f, 273, 4, (uint32_t)nstrips, nstrips > 1 ? offs_off : offs[0]) ||
        tsp_entry(f, 277, 3, 1, 3) ||                               /* SamplesPerPixel */
        tsp_entry(f, 278, 4, 1, (uint32_t)rps) ||                   /* RowsPerStrip */
        tsp_entry(f, 279, 4, (uint32_t)nstrips, nstrips > 1 ? cnts_off : cnts[0]) ||
        tsp_entry(f, 282, 5, 1, xres_off) ||
        tsp_entry(f, 283, 5, 1, yres_off) ||
        tsp_entry(f, 284, 3, 1, 1) ||                               /* PlanarConfiguration */
        tsp_entry(f, 296, 3, 1, 2) ||                               /* inch */
        tsp_entry(f, 305, 2, sw_len, sw_off) ||                     /* Software */
        tsp_w32(f, 0)) goto done;
    if (fseek(f, 4, SEEK_SET) || tsp_w32(f, ifd)) goto done;
    rc = nstrips;
done:
    if (f) { if (fclose(f) != 0) rc = -1; }
    if (rc < 0) remove(path);
    free(band); free(z); free(offs); free(cnts);
    return rc;
}

#endif /* TS_PLOT_RASTER_IMPLEMENTATION */
#endif /* TS_PLOT_RASTER_H */
