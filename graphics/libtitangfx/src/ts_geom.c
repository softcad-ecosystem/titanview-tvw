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
/* ts_geom.c — implementare C99 pură. Build:
 *   gcc -O3 -fPIC -shared -DTS_GEOM_BUILDING -o ts_geom.so ts_geom.c -lm
 */
#define TS_GEOM_BUILDING
#include "ts_geom.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SQRT3 1.7320508075688772
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TS_GEOM_VERSION_STR "0.1.0"

/* ── Arie semnată exactă pe int64 (shoelace) ─────────────────────────── */
TS_GEOM_API int64_t ts_geom_signed_area_i64(const int64_t* xs, const int64_t* ys, int n) {
    int64_t s = 0;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        s += xs[i] * ys[j] - xs[j] * ys[i];
    }
    return s / 2;
}

/* ── Arie absolută în double ─────────────────────────────────────────── */
TS_GEOM_API double ts_geom_polygon_area(const double* xs, const double* ys, int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        s += xs[i] * ys[j] - xs[j] * ys[i];
    }
    return fabs(s) * 0.5;
}

/* ── Point-in-polygon (ray cast) ─────────────────────────────────────── */
TS_GEOM_API int ts_geom_pip(const double* xs, const double* ys, int n,
                            double px, double py) {
    int inside = 0;
    int j = n - 1;
    for (int i = 0; i < n; ++i) {
        double xi = xs[i], yi = ys[i], xj = xs[j], yj = ys[j];
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / ((yj - yi) + 1e-30) + xi)) {
            inside = !inside;
        }
        j = i;
    }
    return inside;
}

/* ── Sutherland-Hodgman: convex clip ─────────────────────────────────── */
/* "Inside" = LA STÎNGA edge-ului e1→e2 (convenție CCW). */
static inline int _inside_left(double px, double py,
                                double ex1, double ey1, double ex2, double ey2) {
    return ((ex2 - ex1) * (py - ey1) - (ey2 - ey1) * (px - ex1)) >= 0.0;
}

static int _line_intersect(double x1, double y1, double x2, double y2,
                           double x3, double y3, double x4, double y4,
                           double* ix, double* iy) {
    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (fabs(denom) < 1e-15) return 0;
    double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    *ix = x1 + t * (x2 - x1);
    *iy = y1 + t * (y2 - y1);
    return 1;
}

TS_GEOM_API int ts_geom_clip_convex(
    const double* sub_xs, const double* sub_ys, int n_sub,
    const double* clip_xs, const double* clip_ys, int n_clip,
    double* out_xs, double* out_ys, int max_out)
{
    /* doi buffer-i de lucru, alternăm */
    double *cur_x = (double*)malloc((size_t)max_out * sizeof(double));
    double *cur_y = (double*)malloc((size_t)max_out * sizeof(double));
    double *nxt_x = (double*)malloc((size_t)max_out * sizeof(double));
    double *nxt_y = (double*)malloc((size_t)max_out * sizeof(double));
    if (!cur_x || !cur_y || !nxt_x || !nxt_y) {
        free(cur_x); free(cur_y); free(nxt_x); free(nxt_y); return -1;
    }
    int n_cur = (n_sub > max_out) ? max_out : n_sub;
    memcpy(cur_x, sub_xs, (size_t)n_cur * sizeof(double));
    memcpy(cur_y, sub_ys, (size_t)n_cur * sizeof(double));

    for (int e = 0; e < n_clip; ++e) {
        double ex1 = clip_xs[e], ey1 = clip_ys[e];
        double ex2 = clip_xs[(e + 1) % n_clip], ey2 = clip_ys[(e + 1) % n_clip];
        int n_new = 0;
        for (int j = 0; j < n_cur; ++j) {
            double cx = cur_x[j], cy = cur_y[j];
            int pi = (j == 0) ? n_cur - 1 : j - 1;
            double px = cur_x[pi], py = cur_y[pi];
            int c_in = _inside_left(cx, cy, ex1, ey1, ex2, ey2);
            int p_in = _inside_left(px, py, ex1, ey1, ex2, ey2);
            if (c_in) {
                if (!p_in) {
                    double ix, iy;
                    if (_line_intersect(px, py, cx, cy, ex1, ey1, ex2, ey2, &ix, &iy)
                        && n_new < max_out) {
                        nxt_x[n_new] = ix; nxt_y[n_new] = iy; n_new++;
                    }
                }
                if (n_new < max_out) { nxt_x[n_new] = cx; nxt_y[n_new] = cy; n_new++; }
            } else if (p_in) {
                double ix, iy;
                if (_line_intersect(px, py, cx, cy, ex1, ey1, ex2, ey2, &ix, &iy)
                    && n_new < max_out) {
                    nxt_x[n_new] = ix; nxt_y[n_new] = iy; n_new++;
                }
            }
        }
        if (n_new == 0) {
            free(cur_x); free(cur_y); free(nxt_x); free(nxt_y);
            return 0;
        }
        /* swap buffers */
        double *tx = cur_x, *ty = cur_y;
        cur_x = nxt_x; cur_y = nxt_y;
        nxt_x = tx; nxt_y = ty;
        n_cur = n_new;
    }
    memcpy(out_xs, cur_x, (size_t)n_cur * sizeof(double));
    memcpy(out_ys, cur_y, (size_t)n_cur * sizeof(double));
    free(cur_x); free(cur_y); free(nxt_x); free(nxt_y);
    return n_cur;
}

/* ── Acoperire poligon ↔ rețea hexagonală (pointy-top, CCW) ──────────── */
static void _hex_vertices_ccw(int q, int r, double hex_size, double* xs, double* ys) {
    double cx = hex_size * SQRT3 * (q + r / 2.0);
    double cy = hex_size * 1.5 * r;
    /* pointy-top: vârfuri la unghiuri (30, 90, 150, 210, 270, 330)° — CCW */
    for (int k = 0; k < 6; ++k) {
        double a = M_PI / 180.0 * (60.0 * k + 30.0);
        xs[k] = cx + hex_size * cos(a);
        ys[k] = cy + hex_size * sin(a);
    }
}

static void _cart_to_axial_round(double x, double y, double hex_size, int* q, int* r) {
    double qf = (SQRT3 / 3.0 * x - 1.0 / 3.0 * y) / hex_size;
    double rf = (2.0 / 3.0 * y) / hex_size;
    double xf = qf, zf = rf, yf = -xf - zf;
    double rxd = round(xf), ryd = round(yf), rzd = round(zf);
    double dx = fabs(rxd - xf), dy = fabs(ryd - yf), dz = fabs(rzd - zf);
    if (dx > dy && dx > dz) rxd = -ryd - rzd;
    else if (dy > dz)        ryd = -rxd - rzd;
    else                     rzd = -rxd - ryd;
    *q = (int)rxd; *r = (int)rzd;
}

TS_GEOM_API int ts_geom_polygon_hex_coverage(
    const double* poly_xs, const double* poly_ys, int n,
    double hex_size,
    int32_t* out_q, int32_t* out_r, int max_hexes)
{
    if (n < 3) return 0;
    double xmin = poly_xs[0], xmax = poly_xs[0];
    double ymin = poly_ys[0], ymax = poly_ys[0];
    for (int i = 1; i < n; ++i) {
        if (poly_xs[i] < xmin) xmin = poly_xs[i];
        if (poly_xs[i] > xmax) xmax = poly_xs[i];
        if (poly_ys[i] < ymin) ymin = poly_ys[i];
        if (poly_ys[i] > ymax) ymax = poly_ys[i];
    }
    int q0, r0, q1, r1;
    _cart_to_axial_round(xmin - hex_size, ymin - hex_size, hex_size, &q0, &r0);
    _cart_to_axial_round(xmax + hex_size, ymax + hex_size, hex_size, &q1, &r1);
    int qmin = (q0 < q1) ? q0 : q1, qmax = (q0 < q1) ? q1 : q0;
    int rmin = (r0 < r1) ? r0 : r1, rmax = (r0 < r1) ? r1 : r0;
    qmin -= 2; qmax += 2; rmin -= 2; rmax += 2;        /* margine */

    int n_out = 0;
    double hex_xs[6], hex_ys[6];
    double clip_xs[64], clip_ys[64];
    const double eps_area = 1e-6;                       /* tolerează zgomot FP */

    for (int r = rmin; r <= rmax && n_out < max_hexes; ++r) {
        for (int q = qmin; q <= qmax && n_out < max_hexes; ++q) {
            _hex_vertices_ccw(q, r, hex_size, hex_xs, hex_ys);
            int nc = ts_geom_clip_convex(poly_xs, poly_ys, n,
                                          hex_xs, hex_ys, 6,
                                          clip_xs, clip_ys, 64);
            if (nc >= 3) {
                double a = ts_geom_polygon_area(clip_xs, clip_ys, nc);
                if (a > eps_area) {
                    out_q[n_out] = q; out_r[n_out] = r; n_out++;
                }
            }
        }
    }
    return n_out;
}

TS_GEOM_API const char* ts_geom_version(void) { return TS_GEOM_VERSION_STR; }
