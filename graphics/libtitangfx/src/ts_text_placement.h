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
#ifndef TS_TEXT_PLACEMENT_H
#define TS_TEXT_PLACEMENT_H

/*
 * ts_text_placement.h — Plasare milimetrică text în geometrie
 * SoftCAD C+C TOPO © 2026
 *
 * Problema: unde pui textul în parcelă să fie vizibil, centrat,
 * și să NU iasă din geometrie? Niciun soft nu face asta corect
 * pe poligoane concave, L-shape, sau înguste.
 *
 * Soluția: "Pole of Inaccessibility" — punctul cel mai depărtat
 * de orice margine. Acolo textul are cel mai mult spațiu.
 * Plus: calcul lățime disponibilă la acel punct pentru font sizing.
 */

#include "ts_outbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



/* ═══════════════════════════════════════════════════════════════ */
/*  POLE OF INACCESSIBILITY (Mapbox/polylabel algorithm)          */
/*  Punctul interior cel mai depărtat de orice margine             */
/* ═══════════════════════════════════════════════════════════════ */

/* Signed distance from point to polygon edge (negative = inside) */
static double _point_to_edge_dist(double px, double py,
                                    const double *ring_x, const double *ring_y, int n) {
    double min_d = 1e30;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double ax = ring_x[i], ay = ring_y[i];
        double bx = ring_x[j], by = ring_y[j];
        double dx = bx - ax, dy = by - ay;
        double len2 = dx*dx + dy*dy;
        double t = len2 > 0 ? ((px-ax)*dx + (py-ay)*dy) / len2 : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double nx = ax + t*dx, ny = ay + t*dy;
        double d = sqrt((px-nx)*(px-nx) + (py-ny)*(py-ny));
        if (d < min_d) min_d = d;
    }
    return min_d;
}

/* Point-in-polygon (ray casting) */
static int _pip(double px, double py, const double *rx, const double *ry, int n) {
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((ry[i] > py) != (ry[j] > py)) &&
            (px < (rx[j]-rx[i]) * (py-ry[i]) / (ry[j]-ry[i]) + rx[i]))
            inside = !inside;
    }
    return inside;
}

/* Cell for quadtree search */
typedef struct {
    double cx, cy, half;  /* center + half-size */
    double dist;          /* distance to nearest edge */
    double max_dist;      /* upper bound = dist + half*sqrt(2) */
} _PoICell;

/* Compare cells by max_dist (descending) for priority queue */
static int _poi_cmp(const void *a, const void *b) {
    double da = ((_PoICell*)b)->max_dist - ((_PoICell*)a)->max_dist;
    return da > 0 ? 1 : (da < 0 ? -1 : 0);
}

/*
 * ts_text_pole — Find the "pole of inaccessibility"
 * The point inside the polygon furthest from any edge.
 * Returns: distance to nearest edge (= max inscribed circle radius)
 *
 * Algorithm: iterative grid refinement (Mapbox polylabel approach)
 * Precision: continues until cell size < precision parameter
 */
double ts_text_pole(const double *ring_x, const double *ring_y, int n,
                     double precision, double *out_x, double *out_y) {
    if (!ring_x || !ring_y || n < 3) return 0;

    /* Bounding box */
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    for (int i = 0; i < n; i++) {
        if (ring_x[i] < x0) x0 = ring_x[i]; if (ring_y[i] < y0) y0 = ring_y[i];
        if (ring_x[i] > x1) x1 = ring_x[i]; if (ring_y[i] > y1) y1 = ring_y[i];
    }

    double width = x1 - x0, height = y1 - y0;
    double cell_size = (width > height ? width : height);
    if (cell_size < precision) {
        if (out_x) *out_x = (x0 + x1) / 2;
        if (out_y) *out_y = (y0 + y1) / 2;
        return 0;
    }
    double half = cell_size / 2;

    /* Start with centroid as initial best */
    double best_x = 0, best_y = 0, best_d = -1e30;
    double cx = 0, cy = 0;
    double area2 = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double a = ring_x[i]*ring_y[j] - ring_x[j]*ring_y[i];
        area2 += a;
        cx += (ring_x[i] + ring_x[j]) * a;
        cy += (ring_y[i] + ring_y[j]) * a;
    }
    if (fabs(area2) > 1e-10) { cx /= 3*area2; cy /= 3*area2; }
    else { cx = (x0+x1)/2; cy = (y0+y1)/2; }

    if (_pip(cx, cy, ring_x, ring_y, n)) {
        best_d = _point_to_edge_dist(cx, cy, ring_x, ring_y, n);
        best_x = cx; best_y = cy;
    }

    /* Priority queue (simple array + sort) */
    int cap = 4096, count = 0;
    _PoICell *queue = (_PoICell*)malloc(sizeof(_PoICell) * (size_t)cap);

    /* Initial grid */
    for (double gx = x0; gx < x1; gx += cell_size) {
        for (double gy = y0; gy < y1; gy += cell_size) {
            double ccx = gx + half, ccy = gy + half;
            double d = _pip(ccx, ccy, ring_x, ring_y, n) ?
                       _point_to_edge_dist(ccx, ccy, ring_x, ring_y, n) : -_point_to_edge_dist(ccx, ccy, ring_x, ring_y, n);
            if (count >= cap) { cap *= 2; queue = (_PoICell*)realloc(queue, sizeof(_PoICell)*(size_t)cap); }
            queue[count].cx = ccx; queue[count].cy = ccy; queue[count].half = half;
            queue[count].dist = d; queue[count].max_dist = d + half * 1.41421356;
            count++;
            if (d > best_d) { best_d = d; best_x = ccx; best_y = ccy; }
        }
    }

    /* Refine */
    int iterations = 0;
    while (count > 0 && iterations < 2000) {
        iterations++;
        qsort(queue, (size_t)count, sizeof(_PoICell), _poi_cmp);

        _PoICell best_cell = queue[0];
        /* Remove top */
        queue[0] = queue[--count];

        if (best_cell.max_dist - best_d <= precision) continue;
        if (best_cell.half < precision) continue;

        /* Subdivide */
        double h2 = best_cell.half / 2;
        double offsets[4][2] = {{-h2,-h2},{h2,-h2},{-h2,h2},{h2,h2}};
        for (int q = 0; q < 4; q++) {
            double qx = best_cell.cx + offsets[q][0];
            double qy = best_cell.cy + offsets[q][1];
            int inside = _pip(qx, qy, ring_x, ring_y, n);
            double d = inside ?
                _point_to_edge_dist(qx, qy, ring_x, ring_y, n) :
                -_point_to_edge_dist(qx, qy, ring_x, ring_y, n);

            if (count >= cap) { cap *= 2; queue = (_PoICell*)realloc(queue, sizeof(_PoICell)*(size_t)cap); }
            queue[count].cx = qx; queue[count].cy = qy; queue[count].half = h2;
            queue[count].dist = d; queue[count].max_dist = d + h2 * 1.41421356;
            count++;

            if (d > best_d) { best_d = d; best_x = qx; best_y = qy; }
        }
    }

    free(queue);
    if (out_x) *out_x = best_x;
    if (out_y) *out_y = best_y;
    return best_d;
}

/*
 * ts_text_width_at — Lățimea disponibilă la un punct interior
 * Scanează orizontal de la punct spre stânga/dreapta până lovește margine.
 * Returns: lățime disponibilă (pentru font sizing)
 */
double ts_text_width_at(const double *ring_x, const double *ring_y, int n,
                          double px, double py) {
    if (!_pip(px, py, ring_x, ring_y, n)) return 0;

    /* Scan left */
    double left_d = 1e30;
    for (int i = 0, j = n-1; i < n; j = i++) {
        double ay = ring_y[i], by = ring_y[j];
        if ((ay > py) == (by > py)) continue; /* doesn't cross horizontal */
        double ax = ring_x[i], bx = ring_x[j];
        double t = (py - ay) / (by - ay);
        double ix = ax + t * (bx - ax);
        if (ix < px && px - ix < left_d) left_d = px - ix;
    }

    /* Scan right */
    double right_d = 1e30;
    for (int i = 0, j = n-1; i < n; j = i++) {
        double ay = ring_y[i], by = ring_y[j];
        if ((ay > py) == (by > py)) continue;
        double ax = ring_x[i], bx = ring_x[j];
        double t = (py - ay) / (by - ay);
        double ix = ax + t * (bx - ax);
        if (ix > px && ix - px < right_d) right_d = ix - px;
    }

    if (left_d > 1e20 || right_d > 1e20) return 0;
    return left_d + right_d;
}

/*
 * ts_text_height_at — Înălțimea disponibilă la un punct interior
 * Scanează vertical de la punct în sus/jos.
 */
double ts_text_height_at(const double *ring_x, const double *ring_y, int n,
                           double px, double py) {
    if (!_pip(px, py, ring_x, ring_y, n)) return 0;

    double up_d = 1e30, down_d = 1e30;
    for (int i = 0, j = n-1; i < n; j = i++) {
        double ax = ring_x[i], bx = ring_x[j];
        if ((ax > px) == (bx > px)) continue;
        double ay = ring_y[i], by = ring_y[j];
        double t = (px - ax) / (bx - ax);
        double iy = ay + t * (by - ay);
        if (iy > py && iy - py < up_d) up_d = iy - py;
        if (iy < py && py - iy < down_d) down_d = py - iy;
    }

    if (up_d > 1e20 || down_d > 1e20) return 0;
    return up_d + down_d;
}

/*
 * ts_text_fit — Calculează font size optim pentru text în geometrie
 * Returns: font size recomandat (în unități coordonate)
 */
double ts_text_fit(const double *ring_x, const double *ring_y, int n,
                    const char *text, double *out_x, double *out_y) {
    double pole_x, pole_y;
    double radius = ts_text_pole(ring_x, ring_y, n, 0.1, &pole_x, &pole_y);
    if (radius <= 0) return 0;

    double avail_w = ts_text_width_at(ring_x, ring_y, n, pole_x, pole_y);
    double avail_h = ts_text_height_at(ring_x, ring_y, n, pole_x, pole_y);

    int text_len = text ? (int)strlen(text) : 1;
    if (text_len < 1) text_len = 1;

    /* Font size: width / text_length * factor, capped by height */
    double font_w = avail_w / (text_len * 0.6); /* 0.6 = avg char width ratio */
    double font_h = avail_h * 0.6;              /* 60% of available height */
    double font_size = font_w < font_h ? font_w : font_h;

    /* Cap at radius (don't exceed inscribed circle) */
    if (font_size > radius * 0.8) font_size = radius * 0.8;
    /* Minimum readable */
    if (font_size < 0.5) font_size = 0.5;

    if (out_x) *out_x = pole_x;
    if (out_y) *out_y = pole_y;
    return font_size;
}

/*
 * ts_text_multi_fit — Plasare mai multe texte (nr_cad, owner, area)
 * Distribuie vertical: nr_cad sus, area mijloc, owner jos
 */
int ts_text_multi_fit(const double *ring_x, const double *ring_y, int n,
                       const char **texts, int text_count,
                       double *out_x, double *out_y, double *out_font) {
    double pole_x, pole_y;
    double radius = ts_text_pole(ring_x, ring_y, n, 0.1, &pole_x, &pole_y);
    if (radius <= 0) return 0;

    double avail_h = ts_text_height_at(ring_x, ring_y, n, pole_x, pole_y);
    double line_h = avail_h / (text_count + 1);  /* distribute vertically */

    for (int i = 0; i < text_count; i++) {
        /* Offset each text vertically from pole */
        double offset_y = (i - text_count / 2.0 + 0.5) * line_h;
        double ty = pole_y + offset_y;
        double tx = pole_x;

        /* Adjust X for available width at this Y */
        double w = ts_text_width_at(ring_x, ring_y, n, tx, ty);
        if (w <= 0) { tx = pole_x; ty = pole_y; w = ts_text_width_at(ring_x, ring_y, n, tx, ty); }

        int tlen = texts[i] ? (int)strlen(texts[i]) : 1;
        double fs = w / (tlen * 0.6);
        if (fs > line_h * 0.8) fs = line_h * 0.8;
        if (fs < 0.3) fs = 0.3;

        out_x[i] = tx;
        out_y[i] = ty;
        out_font[i] = fs;
    }
    return text_count;
}

/* ═══════════════════════════════════════════════════════════════ */
/*  FILE SCANNER — scanare fișiere fără program dedicat           */
/* ═══════════════════════════════════════════════════════════════ */

typedef struct {
    char path[512];
    char type[16];       /* CGXML, TCAD, DXF, CSV, MDB, etc */
    int record_count;    /* nr parcele/entități */
    double total_area;   /* suprafață totală (dacă e cadastral) */
    char summary[256];   /* rezumat text */
} TS_FileScanResult;

/* ═══════════════════════════════════════════════════════════════ */
/*  ADVANCED TEXT PLACEMENT — multi-strategie pentru forme ciudate */
/*                                                                 */
/*  Problema: Pole of Inaccessibility găsește punctul CEL MAI      */
/*  DEPARTE de margine. DAR nu garantează că TEXTUL ÎNCAPE acolo.  */
/*  Pe un poligon îngust la pole, textul overflow-uiește.          */
/*                                                                 */
/*  Soluția: 3 strategii combinate, alege cea mai bună:            */
/*                                                                 */
/*  1. POLE — punct maxim departe de margine (bun pt forme simple) */
/*  2. SCANLINE — rasterizează interiorul, găsește cel mai lat     */
/*     dreptunghi orizontal înscris (bun pt L-shape, benzi)        */  
/*  3. SKELETON — centroidul pe axa mediană a poligonului          */
/*     (bun pt forme alungite unde pole e pe margine)              */
/*                                                                 */
/*  ts_text_best_position() le evaluează pe TOATE și returnează    */
/*  cea cu cel mai mare spațiu disponibil pentru text.              */
/* ═══════════════════════════════════════════════════════════════ */

/* Internal: check if point is inside polygon (ray casting) */
static int _text_point_in_poly(const double *xs, const double *ys, int n,
                                 double px, double py) {
    int inside = 0;
    for (int i = 0, j = n-1; i < n; j = i++) {
        if (((ys[i] > py) != (ys[j] > py)) &&
            (px < (xs[j]-xs[i]) * (py-ys[i]) / (ys[j]-ys[i]) + xs[i]))
            inside = !inside;
    }
    return inside;
}

/* Internal: horizontal width inside polygon at point (px, py) */
static double _text_hwidth_at(const double *xs, const double *ys, int n,
                                double px, double py) {
    /* Find intersections of horizontal ray with polygon edges */
    double left = 1e30, right = -1e30;
    for (int i = 0, j = n-1; i < n; j = i++) {
        if ((ys[i] <= py && ys[j] > py) || (ys[j] <= py && ys[i] > py)) {
            double x_int = xs[i] + (py - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
            if (x_int < px && x_int > (px - 1e30)) {
                if (x_int > left || left > 1e20) left = x_int;
            }
            if (x_int > px && x_int < (px + 1e30)) {
                if (x_int < right || right < -1e20) right = x_int;
            }
        }
    }
    /* Recalculate: find ALL intersections, pair them */
    double intersections[64];
    int ni = 0;
    for (int i = 0, j = n-1; i < n && ni < 64; j = i++) {
        if ((ys[i] <= py && ys[j] > py) || (ys[j] <= py && ys[i] > py)) {
            intersections[ni++] = xs[i] + (py - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
        }
    }
    /* Sort intersections */
    for (int i = 0; i < ni-1; i++)
        for (int j = i+1; j < ni; j++)
            if (intersections[j] < intersections[i]) {
                double tmp = intersections[i]; intersections[i] = intersections[j]; intersections[j] = tmp;
            }
    /* Find the segment containing px */
    for (int i = 0; i < ni-1; i += 2) {
        if (px >= intersections[i] && px <= intersections[i+1])
            return intersections[i+1] - intersections[i];
    }
    return 0;
}

/* ── STRATEGY 2: SCANLINE — largest inscribed horizontal band ── */
/* Rasterizează vertical, la fiecare rând măsoară lățimea         */
/* Găsește banda (y_start, y_end) cu lățimea minimă cea mai mare  */

typedef struct {
    double cx, cy;        /* center of best rectangle */
    double width, height; /* dimensions */
    double score;         /* width * height = area of inscribed rect */
} TS_TextRect;

static TS_TextRect _text_scanline_rect(const double *xs, const double *ys, int n) {
    TS_TextRect best = {0,0,0,0,0};

    /* Bounding box */
    double x0=xs[0], y0=ys[0], x1=xs[0], y1=ys[0];
    for (int i=1; i<n; i++) {
        if (xs[i]<x0) x0=xs[i]; if (ys[i]<y0) y0=ys[i];
        if (xs[i]>x1) x1=xs[i]; if (ys[i]>y1) y1=ys[i];
    }

    /* Scan vertical: 40 rows */
    int rows = 40;
    double dy = (y1 - y0) / rows;
    double row_widths[40];
    double row_centers[40];

    for (int r = 0; r < rows; r++) {
        double py = y0 + dy * (r + 0.5);
        /* Find intersections */
        double intersections[64];
        int ni = 0;
        for (int i = 0, j = n-1; i < n && ni < 64; j = i++) {
            if ((ys[i] <= py && ys[j] > py) || (ys[j] <= py && ys[i] > py)) {
                intersections[ni++] = xs[i] + (py - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
            }
        }
        for (int i=0; i<ni-1; i++)
            for (int j=i+1; j<ni; j++)
                if (intersections[j]<intersections[i]) { double t=intersections[i]; intersections[i]=intersections[j]; intersections[j]=t; }

        /* Widest interior segment */
        double max_w = 0; double max_cx = 0;
        for (int i = 0; i < ni-1; i += 2) {
            double w = intersections[i+1] - intersections[i];
            if (w > max_w) { max_w = w; max_cx = (intersections[i] + intersections[i+1]) / 2; }
        }
        row_widths[r] = max_w;
        row_centers[r] = max_cx;
    }

    /* Find tallest band where minimum width is maximized */
    /* Sliding window: for each start row, extend down while width stays above threshold */
    for (int start = 0; start < rows; start++) {
        double min_w = row_widths[start];
        double sum_cx = row_centers[start];
        int cnt = 1;

        for (int end = start + 1; end < rows; end++) {
            if (row_widths[end] < min_w) min_w = row_widths[end];
            if (min_w < dy) break; /* too narrow */
            sum_cx += row_centers[end];
            cnt++;

            double band_h = dy * cnt;
            double score = min_w * band_h;
            if (score > best.score) {
                best.score = score;
                best.width = min_w;
                best.height = band_h;
                best.cx = sum_cx / cnt;
                best.cy = y0 + dy * (start + cnt * 0.5);
            }
        }
        /* Single row */
        if (row_widths[start] * dy > best.score) {
            best.score = row_widths[start] * dy;
            best.width = row_widths[start];
            best.height = dy;
            best.cx = row_centers[start];
            best.cy = y0 + dy * (start + 0.5);
        }
    }

    return best;
}

/* ═══════════════════════════════════════════════════════════════ */
/*  ts_text_best_position — evaluează TOATE strategiile           */
/*  Returnează cel mai bun (x, y, available_width, available_height)*/
/* ═══════════════════════════════════════════════════════════════ */

double ts_text_best_position(const double *xs, const double *ys, int n,
                               const char *text,
                               double *out_x, double *out_y,
                               double *out_width, double *out_height) {
    if (!xs || !ys || n < 3 || !out_x || !out_y) return 0;

    double best_score = 0;
    double bx = 0, by = 0, bw = 0, bh = 0;

    /* ── Strategy 1: Pole of Inaccessibility ── */
    double pole_x, pole_y;
    double pole_r = ts_text_pole(xs, ys, n, 0.5, &pole_x, &pole_y);
    double pole_w = _text_hwidth_at(xs, ys, n, pole_x, pole_y);
    double pole_score = pole_r * pole_w;  /* radius × width = quality */
    if (pole_score > best_score && _text_point_in_poly(xs, ys, n, pole_x, pole_y)) {
        best_score = pole_score;
        bx = pole_x; by = pole_y; bw = pole_w; bh = pole_r * 2;
    }

    /* ── Strategy 2: Scanline inscribed rectangle ── */
    TS_TextRect rect = _text_scanline_rect(xs, ys, n);
    if (rect.score > best_score && _text_point_in_poly(xs, ys, n, rect.cx, rect.cy)) {
        best_score = rect.score;
        bx = rect.cx; by = rect.cy; bw = rect.width; bh = rect.height;
    }

    /* ── Strategy 3: Centroid (only if inside) ── */
    double cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cx += xs[i]; cy += ys[i]; }
    cx /= n; cy /= n;
    if (_text_point_in_poly(xs, ys, n, cx, cy)) {
        double cw = _text_hwidth_at(xs, ys, n, cx, cy);
        /* Vertical height at centroid */
        double ch = 0;
        for (double dy = 1; ; dy += 1) {
            if (!_text_point_in_poly(xs, ys, n, cx, cy + dy) ||
                !_text_point_in_poly(xs, ys, n, cx, cy - dy)) {
                ch = dy * 2; break;
            }
            if (dy > 10000) { ch = dy * 2; break; }
        }
        double cscore = cw * ch;
        if (cscore > best_score) {
            best_score = cscore;
            bx = cx; by = cy; bw = cw; bh = ch;
        }
    }

    /* ── Strategy 4: Sample grid — brute force pe candidați multipli ── */
    /* 5×5 grid în interior, evaluează fiecare punct */
    double bbx0=xs[0], bby0=ys[0], bbx1=xs[0], bby1=ys[0];
    for (int i=1; i<n; i++) {
        if (xs[i]<bbx0) bbx0=xs[i]; if (ys[i]<bby0) bby0=ys[i];
        if (xs[i]>bbx1) bbx1=xs[i]; if (ys[i]>bby1) bby1=ys[i];
    }
    for (int gx = 1; gx <= 5; gx++) {
        for (int gy = 1; gy <= 5; gy++) {
            double px = bbx0 + (bbx1 - bbx0) * gx / 6.0;
            double py = bby0 + (bby1 - bby0) * gy / 6.0;
            if (!_text_point_in_poly(xs, ys, n, px, py)) continue;
            double gw = _text_hwidth_at(xs, ys, n, px, py);
            /* Quick vertical check */
            double gh = 0;
            for (double dy = 1; ; dy += 1) {
                if (!_text_point_in_poly(xs, ys, n, px, py+dy) ||
                    !_text_point_in_poly(xs, ys, n, px, py-dy)) {
                    gh = dy * 2; break;
                }
                if (dy > 10000) break;
            }
            double gscore = gw * gh;
            if (gscore > best_score) {
                best_score = gscore;
                bx = px; by = py; bw = gw; bh = gh;
            }
        }
    }

    *out_x = bx; *out_y = by;
    if (out_width) *out_width = bw;
    if (out_height) *out_height = bh;
    return best_score;
}

/* ═══════════════════════════════════════════════════════════════ */
/*  ts_text_auto — un singur apel, text plasat perfect            */
/*  Combină best_position + font size calculation                 */
/* ═══════════════════════════════════════════════════════════════ */

double ts_text_auto(const double *xs, const double *ys, int n,
                      const char *text,
                      double *out_x, double *out_y, double *out_font) {
    if (!text || !text[0]) return 0;
    double bx, by, bw, bh;
    double score = ts_text_best_position(xs, ys, n, text, &bx, &by, &bw, &bh);
    if (score <= 0) return 0;

    /* Calculate font size that fits in available width×height */
    int text_len = (int)strlen(text);
    /* Assume avg char width ≈ 0.6 × font_size */
    double font_from_width = bw / (text_len * 0.6);
    /* Height constraint */
    double font_from_height = bh * 0.8; /* 80% of available height */
    /* Use smaller of the two */
    double font = font_from_width < font_from_height ? font_from_width : font_from_height;
    /* Clamp */
    if (font < 1) font = 1;
    if (font > 200) font = 200;

    if (out_x) *out_x = bx;
    if (out_y) *out_y = by;
    if (out_font) *out_font = font;
    return score;
}




/* ═══════════════════════════════════════════════════════════════ */
/*  TRIANGULATION TEXT PLACEMENT — gândim ca topograful            */
/*                                                                 */
/*  Orice poligon în topografie = rețea de triunghiuri.            */
/*  Ear Clipping: descompune poligonul în triunghiuri INTERIOARE.  */
/*  Cercul ÎNSCRIS în cel mai mare triunghi = poziția textului.    */
/*                                                                 */
/*  De ce e mai bun decât scanline:                                */
/*    - Respectă EXACT forma (nu rasterizează)                     */
/*    - Triunghiurile sunt garantat interioare                      */
/*    - Incercul = punct maxim departe de toate 3 laturi            */
/*    - Funcționează pe ORICE poligon simplu (concav, convex)      */
/*    - O(n²) dar pe 4-50 vertecși cadastrali = instant            */
/* ═══════════════════════════════════════════════════════════════ */

/* Cross product sign (2D) */
static double _tri_cross(double ax, double ay, double bx, double by,
                           double cx, double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

/* Is point P inside triangle ABC? */
static int _tri_point_in_triangle(double px, double py,
                                    double ax, double ay,
                                    double bx, double by,
                                    double cx, double cy) {
    double d1 = _tri_cross(px, py, ax, ay, bx, by);
    double d2 = _tri_cross(px, py, bx, by, cx, cy);
    double d3 = _tri_cross(px, py, cx, cy, ax, ay);
    int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

/* Is vertex i an "ear" (no other vertex inside the triangle)? */
static int _tri_is_ear(const double *xs, const double *ys,
                         const int *indices, int count, int i) {
    int prev = (i - 1 + count) % count;
    int next = (i + 1) % count;
    double ax = xs[indices[prev]], ay = ys[indices[prev]];
    double bx = xs[indices[i]],    by = ys[indices[i]];
    double cx = xs[indices[next]], cy = ys[indices[next]];

    /* Must be convex (CCW cross > 0) */
    if (_tri_cross(ax, ay, bx, by, cx, cy) <= 0) return 0;

    /* No other vertex inside this triangle */
    for (int j = 0; j < count; j++) {
        if (j == prev || j == i || j == next) continue;
        if (_tri_point_in_triangle(xs[indices[j]], ys[indices[j]],
                                     ax, ay, bx, by, cx, cy))
            return 0;
    }
    return 1;
}

/* Triangle area */
static double _tri_area(double ax, double ay, double bx, double by,
                          double cx, double cy) {
    double a = fabs(_tri_cross(ax, ay, bx, by, cx, cy)) / 2.0;
    return a;
}

/* Edge length */
static double _tri_edge(double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    return sqrt(dx*dx + dy*dy);
}

/* Incircle of triangle: center (ix, iy) and radius r */
/* Incircul = punctul echidistant de toate cele 3 laturi */
static double _tri_incircle(double ax, double ay, double bx, double by,
                              double cx, double cy,
                              double *ix, double *iy) {
    double a = _tri_edge(bx, by, cx, cy); /* side opposite A */
    double b = _tri_edge(ax, ay, cx, cy); /* side opposite B */
    double c = _tri_edge(ax, ay, bx, by); /* side opposite C */
    double p = a + b + c;
    if (p < 1e-12) return 0;

    /* Incenter = weighted average by opposite side lengths */
    *ix = (a * ax + b * bx + c * cx) / p;
    *iy = (a * ay + b * by + c * cy) / p;

    /* Inradius = area / semi-perimeter */
    double area = _tri_area(ax, ay, bx, by, cx, cy);
    return area / (p / 2.0);
}

/* ═══════════════════════════════════════════════════════════════ */
/*  EAR CLIPPING TRIANGULATION                                    */
/*  Descompune poligon → triunghiuri, returnează cel mai bun      */
/* ═══════════════════════════════════════════════════════════════ */

typedef struct {
    double ax, ay, bx, by, cx, cy;  /* vertices */
    double ix, iy;                   /* incircle center */
    double inradius;                 /* incircle radius */
    double area;
} TS_TextTri;

/* Triangulate polygon and find best triangle for text placement */
int ts_triangulate_polygon(const double *xs, const double *ys, int n,
                             TS_TextTri *triangles, int max_tri) {
    if (n < 3 || !triangles) return 0;
    
    /* Strip closing vertex if first == last */
    if (n > 3 && fabs(xs[0]-xs[n-1]) < 1e-10 && fabs(ys[0]-ys[n-1]) < 1e-10) n--;

    /* Ensure CCW winding */
    double winding = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        winding += (xs[j] - xs[i]) * (ys[j] + ys[i]);
    }
    int ccw = (winding < 0) ? 1 : 0;

    /* Working copy of indices */
    int idx_buf[512];
    int *indices = (n <= 512) ? idx_buf : (int*)malloc(sizeof(int) * (size_t)n);
    int count = n;
    if (ccw) {
        for (int i = 0; i < n; i++) indices[i] = i;
    } else {
        for (int i = 0; i < n; i++) indices[i] = n - 1 - i; /* reverse */
    }

    int tri_count = 0;
    int iterations = 0;
    int max_iter = n * n; /* safety */

    while (count > 2 && tri_count < max_tri && iterations < max_iter) {
        int ear_found = 0;
        for (int i = 0; i < count; i++) {
            if (_tri_is_ear(xs, ys, indices, count, i)) {
                int prev = (i - 1 + count) % count;
                int next = (i + 1) % count;

                TS_TextTri *t = &triangles[tri_count];
                t->ax = xs[indices[prev]]; t->ay = ys[indices[prev]];
                t->bx = xs[indices[i]];    t->by = ys[indices[i]];
                t->cx = xs[indices[next]]; t->cy = ys[indices[next]];
                t->area = _tri_area(t->ax, t->ay, t->bx, t->by, t->cx, t->cy);
                t->inradius = _tri_incircle(t->ax, t->ay, t->bx, t->by,
                                              t->cx, t->cy, &t->ix, &t->iy);
                tri_count++;

                /* Remove ear vertex */
                for (int j = i; j < count - 1; j++) indices[j] = indices[j+1];
                count--;
                ear_found = 1;
                break;
            }
        }
        if (!ear_found) break; /* degenerate polygon */
        iterations++;
    }

    if (n > 512) free(indices);
    return tri_count;
}

/* ═══════════════════════════════════════════════════════════════ */
/*  TEXT PLACEMENT VIA TRIANGULATION — strategia topografului      */
/*                                                                 */
/*  1. Triangulează poligonul (ear clipping)                       */
/*  2. Pentru fiecare triunghi: calculează incercul                 */
/*  3. Triunghiul cu cel mai mare incerc = cel mai mult spațiu     */
/*  4. Textul se plasează la centrul incercului                    */
/*  5. Font size = proporțional cu inraza                          */
/* ═══════════════════════════════════════════════════════════════ */

double ts_text_triangulated(const double *xs, const double *ys, int n,
                              const char *text,
                              double *out_x, double *out_y,
                              double *out_font) {
    if (n < 3 || !out_x || !out_y) return 0;

    TS_TextTri triangles[512]; /* max 512 triangles (510 vertices) */
    int nt = ts_triangulate_polygon(xs, ys, n, triangles, 512);
    if (nt == 0) return 0;

    /* Find triangle with largest incircle */
    int best = 0;
    for (int i = 1; i < nt; i++) {
        if (triangles[i].inradius > triangles[best].inradius)
            best = i;
    }

    *out_x = triangles[best].ix;
    *out_y = triangles[best].iy;

    if (out_font && text && text[0]) {
        int tlen = (int)strlen(text);
        /* Width at incircle center — horizontal span inside polygon */
        double avail_w = _text_hwidth_at(xs, ys, n,
                                           triangles[best].ix, triangles[best].iy);
        double font_w = avail_w / (tlen * 0.6);
        double font_h = triangles[best].inradius * 1.6;
        double font = font_w < font_h ? font_w : font_h;
        if (font < 1) font = 1;
        if (font > 200) font = 200;
        *out_font = font;
    }

    return triangles[best].inradius;
}

/* ═══════════════════════════════════════════════════════════════ */
/*  TEXT IN HATCH — abordare geodezică                             */
/*                                                                 */
/*  Ideea boss-ului: textul se încadrează într-un hatch provizoriu */
/*  Hatch-ul = poligonul parcelei cu granițe fixe                  */
/*  Triangulația transformă orice formă în insule deterministe     */
/*  Fiecare insulă (triunghi) are un dreptunghi inscris maxim     */
/*  Triunghiuri adiacente se unesc în fîșii (strips) pentru text  */
/*  mai lung.                                                      */
/*                                                                 */
/*  Etape:                                                         */
/*  1. Triangulare ear-clipping → n-2 triunghiuri                 */
/*  2. Pentru fiecare triunghi: dreptunghi inscris maxim           */
/*     (baza pe cea mai lungă latură, înălțime = inălțimea tri)   */
/*  3. Triunghiuri cu aceeași latură se unesc → strip              */
/*  4. Dreptunghiul strip-ului = suma bazelor × min înălțime       */
/*  5. Textul se plasează în strip-ul cu cel mai bun aspect ratio  */
/* ═══════════════════════════════════════════════════════════════ */

/* Largest inscribed axis-aligned rectangle in a triangle */
static void _tri_inscribed_rect(double ax, double ay, double bx, double by,
                                  double cx, double cy,
                                  double *rx, double *ry, double *rw, double *rh) {
    /* Find the longest edge → use as base */
    double edges[3][4] = {
        {ax,ay,bx,by}, {bx,by,cx,cy}, {cx,cy,ax,ay}
    };
    double opp[3][2] = {{cx,cy},{ax,ay},{bx,by}}; /* opposite vertex */
    double max_len = 0; int best_e = 0;
    for (int i=0; i<3; i++) {
        double dx = edges[i][2]-edges[i][0], dy = edges[i][3]-edges[i][1];
        double len = sqrt(dx*dx+dy*dy);
        if (len > max_len) { max_len = len; best_e = i; }
    }
    
    double ex = edges[best_e][0], ey = edges[best_e][1];
    double fx = edges[best_e][2], fy = edges[best_e][3];
    double ox = opp[best_e][0], oy = opp[best_e][1];
    
    /* Project opposite vertex onto base edge → height */
    double dx = fx-ex, dy = fy-ey;
    double len = sqrt(dx*dx+dy*dy);
    if (len < 1e-10) { *rx=ex;*ry=ey;*rw=0;*rh=0; return; }
    double nx = -dy/len, ny = dx/len; /* normal to base */
    double h = fabs((ox-ex)*nx + (oy-ey)*ny); /* perpendicular height */
    
    /* Inscribed rectangle: height = h/2 (half triangle height) */
    /* Width at h/2 = base_len * (1 - h_rect/h) = base_len * 0.5 */
    /* Maximum area rectangle: h_rect = h/2, w_rect = base/2 */
    double rect_h = h * 0.5;
    double rect_w = max_len * 0.5;
    
    /* Center of rectangle: centroid shifted toward base */
    double centx = (ax+bx+cx)/3.0;
    double centy = (ay+by+cy)/3.0;
    /* Shift toward base midpoint */
    double midx = (ex+fx)/2.0, midy = (ey+fy)/2.0;
    *rx = centx * 0.3 + midx * 0.7; /* bias toward base */
    *ry = centy * 0.3 + midy * 0.7;
    *rw = rect_w;
    *rh = rect_h;
}

/* Text placement via hatch triangulation + inscribed rectangles */
double ts_text_hatch(const double *xs, const double *ys, int n,
                       const char *text, double desired_aspect,
                       double *out_x, double *out_y,
                       double *out_w, double *out_h, double *out_font) {
    if (n < 3 || !out_x || !out_y) return 0;
    
    TS_TextTri triangles[512];
    int nt = ts_triangulate_polygon(xs, ys, n, triangles, 512);
    if (nt == 0) return 0;
    
    /* For each triangle, compute inscribed rectangle */
    double best_score = 0;
    double best_x=0,best_y=0,best_w=0,best_h=0;
    
    int tlen = text ? (int)strlen(text) : 4;
    double text_aspect = tlen * 0.6; /* width/height ratio for text */
    if (desired_aspect > 0) text_aspect = desired_aspect;
    
    for (int i = 0; i < nt; i++) {
        double rx,ry,rw,rh;
        _tri_inscribed_rect(triangles[i].ax, triangles[i].ay,
                             triangles[i].bx, triangles[i].by,
                             triangles[i].cx, triangles[i].cy,
                             &rx, &ry, &rw, &rh);
        
        /* Score: how well does the rectangle fit the text? */
        /* Penalty for aspect ratio mismatch */
        double rect_aspect = (rh > 0.1) ? rw / rh : 0;
        double aspect_match = 1.0 / (1.0 + fabs(rect_aspect - text_aspect));
        double area_score = rw * rh;
        double score = area_score * aspect_match;
        
        if (score > best_score) {
            best_score = score;
            best_x = rx; best_y = ry;
            best_w = rw; best_h = rh;
        }
    }
    
    /* Try merging adjacent triangles for wider strips */
    for (int i = 0; i < nt; i++) {
        for (int j = i+1; j < nt; j++) {
            /* Check if triangles share an edge (2 common vertices) */
            double verts_i[6] = {triangles[i].ax,triangles[i].ay,
                                  triangles[i].bx,triangles[i].by,
                                  triangles[i].cx,triangles[i].cy};
            double verts_j[6] = {triangles[j].ax,triangles[j].ay,
                                  triangles[j].bx,triangles[j].by,
                                  triangles[j].cx,triangles[j].cy};
            int shared = 0;
            for (int a=0;a<3;a++) for (int b=0;b<3;b++) {
                if (fabs(verts_i[a*2]-verts_j[b*2])<0.01 &&
                    fabs(verts_i[a*2+1]-verts_j[b*2+1])<0.01) shared++;
            }
            if (shared < 2) continue; /* not adjacent */
            
            /* Merge: combined bounding box */
            double minx=1e30,miny=1e30,maxx=-1e30,maxy=-1e30;
            for (int k=0;k<6;k+=2) {
                if(verts_i[k]<minx)minx=verts_i[k]; if(verts_i[k]>maxx)maxx=verts_i[k];
                if(verts_i[k+1]<miny)miny=verts_i[k+1]; if(verts_i[k+1]>maxy)maxy=verts_i[k+1];
            }
            for (int k=0;k<6;k+=2) {
                if(verts_j[k]<minx)minx=verts_j[k]; if(verts_j[k]>maxx)maxx=verts_j[k];
                if(verts_j[k+1]<miny)miny=verts_j[k+1]; if(verts_j[k+1]>maxy)maxy=verts_j[k+1];
            }
            double mw = (maxx-minx)*0.7; /* inscribed ≈ 70% of bbox */
            double mh = (maxy-miny)*0.7;
            double mx = (minx+maxx)/2, my = (miny+maxy)/2;
            
            double rect_aspect = (mh > 0.1) ? mw / mh : 0;
            double aspect_match = 1.0 / (1.0 + fabs(rect_aspect - text_aspect));
            double score = mw * mh * aspect_match * 1.2; /* bonus for strip */
            
            /* Verify center is inside polygon */
            int inside = 0;
            for (int a=0,b=n-1; a<n; b=a++) {
                if (((ys[a]>my) != (ys[b]>my)) &&
                    (mx < (xs[b]-xs[a]) * (my-ys[a]) / (ys[b]-ys[a]) + xs[a]))
                    inside = !inside;
            }
            
            if (inside && score > best_score) {
                best_score = score;
                best_x = mx; best_y = my;
                best_w = mw; best_h = mh;
            }
        }
    }
    
    *out_x = best_x; *out_y = best_y;
    if (out_w) *out_w = best_w;
    if (out_h) *out_h = best_h;
    if (out_font) {
        double fw = best_w / (tlen * 0.6);
        double fh = best_h * 0.8;
        *out_font = (fw < fh ? fw : fh);
        if (*out_font < 1) *out_font = 1;
        if (*out_font > 200) *out_font = 200;
    }
    return best_score;
}



/* ═══════════════════════════════════════════════════════════════ */
/*  ULTIMATE: ts_text_ultimate — 5 strategii, cel mai bun câștigă */
/*  Pole + Scanline + Centroid + Grid + TRIANGULAȚIE               */
/* ═══════════════════════════════════════════════════════════════ */

double ts_text_ultimate(const double *xs, const double *ys, int n,
                          const char *text,
                          double *out_x, double *out_y, double *out_font) {
    if (n < 3 || !out_x || !out_y) return 0;

    double best_score = 0;
    double bx = 0, by = 0, bf = 10;

    /* Strategy 1-4: existing best_position */
    double s1x, s1y, s1w, s1h;
    double score1 = ts_text_best_position(xs, ys, n, text, &s1x, &s1y, &s1w, &s1h);
    if (score1 > best_score) {
        best_score = score1; bx = s1x; by = s1y;
        int tlen = text ? (int)strlen(text) : 4;
        double fw = s1w / (tlen * 0.6);
        double fh = s1h * 0.8;
        bf = fw < fh ? fw : fh;
    }

    /* Strategy 5: Triangulation */
    double tx, ty, tf;
    double tri_r = ts_text_triangulated(xs, ys, n, text, &tx, &ty, &tf);
    /* Score: inradius × available width at that point */
    double tri_w = _text_hwidth_at(xs, ys, n, tx, ty);
    double tri_score = tri_r * tri_w;
    if (tri_score > best_score && _text_point_in_poly(xs, ys, n, tx, ty)) {
        best_score = tri_score; bx = tx; by = ty; bf = tf;
    }

    *out_x = bx; *out_y = by;
    if (out_font) *out_font = (bf < 1) ? 1 : (bf > 200 ? 200 : bf);
    return best_score;
}


#endif /* TS_TEXT_PLACEMENT_H */
