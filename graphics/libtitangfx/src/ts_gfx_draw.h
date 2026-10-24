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
 * ts_gfx_draw.h — 2D Drawing Primitives
 * =======================================
 * Single-header C99. Zero dependencies (uses ts_gfx_canvas.h).
 *
 * Algorithms:
 *   Line:         Bresenham with Wu anti-aliasing
 *   Thick line:   Perpendicular Bresenham fill
 *   Circle:       Midpoint circle algorithm
 *   Polygon fill: Scanline with active edge table
 *   Arc:          Parametric sampling
 *
 * #define TS_GFX_DRAW_IMPLEMENTATION in ONE .c file before #include.
 *
 * SoftCAD C+C TOPO © 2026 — Titan Sentinel Script GFX Engine
 */
#ifndef TS_GFX_DRAW_H
#define TS_GFX_DRAW_H

#include "ts_gfx_canvas.h"
#include <math.h>

/* ── API ───────────────────────────────────────────────────────────────── */

/* Lines */
void ts_draw_line(TS_Canvas *c, int x0, int y0, int x1, int y1);
void ts_draw_line_thick(TS_Canvas *c, int x0, int y0, int x1, int y1, float width);
void ts_draw_line_aa(TS_Canvas *c, int x0, int y0, int x1, int y1); /* Wu AA */

/* Rectangles */
void ts_draw_rect(TS_Canvas *c, int x, int y, int w, int h);
void ts_draw_rect_filled(TS_Canvas *c, int x, int y, int w, int h);

/* Circles */
void ts_draw_circle(TS_Canvas *c, int cx, int cy, int r);
void ts_draw_circle_filled(TS_Canvas *c, int cx, int cy, int r);

/* Ellipses */
void ts_draw_ellipse(TS_Canvas *c, int cx, int cy, int rx, int ry);

/* Arcs */
void ts_draw_arc(TS_Canvas *c, int cx, int cy, int r,
                 float start_deg, float end_deg);

/* Polygons (array of x,y pairs: [x0,y0, x1,y1, ...]) */
void ts_draw_polygon(TS_Canvas *c, const float *pts, int npts);
void ts_draw_polygon_filled(TS_Canvas *c, const float *pts, int npts);

/* Polygons from ring format [(x,y), ...] — convenience for TCAD */
void ts_draw_ring(TS_Canvas *c, const float *xs, const float *ys, int n);
void ts_draw_ring_filled(TS_Canvas *c, const float *xs, const float *ys, int n);

/* Polyline (open, not closed) */
void ts_draw_polyline(TS_Canvas *c, const float *pts, int npts);

/* Tinted alpha-mask glyph blit from a single-channel atlas. */
void ts_draw_glyph_tinted(TS_Canvas *c, const uint8_t *atlas_pixels,
                          int atlas_w, int atlas_h,
                          int sx, int sy, int sw, int sh,
                          int dx, int dy, uint32_t color);

/* ── Implementation ────────────────────────────────────────────────────── */
#ifdef TS_GFX_DRAW_IMPLEMENTATION

/* ── Bresenham line ────────────────────────────────────────────────────── */

void ts_draw_line(TS_Canvas *c, int x0, int y0, int x1, int y1) {
    uint32_t col = c->stroke_color;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        ts_pixel_blend(c, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ── Wu anti-aliased line ──────────────────────────────────────────────── */

static inline float _fpart(float x) { return x - floorf(x); }
static inline float _rfpart(float x) { return 1.0f - _fpart(x); }

static void _plot_aa(TS_Canvas *c, int x, int y, float brightness, uint32_t col) {
    int a = (int)(ts_a(col) * brightness * c->opacity);
    if (a <= 0) return;
    if (a > 255) a = 255;
    uint32_t blended = ts_rgba(ts_r(col), ts_g(col), ts_b(col), a);
    ts_pixel_blend(c, x, y, blended);
}

void ts_draw_line_aa(TS_Canvas *c, int x0, int y0, int x1, int y1) {
    uint32_t col = c->stroke_color;
    int steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) { int t; t=x0; x0=y0; y0=t; t=x1; x1=y1; y1=t; }
    if (x0 > x1) { int t; t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }

    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    /* First endpoint */
    float xend = (float)x0;
    float yend = (float)y0;
    float intery = yend + gradient;

    if (steep) _plot_aa(c, (int)yend, (int)xend, 1.0f, col);
    else       _plot_aa(c, (int)xend, (int)yend, 1.0f, col);

    /* Second endpoint */
    if (steep) _plot_aa(c, y1, x1, 1.0f, col);
    else       _plot_aa(c, x1, y1, 1.0f, col);

    /* Main loop */
    for (int x = x0 + 1; x < x1; x++) {
        if (steep) {
            _plot_aa(c, (int)intery,     x, _rfpart(intery), col);
            _plot_aa(c, (int)intery + 1, x, _fpart(intery),  col);
        } else {
            _plot_aa(c, x, (int)intery,     _rfpart(intery), col);
            _plot_aa(c, x, (int)intery + 1, _fpart(intery),  col);
        }
        intery += gradient;
    }
}

/* ── Thick line ────────────────────────────────────────────────────────── */

void ts_draw_line_thick(TS_Canvas *c, int x0, int y0, int x1, int y1, float width) {
    if (width <= 1.5f) { ts_draw_line_aa(c, x0, y0, x1, y1); return; }

    float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    /* Perpendicular offset */
    float px = -dy / len * width * 0.5f;
    float py =  dx / len * width * 0.5f;

    /* Draw as filled quad */
    float pts[8] = {
        x0 + px, y0 + py,
        x0 - px, y0 - py,
        x1 - px, y1 - py,
        x1 + px, y1 + py,
    };
    ts_draw_polygon_filled(c, pts, 4);

    /* Outline for AA */
    ts_draw_line_aa(c, (int)(x0 + px), (int)(y0 + py), (int)(x1 + px), (int)(y1 + py));
    ts_draw_line_aa(c, (int)(x0 - px), (int)(y0 - py), (int)(x1 - px), (int)(y1 - py));
}

/* ── Rectangle ─────────────────────────────────────────────────────────── */

void ts_draw_rect(TS_Canvas *c, int x, int y, int w, int h) {
    ts_draw_line(c, x, y, x + w - 1, y);
    ts_draw_line(c, x + w - 1, y, x + w - 1, y + h - 1);
    ts_draw_line(c, x + w - 1, y + h - 1, x, y + h - 1);
    ts_draw_line(c, x, y + h - 1, x, y);
}

void ts_draw_rect_filled(TS_Canvas *c, int x, int y, int w, int h) {
    uint32_t col = c->fill_color;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > c->width)  ? c->width  : x + w;
    int y1 = (y + h > c->height) ? c->height : y + h;
    for (int iy = y0; iy < y1; iy++)
        for (int ix = x0; ix < x1; ix++)
            ts_pixel_blend(c, ix, iy, col);
}

/* ── Circle (Midpoint) ─────────────────────────────────────────────────── */

void ts_draw_circle(TS_Canvas *c, int cx, int cy, int r) {
    uint32_t col = c->stroke_color;
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        ts_pixel_blend(c, cx+x, cy+y, col); ts_pixel_blend(c, cx-x, cy+y, col);
        ts_pixel_blend(c, cx+x, cy-y, col); ts_pixel_blend(c, cx-x, cy-y, col);
        ts_pixel_blend(c, cx+y, cy+x, col); ts_pixel_blend(c, cx-y, cy+x, col);
        ts_pixel_blend(c, cx+y, cy-x, col); ts_pixel_blend(c, cx-y, cy-x, col);
        y++;
        if (err < 0) { err += 2 * y + 1; }
        else         { x--; err += 2 * (y - x) + 1; }
    }
}

void ts_draw_circle_filled(TS_Canvas *c, int cx, int cy, int r) {
    uint32_t col = c->fill_color;
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        for (int ix = cx - x; ix <= cx + x; ix++) {
            ts_pixel_blend(c, ix, cy + y, col);
            ts_pixel_blend(c, ix, cy - y, col);
        }
        for (int ix = cx - y; ix <= cx + y; ix++) {
            ts_pixel_blend(c, ix, cy + x, col);
            ts_pixel_blend(c, ix, cy - x, col);
        }
        y++;
        if (err < 0) { err += 2 * y + 1; }
        else         { x--; err += 2 * (y - x) + 1; }
    }
}

/* ── Ellipse ───────────────────────────────────────────────────────────── */

void ts_draw_ellipse(TS_Canvas *c, int cx, int cy, int rx, int ry) {
    int steps = (rx > ry ? rx : ry) * 4;
    if (steps < 32) steps = 32;
    float step = 6.28318530718f / (float)steps;
    float px = cx + rx, py = (float)cy;
    for (int i = 1; i <= steps; i++) {
        float a = step * i;
        float nx = cx + rx * cosf(a);
        float ny = cy + ry * sinf(a);
        ts_draw_line(c, (int)px, (int)py, (int)nx, (int)ny);
        px = nx; py = ny;
    }
}

/* ── Arc ───────────────────────────────────────────────────────────────── */

void ts_draw_arc(TS_Canvas *c, int cx, int cy, int r,
                 float start_deg, float end_deg) {
    float s = start_deg * 0.0174532925f; /* deg to rad */
    float e = end_deg   * 0.0174532925f;
    int steps = (int)(fabsf(e - s) * r * 0.5f);
    if (steps < 16) steps = 16;
    if (steps > 1000) steps = 1000;
    float step = (e - s) / (float)steps;
    float px = cx + r * cosf(s), py = cy + r * sinf(s);
    for (int i = 1; i <= steps; i++) {
        float a = s + step * i;
        float nx = cx + r * cosf(a);
        float ny = cy + r * sinf(a);
        ts_draw_line(c, (int)px, (int)py, (int)nx, (int)ny);
        px = nx; py = ny;
    }
}

/* ── Scanline polygon fill ─────────────────────────────────────────────── */

void ts_draw_polygon_filled(TS_Canvas *c, const float *pts, int npts) {
    if (npts < 3) return;
    uint32_t col = c->fill_color;

    /* Find Y bounds */
    float ymin = pts[1], ymax = pts[1];
    for (int i = 1; i < npts; i++) {
        float y = pts[i * 2 + 1];
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
    }
    int iy_min = (int)ymin;
    int iy_max = (int)ymax;
    if (iy_min < 0) iy_min = 0;
    if (iy_max >= c->height) iy_max = c->height - 1;

    /* Scanline fill with edge intersection */
    float *node_x = (float *)malloc((size_t)(npts + 1) * sizeof(float));
    if (!node_x) return;

    for (int y = iy_min; y <= iy_max; y++) {
        float scanY = (float)y + 0.5f;
        int nodes = 0;

        /* Find all edge intersections */
        int j = npts - 1;
        for (int i = 0; i < npts; i++) {
            float yi = pts[i * 2 + 1], yj = pts[j * 2 + 1];
            if ((yi < scanY && yj >= scanY) || (yj < scanY && yi >= scanY)) {
                float xi = pts[i * 2], xj = pts[j * 2];
                node_x[nodes++] = xi + (scanY - yi) / (yj - yi) * (xj - xi);
            }
            j = i;
        }

        /* Sort intersections */
        for (int i = 0; i < nodes - 1; i++)
            for (int k = i + 1; k < nodes; k++)
                if (node_x[k] < node_x[i]) { float t = node_x[i]; node_x[i] = node_x[k]; node_x[k] = t; }

        /* Fill between pairs */
        for (int i = 0; i < nodes - 1; i += 2) {
            int x0 = (int)node_x[i];
            int x1 = (int)node_x[i + 1];
            if (x0 < 0) x0 = 0;
            if (x1 >= c->width) x1 = c->width - 1;
            for (int x = x0; x <= x1; x++)
                ts_pixel_blend(c, x, y, col);
        }
    }
    free(node_x);
}

/* ── Polygon outline ───────────────────────────────────────────────────── */

void ts_draw_polygon(TS_Canvas *c, const float *pts, int npts) {
    if (npts < 2) return;
    for (int i = 0; i < npts; i++) {
        int j = (i + 1) % npts;
        ts_draw_line_aa(c, (int)pts[i*2], (int)pts[i*2+1],
                            (int)pts[j*2], (int)pts[j*2+1]);
    }
}

/* ── Ring convenience (separate x/y arrays) ────────────────────────────── */

void ts_draw_ring(TS_Canvas *c, const float *xs, const float *ys, int n) {
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        ts_draw_line_aa(c, (int)xs[i], (int)ys[i], (int)xs[j], (int)ys[j]);
    }
}

void ts_draw_ring_filled(TS_Canvas *c, const float *xs, const float *ys, int n) {
    if (n < 3) return;
    /* Convert to interleaved for scanline */
    float *pts = (float *)malloc((size_t)(n * 2) * sizeof(float));
    if (!pts) return;
    for (int i = 0; i < n; i++) { pts[i*2] = xs[i]; pts[i*2+1] = ys[i]; }
    ts_draw_polygon_filled(c, pts, n);
    free(pts);
}

/* ── Polyline (open) ───────────────────────────────────────────────────── */

void ts_draw_polyline(TS_Canvas *c, const float *pts, int npts) {
    for (int i = 0; i < npts - 1; i++)
        ts_draw_line_aa(c, (int)pts[i*2], (int)pts[i*2+1],
                            (int)pts[(i+1)*2], (int)pts[(i+1)*2+1]);
}


void ts_draw_glyph_tinted(TS_Canvas *c, const uint8_t *atlas_pixels,
                          int atlas_w, int atlas_h,
                          int sx, int sy, int sw, int sh,
                          int dx, int dy, uint32_t color) {
    if (!c || !atlas_pixels || atlas_w <= 0 || atlas_h <= 0 || sw <= 0 || sh <= 0) return;
    const int base_alpha = ts_a(color);
    for (int y = 0; y < sh; ++y) {
        const int ay = sy + y;
        const int cy = dy + y;
        if (ay < 0 || ay >= atlas_h || cy < 0 || cy >= c->height) continue;
        for (int x = 0; x < sw; ++x) {
            const int ax = sx + x;
            const int cx = dx + x;
            if (ax < 0 || ax >= atlas_w || cx < 0 || cx >= c->width) continue;
            const int mask = atlas_pixels[ay * atlas_w + ax];
            if (mask == 0) continue;
            int alpha = (base_alpha * mask + 127) / 255;
            alpha = (int)(alpha * c->opacity);
            if (alpha <= 0) continue;
            if (alpha > 255) alpha = 255;
            ts_pixel_blend(c, cx, cy, ts_rgba(ts_r(color), ts_g(color), ts_b(color), alpha));
        }
    }
}

#endif /* TS_GFX_DRAW_IMPLEMENTATION */
#endif /* TS_GFX_DRAW_H */
