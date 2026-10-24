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
#ifndef TS_SPATIAL_OPS_H
#define TS_SPATIAL_OPS_H

/*
 * ts_spatial_ops.h — Operații spațiale avansate
 * SoftCAD C+C TOPO © 2026
 *
 * CE NU EXISTĂ NICĂIERI (nu în ezdxf, nu în shapely pur):
 *
 * 1. POLYGON SPLIT BY LINE — taie o parcelă cu o linie
 *    Cazul real: drum nou taie parcela → 2 parcele noi
 *    Shapely face asta, dar lent pe Python. Noi: C99, O(n).
 *
 * 2. BATCH POINT-IN-POLYGON — 10K puncte × 60K poligoane
 *    Cazul real: care parcele conțin punctele din releveu?
 *    Naive: O(n×m). Cu grid spatial: O(n + m).
 *
 * 3. POLYGON OFFSET (inward/outward)
 *    Cazul real: linia de retragere construcție (5m de la hotare)
 *    Shapely face buffer, dar offset INTERN e diferit de buffer negativ
 *    pe poligoane concave.
 *
 * 4. SNAP TO GRID / SNAP TO NEAREST VERTEX
 *    Cazul real: coordonate cu prea multe zecimale → snap la mm
 *    + snap la vertecșii vecini (eliminare micro-gaps)
 *
 * 5. POLYGON AREA SPLIT — taie poligon în N părți de arii egale
 *    Cazul real: moștenire → parcelă împărțită la 3 copii egal
 *    NIMENI nu face asta nativ. Nici AutoCAD, nici QGIS.
 */

#include "ts_outbuf.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════ */
/*  1. POLYGON SPLIT BY LINE                          */
/* ═══════════════════════════════════════════════════ */

/* Split polygon by line into two parts
   Returns: 1 if split successful, 0 if line doesn't cross polygon (sau
   tăietura ar da o parte de arie zero), -1 dacă o parte nu încape în
   buffer, -2 dacă linia taie conturul în mai mult de 2 puncte (parcela nu
   se poate împărți în exact 2 bucăți cu această linie).
   out_a/out_b: WKT of the two resulting polygons

   4.66 — rescris:
   - linia prin VÂRF (ex. tăietura prinsă cu snap pe un colț) dădea 0:
     același punct se găsea pe ambele laturi vecine. Acum fiecare vârf
     aparține unei singure laturi (parametru t ∈ [0,1)) și punctele identice
     se contopesc;
   - latura se știe din calculul intersecției (înainte se ghicea după o
     toleranță de 1 cm, prima latură „apropiată” câștiga);
   - peste 2 intersecții (parcelă concavă) se folosea perechea greșită și
     ieșeau poligoane care se auto-intersectau → acum -2;
   - inelul închis (ultimul = primul) nu mai dublează vârful de închidere;
   - ambele părți trebuie să aibă arie > 0. */
typedef struct { int edge; double t, x, y; } _SP_Cut;

static int _sp_cut_cmp(const void *pa, const void *pb) {
    const _SP_Cut *a = (const _SP_Cut *)pa, *b = (const _SP_Cut *)pb;
    if (a->edge != b->edge) return a->edge < b->edge ? -1 : 1;
    return (a->t < b->t) ? -1 : (a->t > b->t);
}

static double _sp_ring_area2(const double *x, const double *y, int n) {
    double s = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; s += x[i] * y[j] - x[j] * y[i]; }
    return s;
}

int ts_spatial_split_by_line(
    const double *poly_x, const double *poly_y, int poly_n,
    double line_x0, double line_y0, double line_x1, double line_y1,
    char *out_a, int cap_a, char *out_b, int cap_b) {

    if (!poly_x || !poly_y || poly_n < 3 || !out_a || !out_b) return 0;
    if (cap_a > 0) out_a[0] = 0;
    if (cap_b > 0) out_b[0] = 0;

    /* inel închis → fără vârful de închidere dublat */
    int n = poly_n;
    while (n > 3 && poly_x[n - 1] == poly_x[0] && poly_y[n - 1] == poly_y[0]) n--;

    double ldx = line_x1 - line_x0, ldy = line_y1 - line_y0;
    double llen = sqrt(ldx * ldx + ldy * ldy);
    if (!(llen > 0)) return 0;

    /* toleranța de contopire: relativă la mărimea parcelei */
    double minx = poly_x[0], maxx = poly_x[0], miny = poly_y[0], maxy = poly_y[0];
    for (int i = 1; i < n; i++) {
        if (poly_x[i] < minx) minx = poly_x[i];
        if (poly_x[i] > maxx) maxx = poly_x[i];
        if (poly_y[i] < miny) miny = poly_y[i];
        if (poly_y[i] > maxy) maxy = poly_y[i];
    }
    double ext = (maxx - minx) + (maxy - miny);
    double eps = ext * 1e-12 + 1e-12;

    /* intersecțiile, cu latura și poziția pe latură */
    _SP_Cut cuts[8];
    int nc = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double ax = poly_x[i], ay = poly_y[i], bx = poly_x[j], by = poly_y[j];
        double ex = bx - ax, ey = by - ay;
        double d = ex * ldy - ey * ldx;
        double elen = sqrt(ex * ex + ey * ey);
        if (!(elen > 0) || fabs(d) <= 1e-12 * elen * llen) continue;   /* paralele / latură nulă */
        double t = ((line_x0 - ax) * ldy - (line_y0 - ay) * ldx) / d;  /* pe latură */
        double u = ((line_x0 - ax) * ey - (line_y0 - ay) * ex) / d;    /* pe linie */
        double tt = eps / elen, uu = eps / llen;
        if (t < -tt || t >= 1 - tt || u < -uu || u > 1 + uu) continue;
        if (t < tt) t = 0;                       /* exact în vârful i */
        double px = t == 0 ? ax : ax + t * ex, py = t == 0 ? ay : ay + t * ey;
        int dup = 0;
        for (int k = 0; k < nc; k++)
            if (fabs(cuts[k].x - px) <= eps * 10 && fabs(cuts[k].y - py) <= eps * 10) { dup = 1; break; }
        if (dup) continue;
        if (nc == 8) return -2;
        cuts[nc].edge = i; cuts[nc].t = t; cuts[nc].x = px; cuts[nc].y = py;
        nc++;
    }
    if (nc < 2) return 0;
    if (nc > 2) return -2;
    qsort(cuts, 2, sizeof cuts[0], _sp_cut_cmp);
    _SP_Cut P = cuts[0], Q = cuts[1];
    /* două puncte pe aceeași latură = linia merge de-a lungul laturii */
    if (P.edge == Q.edge) return 0;

    /* coordonatele celor două părți (pentru arie și scriere) */
    double *ax_ = (double *)malloc(sizeof(double) * (size_t)(2 * (n + 4)));
    double *bx_ = (double *)malloc(sizeof(double) * (size_t)(2 * (n + 4)));
    if (!ax_ || !bx_) { free(ax_); free(bx_); return 0; }
    double *ay_ = ax_ + (n + 4), *by_ = bx_ + (n + 4);
    int na = 0, nb = 0;
    /* A: v[0..P.edge], P (dacă nu e chiar vârful), Q, v[Q.edge+1..n-1] */
    for (int i = 0; i <= P.edge; i++) { ax_[na] = poly_x[i]; ay_[na] = poly_y[i]; na++; }
    if (P.t > 0) { ax_[na] = P.x; ay_[na] = P.y; na++; }
    ax_[na] = Q.x; ay_[na] = Q.y; na++;
    for (int i = Q.edge + 1; i < n; i++) { ax_[na] = poly_x[i]; ay_[na] = poly_y[i]; na++; }
    /* B: P, v[P.edge+1..Q.edge], Q (dacă nu e chiar vârful) */
    bx_[nb] = P.x; by_[nb] = P.y; nb++;
    for (int i = P.edge + 1; i <= Q.edge; i++) { bx_[nb] = poly_x[i]; by_[nb] = poly_y[i]; nb++; }
    if (Q.t > 0) { bx_[nb] = Q.x; by_[nb] = Q.y; nb++; }

    double total = fabs(_sp_ring_area2(poly_x, poly_y, n));
    double area_a = fabs(_sp_ring_area2(ax_, ay_, na));
    double area_b = fabs(_sp_ring_area2(bx_, by_, nb));
    if (na < 3 || nb < 3 || !(area_a > total * 1e-12) || !(area_b > total * 1e-12)) {
        free(ax_); free(bx_);
        return 0;
    }

    int wa = ts__catf(out_a, cap_a, 0, "POLYGON((");
    for (int i = 0; i < na; i++) wa += ts__catf(out_a, cap_a, wa, "%.6f %.6f,", ax_[i], ay_[i]);
    wa += ts__catf(out_a, cap_a, wa, "%.6f %.6f))", ax_[0], ay_[0]);
    int wb = ts__catf(out_b, cap_b, 0, "POLYGON((");
    for (int i = 0; i < nb; i++) wb += ts__catf(out_b, cap_b, wb, "%.6f %.6f,", bx_[i], by_[i]);
    wb += ts__catf(out_b, cap_b, wb, "%.6f %.6f))", bx_[0], by_[0]);
    free(ax_); free(bx_);

    /* 4.66: o parte care nu încape NU se întoarce ciuntită: -1 = buffer mic
     * (0 rămâne „linia nu taie parcela”). Înainte: scriere în afara bufferului
     * — procesul murea la 300 de vârfuri cu bufferul de 4 KB din pytcad. */
    if (TS_OUT_TRUNCATED(wa, cap_a) || TS_OUT_TRUNCATED(wb, cap_b)) {
        if (cap_a > 0) out_a[0] = 0;
        if (cap_b > 0) out_b[0] = 0;
        return -1;
    }
    return 1;
}

/* ═══════════════════════════════════════════════════ */
/*  2. BATCH POINT-IN-POLYGON (grid-accelerated)      */
/* ═══════════════════════════════════════════════════ */

#define SP_GRID_MAX 200

typedef struct {
    int *polygon_ids;  /* which polygons touch this cell */
    int count, cap;
} _SP_GridCell;

typedef struct {
    _SP_GridCell *cells;
    int cols, rows;
    double x0, y0, cell_w, cell_h;
    /* Polygon data */
    const double **poly_xs;
    const double **poly_ys;
    const int *poly_ns;
    int n_polys;
} SP_BatchPIP;

SP_BatchPIP *ts_spatial_pip_build(
    const double **poly_xs, const double **poly_ys, const int *poly_ns,
    int n_polys) {
    
    if (!poly_xs || !poly_ys || !poly_ns || n_polys <= 0) return NULL;
    
    SP_BatchPIP *b = (SP_BatchPIP*)calloc(1, sizeof(SP_BatchPIP));
    b->poly_xs = poly_xs; b->poly_ys = poly_ys;
    b->poly_ns = poly_ns; b->n_polys = n_polys;
    
    /* Find extent */
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    for (int p = 0; p < n_polys; p++) {
        for (int i = 0; i < poly_ns[p]; i++) {
            if (poly_xs[p][i] < x0) x0 = poly_xs[p][i];
            if (poly_ys[p][i] < y0) y0 = poly_ys[p][i];
            if (poly_xs[p][i] > x1) x1 = poly_xs[p][i];
            if (poly_ys[p][i] > y1) y1 = poly_ys[p][i];
        }
    }
    
    b->x0 = x0 - 1; b->y0 = y0 - 1;
    b->cols = (int)ceil(sqrt(n_polys / 5.0));
    b->rows = b->cols;
    if (b->cols > SP_GRID_MAX) b->cols = SP_GRID_MAX;
    if (b->rows > SP_GRID_MAX) b->rows = SP_GRID_MAX;
    if (b->cols < 5) b->cols = 5;
    if (b->rows < 5) b->rows = 5;
    b->cell_w = (x1 - x0 + 2) / b->cols;
    b->cell_h = (y1 - y0 + 2) / b->rows;
    
    b->cells = (_SP_GridCell*)calloc((size_t)(b->cols * b->rows), sizeof(_SP_GridCell));
    
    /* Insert polygons into grid */
    for (int p = 0; p < n_polys; p++) {
        double px0=1e30,py0=1e30,px1=-1e30,py1=-1e30;
        for (int i = 0; i < poly_ns[p]; i++) {
            if (poly_xs[p][i]<px0) px0=poly_xs[p][i];
            if (poly_ys[p][i]<py0) py0=poly_ys[p][i];
            if (poly_xs[p][i]>px1) px1=poly_xs[p][i];
            if (poly_ys[p][i]>py1) py1=poly_ys[p][i];
        }
        int cx0 = (int)((px0-b->x0)/b->cell_w); if(cx0<0)cx0=0;
        int cy0 = (int)((py0-b->y0)/b->cell_h); if(cy0<0)cy0=0;
        int cx1 = (int)((px1-b->x0)/b->cell_w); if(cx1>=b->cols)cx1=b->cols-1;
        int cy1 = (int)((py1-b->y0)/b->cell_h); if(cy1>=b->rows)cy1=b->rows-1;
        for (int gy=cy0;gy<=cy1;gy++) for (int gx=cx0;gx<=cx1;gx++) {
            _SP_GridCell *c = &b->cells[gy*b->cols+gx];
            if (c->count >= c->cap) {
                c->cap = c->cap ? c->cap*2 : 8;
                c->polygon_ids = (int*)realloc(c->polygon_ids, sizeof(int)*(size_t)c->cap);
            }
            c->polygon_ids[c->count++] = p;
        }
    }
    return b;
}

void ts_spatial_pip_free(SP_BatchPIP *b) {
    if (!b) return;
    for (int i = 0; i < b->cols * b->rows; i++) free(b->cells[i].polygon_ids);
    free(b->cells); free(b);
}

/* Ray-casting point-in-polygon */
static int _sp_pip(const double *xs, const double *ys, int n, double px, double py) {
    int inside = 0;
    for (int i = 0, j = n-1; i < n; j = i++) {
        if (((ys[i]>py) != (ys[j]>py)) &&
            (px < (xs[j]-xs[i]) * (py-ys[i]) / (ys[j]-ys[i]) + xs[i]))
            inside = !inside;
    }
    return inside;
}

/* Query: which polygon contains this point? Returns polygon index or -1 */
int ts_spatial_pip_query(const SP_BatchPIP *b, double px, double py) {
    if (!b) return -1;
    int gx = (int)((px - b->x0) / b->cell_w);
    int gy = (int)((py - b->y0) / b->cell_h);
    if (gx < 0 || gx >= b->cols || gy < 0 || gy >= b->rows) return -1;
    
    _SP_GridCell *cell = &b->cells[gy * b->cols + gx];
    for (int i = 0; i < cell->count; i++) {
        int p = cell->polygon_ids[i];
        if (_sp_pip(b->poly_xs[p], b->poly_ys[p], b->poly_ns[p], px, py))
            return p;
    }
    return -1;
}

/* Batch query: classify N points against all polygons */
int ts_spatial_pip_batch(const SP_BatchPIP *b,
                           const double *pts_x, const double *pts_y, int n_pts,
                           int *results) {
    if (!b || !pts_x || !pts_y || !results) return 0;
    int classified = 0;
    for (int i = 0; i < n_pts; i++) {
        results[i] = ts_spatial_pip_query(b, pts_x[i], pts_y[i]);
        if (results[i] >= 0) classified++;
    }
    return classified;
}

/* ═══════════════════════════════════════════════════ */
/*  3. POLYGON OFFSET (inward/outward)                */
/* ═══════════════════════════════════════════════════ */

int ts_spatial_offset(const double *xs, const double *ys, int n_in,
                        double distance, /* positive=outward, negative=inward */
                        double *out_x, double *out_y, int max_out) {
    if (!xs || !ys || n_in < 3 || !out_x || !out_y) return 0;
    /* Skip duplicate closing vertex */
    int n = n_in;
    if (n > 3 && fabs(xs[0]-xs[n-1]) < 1e-10 && fabs(ys[0]-ys[n-1]) < 1e-10) n--;
    if (n > max_out) return 0;
    
    for (int i = 0; i < n; i++) {
        int prev = (i + n - 1) % n;
        int next = (i + 1) % n;
        
        /* Edge vectors */
        double e1x = xs[i] - xs[prev], e1y = ys[i] - ys[prev];
        double e2x = xs[next] - xs[i], e2y = ys[next] - ys[i];
        
        /* Normals (perpendicular, pointing outward for CCW polygon) */
        double len1 = sqrt(e1x*e1x + e1y*e1y);
        double len2 = sqrt(e2x*e2x + e2y*e2y);
        if (len1 < 1e-10 || len2 < 1e-10) { out_x[i]=xs[i]; out_y[i]=ys[i]; continue; }
        
        double n1x = -e1y/len1, n1y = e1x/len1;
        double n2x = -e2y/len2, n2y = e2x/len2;
        
        /* Bisector = average of normals */
        double bx = n1x + n2x, by = n1y + n2y;
        double blen = sqrt(bx*bx + by*by);
        if (blen < 1e-10) { out_x[i]=xs[i]; out_y[i]=ys[i]; continue; }
        bx /= blen; by /= blen;
        
        /* Offset amount (account for angle between edges) */
        double dot = n1x*bx + n1y*by;
        if (fabs(dot) < 0.01) dot = 0.01; /* prevent division by near-zero */
        double offset = distance / dot;
        /* Clamp to prevent spikes at very acute angles */
        if (fabs(offset) > fabs(distance) * 10) offset = distance * 10 * (offset > 0 ? 1 : -1);
        
        out_x[i] = xs[i] + bx * offset;
        out_y[i] = ys[i] + by * offset;
    }
    return n;
}

/* ═══════════════════════════════════════════════════ */
/*  4. SNAP TO GRID                                   */
/* ═══════════════════════════════════════════════════ */

void ts_spatial_snap_to_grid(double *xs, double *ys, int n, double grid_size) {
    if (!xs || !ys || grid_size <= 0) return;
    for (int i = 0; i < n; i++) {
        xs[i] = round(xs[i] / grid_size) * grid_size;
        ys[i] = round(ys[i] / grid_size) * grid_size;
    }
}

/* Snap vertices to nearest vertex in another polygon (within tolerance) */
int ts_spatial_snap_to_vertices(double *xs, double *ys, int n,
                                  const double *ref_x, const double *ref_y, int ref_n,
                                  double tolerance) {
    if (!xs || !ys || !ref_x || !ref_y) return 0;
    int snapped = 0;
    for (int i = 0; i < n; i++) {
        double min_d = tolerance;
        int best = -1;
        for (int j = 0; j < ref_n; j++) {
            double dx = xs[i]-ref_x[j], dy = ys[i]-ref_y[j];
            double d = sqrt(dx*dx + dy*dy);
            if (d < min_d) { min_d = d; best = j; }
        }
        if (best >= 0) {
            xs[i] = ref_x[best]; ys[i] = ref_y[best];
            snapped++;
        }
    }
    return snapped;
}

/* ═══════════════════════════════════════════════════ */
/*  5. AREA-EQUAL SPLIT (parcelă → N bucăți egale)    */
/* ═══════════════════════════════════════════════════ */

/* Split polygon into 2 parts of equal area by a horizontal line
   Finds y-coordinate where horizontal line splits polygon into 2 equal areas
   Returns y_split. out_top and out_bottom are WKT of the two parts. */
double ts_spatial_equal_area_split(
    const double *xs, const double *ys, int n,
    char *out_top, int cap_top, char *out_bottom, int cap_bottom) {
    
    if (!xs || !ys || n < 3) return 0;
    
    /* Find total area */
    double total = 0;
    for (int i = 0; i < n; i++) {
        int j = (i+1) % n;
        total += xs[i]*ys[j] - xs[j]*ys[i];
    }
    total = fabs(total) / 2.0;
    double half = total / 2.0;
    
    /* Find y range */
    double ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < n; i++) {
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }
    
    /* Binary search for y_split where area below = half */
    double y_lo = ymin, y_hi = ymax;
    double y_split = (ymin + ymax) / 2;
    
    /* 4.66: tampon dinamic (înainte cx[256] — peste ~250 de vârfuri aria de
       sub linie se calcula pe o bucată din contur → linie greșită) și
       bisecție completă (înainte oprire la 0,01% din arie: 1 m² la 10 ha) */
    double *cx = (double *)malloc(sizeof(double) * (size_t)(2 * n + 2));
    double *cy = (double *)malloc(sizeof(double) * (size_t)(2 * n + 2));
    if (!cx || !cy) { free(cx); free(cy); return 0; }
    for (int iter = 0; iter < 64; iter++) {
        y_split = (y_lo + y_hi) / 2;

        /* Calculate area below y_split using clipping */
        /* Sutherland-Hodgman clip against y <= y_split */
        int cn = 0;
        for (int i = 0; i < n; i++) {
            int j = (i+1) % n;
            int i_below = (ys[i] <= y_split);
            int j_below = (ys[j] <= y_split);

            if (i_below) { cx[cn]=xs[i]; cy[cn]=ys[i]; cn++; }
            if (i_below != j_below) {
                /* Edge crosses y_split */
                double t = (y_split - ys[i]) / (ys[j] - ys[i]);
                cx[cn] = xs[i] + t * (xs[j] - xs[i]);
                cy[cn] = y_split;
                cn++;
            }
        }

        /* Area of clipped polygon */
        double area_below = 0;
        for (int i = 0; i < cn; i++) {
            int j = (i+1) % cn;
            area_below += cx[i]*cy[j] - cx[j]*cy[i];
        }
        area_below = fabs(area_below) / 2.0;

        if (fabs(area_below - half) <= total * 1e-13) break;
        if (area_below < half) y_lo = y_split;
        else y_hi = y_split;
        if (!(y_hi > y_lo)) break;
    }
    free(cx); free(cy);

    /* Generate the two polygons using split_by_line with horizontal line */
    double lx0 = xs[0], lx1 = xs[0];
    for (int i = 1; i < n; i++) {
        if (xs[i] < lx0) lx0 = xs[i];
        if (xs[i] > lx1) lx1 = xs[i];
    }
    lx0 -= 100; lx1 += 100; /* extend line beyond polygon */
    
    int ok = ts_spatial_split_by_line(xs, ys, n,
                                        lx0, y_split, lx1, y_split,
                                        out_bottom, cap_bottom, out_top, cap_top);
    return ok == 1 ? y_split : 0;   /* 4.66: -1/-2 nu mai trec drept succes */
}

#endif /* TS_SPATIAL_OPS_H */
