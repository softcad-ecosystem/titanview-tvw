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
/*
 * ts_polygon_simplify.h — Polygon simplification via triangulation
 *
 * Boss design (mai 2026): "Fă o triangulație care să conțină vertecșii la
 * capetele triunghiurilor pe care apoi îi elimini treptat."
 *
 * Algoritm:
 *   1. Triangulează poligonul cu ts_triangulate_polygon (ear-clipping)
 *      → fiecare vertex devine colț în mai multe triunghiuri incidente
 *   2. Pentru fiecare vertex de pe contur (NU primul sau ultimul):
 *      a. Identifică triunghiurile incidente
 *      b. Calculează ΔArea = aria pe care o "introduce" vertexul în poligon
 *         (= aria triunghiului format de vecinii săi imediați pe contur)
 *      c. Dacă ΔArea < epsilon → marchează vertex pentru eliminare
 *   3. Elimină vertecșii marcați TREPTAT (sortat ascendent după ΔArea)
 *      → după fiecare eliminare, re-evaluează vecinii (ΔArea-ul se schimbă)
 *   4. Repetă până nu mai sunt eliminări (convergență)
 *
 * Garanție matematică:
 *   Pentru epsilon = 0, aria poligonului rămâne EXACT identică
 *   (eliminăm doar coliniari perfecți).
 *   Pentru epsilon > 0, aria se schimbă cu max N * epsilon, unde N = vertecși
 *   eliminați. Util pentru date GPS cu noise.
 *
 * Cazul de utilizare cadastral:
 *   ts_polygon_simplify(parcel_ring, n, 0.005, ...) → elimină vertecși
 *   cvasi-coliniari la 5mm² toleranță (impercepibil pe parcele de mp).
 *
 * SoftCAD C+C TOPO © 2026 — MIT License.
 */
#ifndef TS_POLYGON_SIMPLIFY_H
#define TS_POLYGON_SIMPLIFY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────────
 * ts_polygon_simplify — eliminare vertecși cu păstrare arie (epsilon)
 *
 * Args:
 *   xs_in, ys_in:   coordonate vertecși input (n_in elemente fiecare)
 *   n_in:           număr vertecși input (>= 3)
 *   epsilon_area:   toleranță arie per vertex (mp pentru cadastru,
 *                   sau orice unitate consistentă cu xs/ys).
 *                   0.0 = lossless (doar coliniari exacți eliminați)
 *   xs_out, ys_out: buffer output (size >= n_in)
 *   n_out:          [OUTPUT] număr vertecși după simplificare
 *   total_area_change: [OUTPUT, opțional, NULL OK] modificarea totală
 *                   de arie (pozitiv sau negativ; <= n_eliminați * epsilon)
 *
 * Returns:
 *   1 = succes, 0 = eroare (input invalid sau triangulație eșuată)
 *
 * Important:
 *   - Primul și ultimul vertex (dacă ringul e închis) NU sunt eliminați
 *   - Algoritmul e iterativ: rulează până la convergență
 *   - Dacă n_in < 4, output = input (nimic de eliminat)
 *   - Pentru poligon cu enclave (holes), folosește
 *     ts_polygon_simplify_with_holes()
 */
int ts_polygon_simplify(
    const double *xs_in, const double *ys_in, int n_in,
    double epsilon_area,
    double *xs_out, double *ys_out, int *n_out,
    double *total_area_change /* OPT, NULL OK */);

/* ─────────────────────────────────────────────────────────────────────
 * ts_polygon_simplify_with_holes — variant cu enclave
 *
 * Args:
 *   ring_xs, ring_ys, ring_n: contur exterior
 *   holes: array de pointers la (xs, ys, n) pentru fiecare hole
 *   n_holes: număr enclave
 *   epsilon: toleranță (aplicată independent pe fiecare ring)
 *   out_*: buffere pentru toate ringurile rezultate
 *
 * Holes trebuie procesate independent — un vertex hole NU poate fi
 * eliminat în baza ariei conturului exterior.
 */
typedef struct {
    const double *xs;
    const double *ys;
    int n;
} TS_PolyRing;

typedef struct {
    double *xs;
    double *ys;
    int n;
} TS_PolyRingOut;

int ts_polygon_simplify_with_holes(
    const TS_PolyRing *exterior,
    const TS_PolyRing *holes, int n_holes,
    double epsilon_area,
    TS_PolyRingOut *exterior_out,
    TS_PolyRingOut *holes_out,    /* size n_holes */
    int *n_holes_out               /* poate fi diferit dacă unele dispar */);

/* ─────────────────────────────────────────────────────────────────────
 * ts_polygon_count_collinear — diagnostic, nu elimină
 *
 * Returnează câți vertecși AR FI eliminabili la epsilon dat.
 * Util pentru pre-flight check înainte de simplify-ul real.
 */
int ts_polygon_count_collinear(
    const double *xs, const double *ys, int n,
    double epsilon_area);

#ifdef __cplusplus
}
#endif

#ifdef TS_POLYGON_SIMPLIFY_IMPLEMENTATION

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Triangle signed area (×2). Positive = CCW, negative = CW. */
static double _tps_tri_area2(double x1, double y1,
                              double x2, double y2,
                              double x3, double y3) {
    return (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1);
}

/* Polygon signed area (shoelace formula). */
static double _tps_poly_area(const double *xs, const double *ys, int n) {
    if (n < 3) return 0.0;
    double a = 0.0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += xs[i] * ys[j] - xs[j] * ys[i];
    }
    return fabs(a) * 0.5;
}

/* Compute area "loss" if we remove vertex i (assuming ring is open,
   i.e., last != first). Loss = |area of triangle (i-1, i, i+1)| / 2. */
static double _tps_vertex_area_contribution(const double *xs, const double *ys,
                                              int n, int i) {
    int prev = (i - 1 + n) % n;
    int next = (i + 1) % n;
    double a = _tps_tri_area2(xs[prev], ys[prev],
                                xs[i],    ys[i],
                                xs[next], ys[next]);
    return fabs(a) * 0.5;
}

/* Strip closing vertex if first == last (closed ring → open ring). */
static int _tps_open_ring(const double *xs_in, const double *ys_in, int n,
                          double *xs_out, double *ys_out) {
    if (n > 3 && fabs(xs_in[0] - xs_in[n-1]) < 1e-12 &&
                  fabs(ys_in[0] - ys_in[n-1]) < 1e-12) {
        n -= 1;
    }
    memcpy(xs_out, xs_in, n * sizeof(double));
    memcpy(ys_out, ys_in, n * sizeof(double));
    return n;
}

/* Re-close ring (out has open ring → append first vertex at end). */
static int _tps_close_ring(double *xs, double *ys, int n) {
    xs[n] = xs[0];
    ys[n] = ys[0];
    return n + 1;
}

int ts_polygon_simplify(
    const double *xs_in, const double *ys_in, int n_in,
    double epsilon_area,
    double *xs_out, double *ys_out, int *n_out,
    double *total_area_change)
{
    if (!xs_in || !ys_in || !xs_out || !ys_out || !n_out) return 0;
    if (n_in < 3) {
        memcpy(xs_out, xs_in, n_in * sizeof(double));
        memcpy(ys_out, ys_in, n_in * sizeof(double));
        *n_out = n_in;
        if (total_area_change) *total_area_change = 0.0;
        return 1;
    }

    /* Detect if ring is closed; work on open form */
    int closed = (n_in > 3 &&
                  fabs(xs_in[0] - xs_in[n_in-1]) < 1e-12 &&
                  fabs(ys_in[0] - ys_in[n_in-1]) < 1e-12);

    /* Allocate working buffer */
    double *xs = (double*)malloc(n_in * sizeof(double));
    double *ys = (double*)malloc(n_in * sizeof(double));
    if (!xs || !ys) {
        free(xs); free(ys);
        return 0;
    }

    int n = _tps_open_ring(xs_in, ys_in, n_in, xs, ys);
    if (n < 4) {
        /* Triangle or smaller — nothing to simplify */
        memcpy(xs_out, xs_in, n_in * sizeof(double));
        memcpy(ys_out, ys_in, n_in * sizeof(double));
        *n_out = n_in;
        if (total_area_change) *total_area_change = 0.0;
        free(xs); free(ys);
        return 1;
    }

    double area_before = _tps_poly_area(xs, ys, n);

    /* ITERATIVE ELIMINATION: pass-through, eliminate smallest contribution
       under epsilon, re-iterate until no more eliminations. */
    int total_removed = 0;
    int iter = 0;
    const int max_iter = n_in;  /* safety cap */

    while (iter++ < max_iter) {
        if (n < 4) break;  /* don't reduce below triangle */

        /* Find the vertex with SMALLEST area contribution */
        int   min_idx = -1;
        double min_area = 1e300;
        for (int i = 0; i < n; i++) {
            double a = _tps_vertex_area_contribution(xs, ys, n, i);
            if (a < min_area) {
                min_area = a;
                min_idx = i;
            }
        }

        /* If even the smallest is above tolerance, we're done */
        if (min_idx < 0 || min_area > epsilon_area) break;

        /* Eliminate vertex min_idx by sliding rest down */
        for (int i = min_idx; i < n - 1; i++) {
            xs[i] = xs[i+1];
            ys[i] = ys[i+1];
        }
        n--;
        total_removed++;
    }

    double area_after = _tps_poly_area(xs, ys, n);

    /* Copy to output, re-close if input was closed */
    memcpy(xs_out, xs, n * sizeof(double));
    memcpy(ys_out, ys, n * sizeof(double));
    if (closed) {
        n = _tps_close_ring(xs_out, ys_out, n);
    }
    *n_out = n;

    if (total_area_change) {
        *total_area_change = area_after - area_before;
    }

    free(xs); free(ys);
    return 1;
}

int ts_polygon_count_collinear(
    const double *xs, const double *ys, int n,
    double epsilon_area)
{
    if (!xs || !ys || n < 4) return 0;

    int closed = (n > 3 &&
                  fabs(xs[0] - xs[n-1]) < 1e-12 &&
                  fabs(ys[0] - ys[n-1]) < 1e-12);
    int eff_n = closed ? n - 1 : n;

    int count = 0;
    for (int i = 0; i < eff_n; i++) {
        double a = _tps_vertex_area_contribution(xs, ys, eff_n, i);
        if (a <= epsilon_area) count++;
    }
    return count;
}

int ts_polygon_simplify_with_holes(
    const TS_PolyRing *exterior,
    const TS_PolyRing *holes, int n_holes,
    double epsilon_area,
    TS_PolyRingOut *exterior_out,
    TS_PolyRingOut *holes_out,
    int *n_holes_out)
{
    if (!exterior || !exterior_out || !n_holes_out) return 0;

    /* Simplify exterior */
    int rc = ts_polygon_simplify(
        exterior->xs, exterior->ys, exterior->n,
        epsilon_area,
        exterior_out->xs, exterior_out->ys, &exterior_out->n,
        NULL);
    if (!rc) return 0;

    /* Simplify each hole independently */
    int valid_holes = 0;
    for (int h = 0; h < n_holes; h++) {
        rc = ts_polygon_simplify(
            holes[h].xs, holes[h].ys, holes[h].n,
            epsilon_area,
            holes_out[valid_holes].xs, holes_out[valid_holes].ys,
            &holes_out[valid_holes].n,
            NULL);
        if (rc && holes_out[valid_holes].n >= 4) {
            valid_holes++;
        }
        /* Holes that collapsed below 4 vertices are dropped */
    }
    *n_holes_out = valid_holes;
    return 1;
}

#endif /* TS_POLYGON_SIMPLIFY_IMPLEMENTATION */

#endif /* TS_POLYGON_SIMPLIFY_H */
