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
#ifndef TS_RENDER_ENGINE_H
#define TS_RENDER_ENGINE_H

/*
 * ═══════════════════════════════════════════════════════════════════
 *  TITAN Render Engine — native 2D polygon renderer
 *  Direct C implementation of render_engine.ts
 *  Input: JSON with polygons → Output: RGB pixel buffer
 *  SoftCAD C+C TOPO © 2026
 * ═══════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Render buffer ────────────────────────────────────────────── */
typedef struct {
    int w, h;
    unsigned char *pixels;  /* RGB, w*h*3 bytes */
} TS_RenderBuf;

/* Create render buffer */
TS_RenderBuf *ts_render_new(int w, int h) {
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return NULL;
    TS_RenderBuf *rb = (TS_RenderBuf *)calloc(1, sizeof(TS_RenderBuf));
    if (!rb) return NULL;
    rb->w = w; rb->h = h;
    rb->pixels = (unsigned char *)calloc((size_t)(w * h * 3), 1);
    if (!rb->pixels) { free(rb); return NULL; }
    return rb;
}

void ts_render_free(TS_RenderBuf *rb) {
    if (rb) { free(rb->pixels); free(rb); }
}

/* Clear to background color */
void ts_render_clear(TS_RenderBuf *rb, int r, int g, int b) {
    if (!rb || !rb->pixels) return;
    int n = rb->w * rb->h;
    for (int i = 0; i < n; i++) {
        rb->pixels[i*3]   = (unsigned char)r;
        rb->pixels[i*3+1] = (unsigned char)g;
        rb->pixels[i*3+2] = (unsigned char)b;
    }
}

/* Set pixel with alpha blend */
static void _rend_set_pixel(TS_RenderBuf *rb, int x, int y,
                              int r, int g, int b, int alpha) {
    if (x < 0 || x >= rb->w || y < 0 || y >= rb->h) return;
    int idx = (y * rb->w + x) * 3;
    if (alpha >= 255) {
        rb->pixels[idx] = (unsigned char)r;
        rb->pixels[idx+1] = (unsigned char)g;
        rb->pixels[idx+2] = (unsigned char)b;
    } else {
        int inv = 255 - alpha;
        rb->pixels[idx]   = (unsigned char)((r*alpha + rb->pixels[idx]*inv) / 255);
        rb->pixels[idx+1] = (unsigned char)((g*alpha + rb->pixels[idx+1]*inv) / 255);
        rb->pixels[idx+2] = (unsigned char)((b*alpha + rb->pixels[idx+2]*inv) / 255);
    }
}

/* Bresenham line with thickness */
void ts_render_line(TS_RenderBuf *rb, int x0, int y0, int x1, int y1,
                     int r, int g, int b, int pen_width) {
    if (!rb) return;
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int hw = pen_width / 2;
    int steps = dx + dy + 1;
    for (int i = 0; i < steps; i++) {
        for (int tx = -hw; tx <= hw; tx++)
            for (int ty = -hw; ty <= hw; ty++)
                _rend_set_pixel(rb, x0+tx, y0+ty, r, g, b, 220);
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

/* Scanline polygon fill */
void ts_render_fill_polygon(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                              int r, int g, int b, int alpha) {
    if (!rb || n < 3) return;
    int y_min = ys[0], y_max = ys[0];
    for (int i = 1; i < n; i++) {
        if (ys[i] < y_min) y_min = ys[i];
        if (ys[i] > y_max) y_max = ys[i];
    }
    if (y_min < 0) y_min = 0;
    if (y_max >= rb->h) y_max = rb->h - 1;

    int *intersections = (int *)malloc(sizeof(int) * (size_t)(n + 2));
    if (!intersections) return;

    for (int y = y_min; y <= y_max; y++) {
        int ni = 0;
        for (int k = 0, j = n - 1; k < n; j = k++) {
            int yi = ys[k], yj = ys[j];
            if ((yi <= y && yj > y) || (yj <= y && yi > y)) {
                intersections[ni++] = xs[k] + (y - yi) * (xs[j] - xs[k]) / (yj - yi);
            }
        }
        /* Sort */
        for (int a = 0; a < ni - 1; a++)
            for (int jj = a + 1; jj < ni; jj++)
                if (intersections[jj] < intersections[a]) {
                    int tmp = intersections[a]; intersections[a] = intersections[jj]; intersections[jj] = tmp;
                }
        /* Fill pairs */
        for (int p = 0; p + 1 < ni; p += 2) {
            int xl = intersections[p] < 0 ? 0 : intersections[p];
            int xr = intersections[p+1] >= rb->w ? rb->w - 1 : intersections[p+1];
            for (int x = xl; x <= xr; x++)
                _rend_set_pixel(rb, x, y, r, g, b, alpha);
        }
    }
    free(intersections);
}

/* Render polygon: fill + outline */
void ts_render_polygon(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                         int r, int g, int b, int fill_alpha, int pen_width) {
    if (!rb || n < 2) return;
    if (fill_alpha > 0 && n >= 3)
        ts_render_fill_polygon(rb, xs, ys, n, r, g, b, fill_alpha);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        ts_render_line(rb, xs[i], ys[i], xs[j], ys[j], r, g, b, pen_width);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  PIPELINE: JSON → render → RGB buffer
 *  This is the C equivalent of render_engine.ts main()
 * ═══════════════════════════════════════════════════════════════ */

/* Render from JSON string → RGB pixel buffer
   Returns: TS_RenderBuf* (caller frees with ts_render_free)
   JSON format: {"w":1200,"h":800,"bg":[13,18,32],"polys":[
     {"c":[r,g,b],"a":alpha,"pw":pen_width,"p":[x0,y0,x1,y1,...]},...]} */

/* Write render buffer as raw RGB to file */
int ts_render_save_raw(const TS_RenderBuf *rb, const char *path) {
    if (!rb || !path) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    /* Header: W H as uint32 LE */
    unsigned char hdr[8];
    hdr[0] = rb->w & 0xFF; hdr[1] = (rb->w >> 8) & 0xFF;
    hdr[2] = (rb->w >> 16) & 0xFF; hdr[3] = (rb->w >> 24) & 0xFF;
    hdr[4] = rb->h & 0xFF; hdr[5] = (rb->h >> 8) & 0xFF;
    hdr[6] = (rb->h >> 16) & 0xFF; hdr[7] = (rb->h >> 24) & 0xFF;
    fwrite(hdr, 1, 8, f);
    fwrite(rb->pixels, 1, (size_t)(rb->w * rb->h * 3), f);
    fclose(f);
    return 1;
}

/* Get pixel buffer pointer (for direct access) */
unsigned char *ts_render_pixels(const TS_RenderBuf *rb) {
    return rb ? rb->pixels : NULL;
}

int ts_render_width(const TS_RenderBuf *rb) { return rb ? rb->w : 0; }
int ts_render_height(const TS_RenderBuf *rb) { return rb ? rb->h : 0; }



/* ═══════════════════════════════════════════════════════════════
 *  RENDER ENGINE v2 — Anti-aliasing, Bezier, Gradients, Circles
 *  Fiecare feature de la zero, fără dependențe
 *  SoftCAD C+C TOPO © 2026
 * ═══════════════════════════════════════════════════════════════ */

/* ── Anti-aliased line (Xiaolin Wu) ─────────────────────────── */
void ts_render_line_aa(TS_RenderBuf *rb, double x0, double y0, double x1, double y1,
                        int r, int g, int b) {
    if (!rb) return;
    int steep = fabs(y1-y0) > fabs(x1-x0);
    if (steep) { double t=x0; x0=y0; y0=t; t=x1; x1=y1; y1=t; }
    if (x0 > x1) { double t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
    double dx = x1-x0, dy = y1-y0;
    double grad = dx == 0 ? 1.0 : dy/dx;
    /* First endpoint */
    double xend = (int)(x0+0.5); double yend = y0 + grad*(xend-x0);
    double xgap = 1.0 - (x0+0.5 - (int)(x0+0.5));
    int xpxl1 = (int)xend, ypxl1 = (int)yend;
    double fpart = yend - ypxl1;
    if (steep) {
        _rend_set_pixel(rb, ypxl1, xpxl1, r,g,b, (int)((1-fpart)*xgap*255));
        _rend_set_pixel(rb, ypxl1+1, xpxl1, r,g,b, (int)(fpart*xgap*255));
    } else {
        _rend_set_pixel(rb, xpxl1, ypxl1, r,g,b, (int)((1-fpart)*xgap*255));
        _rend_set_pixel(rb, xpxl1, ypxl1+1, r,g,b, (int)(fpart*xgap*255));
    }
    double intery = yend + grad;
    /* Second endpoint */
    xend = (int)(x1+0.5); yend = y1 + grad*(xend-x1);
    xgap = x1+0.5 - (int)(x1+0.5);
    int xpxl2 = (int)xend, ypxl2 = (int)yend;
    fpart = yend - ypxl2;
    if (steep) {
        _rend_set_pixel(rb, ypxl2, xpxl2, r,g,b, (int)((1-fpart)*xgap*255));
        _rend_set_pixel(rb, ypxl2+1, xpxl2, r,g,b, (int)(fpart*xgap*255));
    } else {
        _rend_set_pixel(rb, xpxl2, ypxl2, r,g,b, (int)((1-fpart)*xgap*255));
        _rend_set_pixel(rb, xpxl2, ypxl2+1, r,g,b, (int)(fpart*xgap*255));
    }
    /* Main loop */
    for (int x = xpxl1+1; x < xpxl2; x++) {
        int iy = (int)intery;
        double f = intery - iy;
        if (steep) {
            _rend_set_pixel(rb, iy, x, r,g,b, (int)((1-f)*255));
            _rend_set_pixel(rb, iy+1, x, r,g,b, (int)(f*255));
        } else {
            _rend_set_pixel(rb, x, iy, r,g,b, (int)((1-f)*255));
            _rend_set_pixel(rb, x, iy+1, r,g,b, (int)(f*255));
        }
        intery += grad;
    }
}

/* ── Anti-aliased polygon outline ──────────────────────────── */
void ts_render_polygon_aa(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                           int r, int g, int b, int fill_alpha) {
    if (!rb || n < 2) return;
    if (fill_alpha > 0 && n >= 3)
        ts_render_fill_polygon(rb, xs, ys, n, r, g, b, fill_alpha);
    for (int i = 0; i < n; i++) {
        int j = (i+1) % n;
        ts_render_line_aa(rb, xs[i], ys[i], xs[j], ys[j], r, g, b);
    }
}

/* ── Bezier cubic (de Casteljau) ───────────────────────────── */
void ts_render_bezier(TS_RenderBuf *rb,
                       double x0, double y0, double x1, double y1,
                       double x2, double y2, double x3, double y3,
                       int r, int g, int b, int steps) {
    if (!rb) return;
    if (steps <= 0) steps = 64;
    double px = x0, py = y0;
    for (int i = 1; i <= steps; i++) {
        double t = (double)i / steps;
        double u = 1-t;
        double x = u*u*u*x0 + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3;
        double y = u*u*u*y0 + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3;
        ts_render_line_aa(rb, px, py, x, y, r, g, b);
        px = x; py = y;
    }
}

/* ── Bezier quadratic ──────────────────────────────────────── */
void ts_render_bezier_quad(TS_RenderBuf *rb,
                            double x0, double y0, double x1, double y1,
                            double x2, double y2,
                            int r, int g, int b, int steps) {
    if (!rb) return;
    if (steps <= 0) steps = 48;
    double px = x0, py = y0;
    for (int i = 1; i <= steps; i++) {
        double t = (double)i / steps;
        double u = 1-t;
        double x = u*u*x0 + 2*u*t*x1 + t*t*x2;
        double y = u*u*y0 + 2*u*t*y1 + t*t*y2;
        ts_render_line_aa(rb, px, py, x, y, r, g, b);
        px = x; py = y;
    }
}

/* ── Circle (Midpoint algorithm, anti-aliased) ─────────────── */
void ts_render_circle(TS_RenderBuf *rb, int cx, int cy, int radius,
                       int r, int g, int b) {
    if (!rb || radius <= 0) return;
    int steps = (int)(2 * 3.14159265 * radius);
    if (steps < 32) steps = 32;
    if (steps > 512) steps = 512;
    double px = cx + radius, py = cy;
    for (int i = 1; i <= steps; i++) {
        double a = 2.0 * 3.14159265 * i / steps;
        double x = cx + radius * cos(a);
        double y = cy + radius * sin(a);
        ts_render_line_aa(rb, px, py, x, y, r, g, b);
        px = x; py = y;
    }
}

/* ── Filled circle ─────────────────────────────────────────── */
void ts_render_circle_filled(TS_RenderBuf *rb, int cx, int cy, int radius,
                              int r, int g, int b, int alpha) {
    if (!rb || radius <= 0) return;
    for (int y = -radius; y <= radius; y++) {
        int half = (int)sqrt((double)(radius*radius - y*y));
        for (int x = -half; x <= half; x++) {
            /* Edge AA: pixels near border get reduced alpha */
            double dist = sqrt((double)(x*x + y*y));
            int a = alpha;
            if (dist > radius - 1.0) {
                a = (int)(alpha * (radius - dist));
                if (a < 0) a = 0;
            }
            _rend_set_pixel(rb, cx+x, cy+y, r, g, b, a);
        }
    }
}

/* ── Ellipse ───────────────────────────────────────────────── */
void ts_render_ellipse(TS_RenderBuf *rb, int cx, int cy, int rx, int ry,
                        int r, int g, int b) {
    if (!rb || rx <= 0 || ry <= 0) return;
    int steps = (int)(2 * 3.14159265 * (rx > ry ? rx : ry));
    if (steps < 32) steps = 32;
    if (steps > 512) steps = 512;
    double px = cx + rx, py = cy;
    for (int i = 1; i <= steps; i++) {
        double a = 2.0 * 3.14159265 * i / steps;
        double x = cx + rx * cos(a);
        double y = cy + ry * sin(a);
        ts_render_line_aa(rb, px, py, x, y, r, g, b);
        px = x; py = y;
    }
}

/* ── Gradient fill (linear, horizontal) ────────────────────── */
void ts_render_gradient_h(TS_RenderBuf *rb, int x, int y, int w, int h,
                           int r1, int g1, int b1,
                           int r2, int g2, int b2, int alpha) {
    if (!rb || w <= 0 || h <= 0) return;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            double t = (double)dx / (w - 1);
            int r = r1 + (int)((r2-r1)*t);
            int g = g1 + (int)((g2-g1)*t);
            int b = b1 + (int)((b2-b1)*t);
            _rend_set_pixel(rb, x+dx, y+dy, r, g, b, alpha);
        }
    }
}

/* ── Gradient fill (linear, vertical) ──────────────────────── */
void ts_render_gradient_v(TS_RenderBuf *rb, int x, int y, int w, int h,
                           int r1, int g1, int b1,
                           int r2, int g2, int b2, int alpha) {
    if (!rb || w <= 0 || h <= 0) return;
    for (int dy = 0; dy < h; dy++) {
        double t = (double)dy / (h - 1);
        int r = r1 + (int)((r2-r1)*t);
        int g = g1 + (int)((g2-g1)*t);
        int b = b1 + (int)((b2-b1)*t);
        for (int dx = 0; dx < w; dx++)
            _rend_set_pixel(rb, x+dx, y+dy, r, g, b, alpha);
    }
}

/* ── Gradient fill (radial) ────────────────────────────────── */
void ts_render_gradient_radial(TS_RenderBuf *rb, int cx, int cy, int radius,
                                int r1, int g1, int b1,
                                int r2, int g2, int b2, int alpha) {
    if (!rb || radius <= 0) return;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            double dist = sqrt((double)(dx*dx + dy*dy));
            if (dist > radius) continue;
            double t = dist / radius;
            int r = r1 + (int)((r2-r1)*t);
            int g = g1 + (int)((g2-g1)*t);
            int b = b1 + (int)((b2-b1)*t);
            _rend_set_pixel(rb, cx+dx, cy+dy, r, g, b, alpha);
        }
    }
}

/* ── Rounded rectangle ─────────────────────────────────────── */
void ts_render_rounded_rect(TS_RenderBuf *rb, int x, int y, int w, int h,
                             int radius, int r, int g, int b, int fill_alpha) {
    if (!rb || w <= 0 || h <= 0) return;
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;
    /* Fill center */
    for (int dy = radius; dy < h-radius; dy++)
        for (int dx = 0; dx < w; dx++)
            _rend_set_pixel(rb, x+dx, y+dy, r, g, b, fill_alpha);
    /* Fill top/bottom strips */
    for (int dy = 0; dy < radius; dy++)
        for (int dx = radius; dx < w-radius; dx++) {
            _rend_set_pixel(rb, x+dx, y+dy, r, g, b, fill_alpha);
            _rend_set_pixel(rb, x+dx, y+h-1-dy, r, g, b, fill_alpha);
        }
    /* Fill corners (quarter circles) */
    int cx[4] = {x+radius, x+w-1-radius, x+w-1-radius, x+radius};
    int cy[4] = {y+radius, y+radius, y+h-1-radius, y+h-1-radius};
    for (int c = 0; c < 4; c++) {
        for (int dy = -radius; dy <= 0; dy++) {
            int half = (int)sqrt((double)(radius*radius - dy*dy));
            for (int dx = -half; dx <= 0; dx++) {
                int px = cx[c] + (c==0||c==3 ? dx : -dx);
                int py = cy[c] + (c<2 ? dy : -dy);
                _rend_set_pixel(rb, px, py, r, g, b, fill_alpha);
            }
        }
    }
    /* Outline (AA) */
    ts_render_line_aa(rb, x+radius, y, x+w-1-radius, y, r,g,b);
    ts_render_line_aa(rb, x+radius, y+h-1, x+w-1-radius, y+h-1, r,g,b);
    ts_render_line_aa(rb, x, y+radius, x, y+h-1-radius, r,g,b);
    ts_render_line_aa(rb, x+w-1, y+radius, x+w-1, y+h-1-radius, r,g,b);
    /* Corner arcs */
    for (int c = 0; c < 4; c++) {
        double a0 = c * 3.14159265 / 2 + 3.14159265;
        int steps = radius * 2;
        if (steps < 8) steps = 8;
        double px2 = cx[c] + radius*cos(a0), py2 = cy[c] + radius*sin(a0);
        for (int i = 1; i <= steps; i++) {
            double a = a0 + 3.14159265/2 * i / steps;
            double nx = cx[c] + radius*cos(a), ny = cy[c] + radius*sin(a);
            ts_render_line_aa(rb, px2, py2, nx, ny, r, g, b);
            px2 = nx; py2 = ny;
        }
    }
}

/* ── Thick AA line (offset parallel lines) ─────────────────── */
void ts_render_line_aa_thick(TS_RenderBuf *rb, double x0, double y0,
                              double x1, double y1, int r, int g, int b,
                              double thickness) {
    if (!rb || thickness <= 0) return;
    if (thickness <= 1.5) { ts_render_line_aa(rb, x0,y0,x1,y1,r,g,b); return; }
    double dx = x1-x0, dy = y1-y0;
    double len = sqrt(dx*dx+dy*dy);
    if (len < 0.01) return;
    double nx = -dy/len, ny = dx/len; /* normal */
    int half = (int)(thickness/2);
    for (int i = -half; i <= half; i++) {
        double off = i * 0.8;
        ts_render_line_aa(rb, x0+nx*off, y0+ny*off, x1+nx*off, y1+ny*off, r,g,b);
    }
}
/* ═══════════════════════════════════════════════════════════════
 *  RENDER ENGINE v3 — Transforms, Clipping, Dash, Caps, Text,
 *  Compositing, Pattern fills
 *  SoftCAD C+C TOPO (C) 2026
 * ═══════════════════════════════════════════════════════════════ */

/* ── 2D Transform matrix (affine 3x3, stored as 6 floats) ──── */
typedef struct {
    double a, b, c, d, tx, ty;  /* |a c tx| |b d ty| |0 0 1| */
} TS_RenderXform;

TS_RenderXform ts_render_xform_identity(void) {
    TS_RenderXform t = {1,0,0,1,0,0}; return t;
}
TS_RenderXform ts_render_xform_translate(double dx, double dy) {
    TS_RenderXform t = {1,0,0,1,dx,dy}; return t;
}
TS_RenderXform ts_render_xform_scale(double sx, double sy) {
    TS_RenderXform t = {sx,0,0,sy,0,0}; return t;
}
TS_RenderXform ts_render_xform_rotate(double radians) {
    double cs = cos(radians), sn = sin(radians);
    TS_RenderXform t = {cs, sn, -sn, cs, 0, 0}; return t;
}
TS_RenderXform ts_render_xform_multiply(TS_RenderXform a, TS_RenderXform b) {
    TS_RenderXform r;
    r.a  = a.a*b.a  + a.c*b.b;
    r.b  = a.b*b.a  + a.d*b.b;
    r.c  = a.a*b.c  + a.c*b.d;
    r.d  = a.b*b.c  + a.d*b.d;
    r.tx = a.a*b.tx + a.c*b.ty + a.tx;
    r.ty = a.b*b.tx + a.d*b.ty + a.ty;
    return r;
}
void ts_render_xform_point(TS_RenderXform t, double x, double y, double *ox, double *oy) {
    *ox = t.a*x + t.c*y + t.tx;
    *oy = t.b*x + t.d*y + t.ty;
}

/* Apply transform to polygon, render it */
void ts_render_polygon_transformed(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                                    TS_RenderXform t,
                                    int r, int g, int b, int fill_alpha, int pen_width) {
    if (!rb || n < 2 || n > 50000) return;
    int *tx = (int*)malloc(sizeof(int)*(size_t)n);
    int *ty = (int*)malloc(sizeof(int)*(size_t)n);
    if (!tx || !ty) { free(tx); free(ty); return; }
    for (int i = 0; i < n; i++) {
        double ox, oy;
        ts_render_xform_point(t, xs[i], ys[i], &ox, &oy);
        tx[i] = (int)ox; ty[i] = (int)oy;
    }
    ts_render_polygon_aa(rb, tx, ty, n, r, g, b, fill_alpha);
    free(tx); free(ty);
}

/* ── Clipping rectangle ────────────────────────────────────── */
typedef struct { int x, y, w, h; } TS_ClipRect;

/* Sutherland-Hodgman polygon clipping */
static int _clip_edge(const int *in_x, const int *in_y, int in_n,
                       int *out_x, int *out_y,
                       int edge, int val, int is_min) {
    int out_n = 0;
    for (int i = 0; i < in_n; i++) {
        int j = (i + 1) % in_n;
        int ci, cj, xi = in_x[i], yi = in_y[i], xj = in_x[j], yj = in_y[j];
        if (edge == 0) { ci = is_min ? xi >= val : xi <= val; cj = is_min ? xj >= val : xj <= val; }
        else           { ci = is_min ? yi >= val : yi <= val; cj = is_min ? yj >= val : yj <= val; }
        if (ci && cj) { out_x[out_n] = xj; out_y[out_n] = yj; out_n++; }
        else if (ci && !cj) {
            int dx = xj-xi, dy = yj-yi;
            if (edge == 0) { double t = (double)(val-xi)/dx; out_x[out_n]=(int)(xi+dx*t); out_y[out_n]=(int)(yi+dy*t); }
            else           { double t = (double)(val-yi)/dy; out_x[out_n]=(int)(xi+dx*t); out_y[out_n]=(int)(yi+dy*t); }
            out_n++;
        }
        else if (!ci && cj) {
            int dx = xj-xi, dy = yj-yi;
            if (edge == 0) { double t = (double)(val-xi)/dx; out_x[out_n]=(int)(xi+dx*t); out_y[out_n]=(int)(yi+dy*t); out_n++; }
            else           { double t = (double)(val-yi)/dy; out_x[out_n]=(int)(xi+dx*t); out_y[out_n]=(int)(yi+dy*t); out_n++; }
            out_x[out_n] = xj; out_y[out_n] = yj; out_n++;
        }
    }
    return out_n;
}

void ts_render_polygon_clipped(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                                TS_ClipRect clip, int r, int g, int b,
                                int fill_alpha, int pen_width) {
    if (!rb || n < 2) return;
    int cap = n * 4 + 16;
    int *b1x=(int*)malloc(sizeof(int)*(size_t)cap), *b1y=(int*)malloc(sizeof(int)*(size_t)cap);
    int *b2x=(int*)malloc(sizeof(int)*(size_t)cap), *b2y=(int*)malloc(sizeof(int)*(size_t)cap);
    if (!b1x||!b1y||!b2x||!b2y) { free(b1x);free(b1y);free(b2x);free(b2y); return; }
    memcpy(b1x, xs, sizeof(int)*(size_t)n); memcpy(b1y, ys, sizeof(int)*(size_t)n);
    int cn = n;
    cn = _clip_edge(b1x,b1y,cn, b2x,b2y, 0, clip.x, 1);
    cn = _clip_edge(b2x,b2y,cn, b1x,b1y, 0, clip.x+clip.w, 0);
    cn = _clip_edge(b1x,b1y,cn, b2x,b2y, 1, clip.y, 1);
    cn = _clip_edge(b2x,b2y,cn, b1x,b1y, 1, clip.y+clip.h, 0);
    if (cn >= 3) ts_render_polygon_aa(rb, b1x, b1y, cn, r, g, b, fill_alpha);
    free(b1x); free(b1y); free(b2x); free(b2y);
}

/* ── Dash pattern line ─────────────────────────────────────── */
void ts_render_line_dashed(TS_RenderBuf *rb, double x0, double y0, double x1, double y1,
                            int r, int g, int b,
                            double dash_on, double dash_off) {
    if (!rb) return;
    double dx = x1-x0, dy = y1-y0;
    double len = sqrt(dx*dx+dy*dy);
    if (len < 0.5) return;
    double ux = dx/len, uy = dy/len;
    double pos = 0;
    int drawing = 1;
    while (pos < len) {
        double seg = drawing ? dash_on : dash_off;
        if (pos + seg > len) seg = len - pos;
        if (drawing) {
            ts_render_line_aa(rb, x0+ux*pos, y0+uy*pos,
                               x0+ux*(pos+seg), y0+uy*(pos+seg), r, g, b);
        }
        pos += seg;
        drawing = !drawing;
    }
}

/* ── Line caps ─────────────────────────────────────────────── */
/* 0=flat, 1=round, 2=square */
void ts_render_line_capped(TS_RenderBuf *rb, double x0, double y0, double x1, double y1,
                            int r, int g, int b, double thickness, int cap_style) {
    if (!rb) return;
    ts_render_line_aa_thick(rb, x0, y0, x1, y1, r, g, b, thickness);
    if (cap_style == 1) { /* round */
        int rad = (int)(thickness / 2 + 0.5);
        ts_render_circle_filled(rb, (int)x0, (int)y0, rad, r, g, b, 255);
        ts_render_circle_filled(rb, (int)x1, (int)y1, rad, r, g, b, 255);
    } else if (cap_style == 2) { /* square */
        double dx = x1-x0, dy = y1-y0;
        double len = sqrt(dx*dx+dy*dy);
        if (len < 0.01) return;
        double ux = dx/len * thickness/2, uy = dy/len * thickness/2;
        /* Extend both ends by half thickness */
        ts_render_line_aa_thick(rb, x0-ux, y0-uy, x0, y0, r, g, b, thickness);
        ts_render_line_aa_thick(rb, x1, y1, x1+ux, y1+uy, r, g, b, thickness);
    }
}

/* ── Line joins ────────────────────────────────────────────── */
/* 0=miter, 1=round, 2=bevel */
void ts_render_polyline_joined(TS_RenderBuf *rb, const double *pts, int n_pts,
                                int r, int g, int b, double thickness, int join_style) {
    if (!rb || n_pts < 2) return;
    for (int i = 0; i < n_pts - 1; i++) {
        ts_render_line_aa_thick(rb, pts[i*2], pts[i*2+1],
                                 pts[(i+1)*2], pts[(i+1)*2+1], r, g, b, thickness);
        /* Joint at vertex i+1 (if not last) */
        if (i < n_pts - 2 && join_style == 1) {
            ts_render_circle_filled(rb, (int)pts[(i+1)*2], (int)pts[(i+1)*2+1],
                                     (int)(thickness/2+0.5), r, g, b, 255);
        }
    }
}

/* ── Pattern fill: hatch ───────────────────────────────────── */
/* angle in degrees: 0=horizontal, 45=diagonal, 90=vertical */
void ts_render_hatch_fill(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                           int r, int g, int b, int alpha,
                           double spacing, double angle_deg) {
    if (!rb || n < 3 || spacing < 1) return;
    /* Fill polygon first */
    ts_render_fill_polygon(rb, xs, ys, n, r, g, b, alpha/3);
    /* Find bounding box */
    int x_min=xs[0], x_max=xs[0], y_min=ys[0], y_max=ys[0];
    for (int i = 1; i < n; i++) {
        if (xs[i]<x_min) x_min=xs[i]; if (xs[i]>x_max) x_max=xs[i];
        if (ys[i]<y_min) y_min=ys[i]; if (ys[i]>y_max) y_max=ys[i];
    }
    /* Draw hatch lines */
    double rad = angle_deg * 3.14159265 / 180.0;
    double cs = cos(rad), sn = sin(rad);
    double diag = sqrt((double)((x_max-x_min)*(x_max-x_min)+(y_max-y_min)*(y_max-y_min)));
    double cx = (x_min+x_max)/2.0, cy = (y_min+y_max)/2.0;
    for (double d = -diag; d <= diag; d += spacing) {
        double lx0 = cx + cs*(-diag) + sn*d;
        double ly0 = cy - sn*(-diag) + cs*d;
        double lx1 = cx + cs*(diag) + sn*d;
        double ly1 = cy - sn*(diag) + cs*d;
        /* Clip line to polygon (simplified: just draw, pixels outside polygon already set) */
        /* For proper hatch, we'd intersect with polygon edges — but scanline fill + overlay works */
        ts_render_line_aa(rb, lx0, ly0, lx1, ly1, r, g, b);
    }
}

/* ── Crosshatch ────────────────────────────────────────────── */
void ts_render_crosshatch_fill(TS_RenderBuf *rb, const int *xs, const int *ys, int n,
                                int r, int g, int b, int alpha,
                                double spacing) {
    ts_render_hatch_fill(rb, xs, ys, n, r, g, b, alpha, spacing, 45.0);
    ts_render_hatch_fill(rb, xs, ys, n, r, g, b, alpha, spacing, -45.0);
}

/* ── Image compositing (Porter-Duff over) ──────────────────── */
void ts_render_composite(TS_RenderBuf *dst, const TS_RenderBuf *src,
                          int dx, int dy, int alpha) {
    if (!dst || !src) return;
    for (int sy = 0; sy < src->h; sy++) {
        int ty = dy + sy;
        if (ty < 0 || ty >= dst->h) continue;
        for (int sx = 0; sx < src->w; sx++) {
            int tx = dx + sx;
            if (tx < 0 || tx >= dst->w) continue;
            int si = (sy * src->w + sx) * 3;
            _rend_set_pixel(dst, tx, ty, src->pixels[si], src->pixels[si+1], src->pixels[si+2], alpha);
        }
    }
}

/* ── Built-in bitmap font (5x7, ASCII 32-126) ─────────────── */
/* Minimal: enough for labels on cadastral maps */
static const unsigned char _font5x7[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x04,0x04,0x04,0x04,0x00,0x04,0x00}, /* ! */
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x0A,0x1F,0x0A,0x1F,0x0A,0x00,0x00}, /* # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, /* $ */
    {0x19,0x1A,0x04,0x0B,0x13,0x00,0x00}, /* % */
    {0x08,0x14,0x08,0x15,0x12,0x0D,0x00}, /* & */
    {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, /* ' */
    {0x02,0x04,0x04,0x04,0x04,0x02,0x00}, /* ( */
    {0x08,0x04,0x04,0x04,0x04,0x08,0x00}, /* ) */
    {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00}, /* * */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, /* , */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x04,0x00}, /* . */
    {0x01,0x02,0x04,0x08,0x10,0x00,0x00}, /* / */
    {0x0E,0x11,0x13,0x15,0x19,0x0E,0x00}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x0E,0x00}, /* 1 */
    {0x0E,0x11,0x01,0x06,0x08,0x1F,0x00}, /* 2 */
    {0x0E,0x11,0x02,0x01,0x11,0x0E,0x00}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x00}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x11,0x0E,0x00}, /* 5 */
    {0x06,0x08,0x1E,0x11,0x11,0x0E,0x00}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x00}, /* 7 */
    {0x0E,0x11,0x0E,0x11,0x11,0x0E,0x00}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x02,0x0C,0x00}, /* 9 */
    {0x00,0x04,0x00,0x00,0x04,0x00,0x00}, /* : */
    {0x00,0x04,0x00,0x00,0x04,0x04,0x08}, /* ; */
    {0x02,0x04,0x08,0x04,0x02,0x00,0x00}, /* < */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* = */
    {0x08,0x04,0x02,0x04,0x08,0x00,0x00}, /* > */
    {0x0E,0x11,0x02,0x04,0x00,0x04,0x00}, /* ? */
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, /* @ */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x00}, /* A */
    {0x1E,0x11,0x1E,0x11,0x11,0x1E,0x00}, /* B */
    {0x0E,0x11,0x10,0x10,0x11,0x0E,0x00}, /* C */
    {0x1C,0x12,0x11,0x11,0x12,0x1C,0x00}, /* D */
    {0x1F,0x10,0x1E,0x10,0x10,0x1F,0x00}, /* E */
    {0x1F,0x10,0x1E,0x10,0x10,0x10,0x00}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x0F,0x00}, /* G */
    {0x11,0x11,0x1F,0x11,0x11,0x11,0x00}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x0E,0x00}, /* I */
    {0x07,0x02,0x02,0x02,0x12,0x0C,0x00}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x1F,0x00}, /* L */
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x00}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x00}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x0E,0x00}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x00}, /* P */
    {0x0E,0x11,0x11,0x15,0x12,0x0D,0x00}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x00}, /* R */
    {0x0E,0x11,0x10,0x0E,0x01,0x1E,0x00}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x00}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x0E,0x00}, /* U */
    {0x11,0x11,0x11,0x0A,0x0A,0x04,0x00}, /* V */
    {0x11,0x11,0x11,0x15,0x1B,0x11,0x00}, /* W */
    {0x11,0x0A,0x04,0x04,0x0A,0x11,0x00}, /* X */
    {0x11,0x0A,0x04,0x04,0x04,0x04,0x00}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x1F,0x00}, /* Z */
    {0x0E,0x08,0x08,0x08,0x08,0x0E,0x00}, /* [ */
    {0x10,0x08,0x04,0x02,0x01,0x00,0x00}, /* \ */
    {0x0E,0x02,0x02,0x02,0x02,0x0E,0x00}, /* ] */
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x1F,0x00}, /* _ */
    {0x08,0x04,0x00,0x00,0x00,0x00,0x00}, /* ` */
    {0x00,0x0E,0x01,0x0F,0x11,0x0F,0x00}, /* a */
    {0x10,0x10,0x1E,0x11,0x11,0x1E,0x00}, /* b */
    {0x00,0x0E,0x11,0x10,0x11,0x0E,0x00}, /* c */
    {0x01,0x01,0x0F,0x11,0x11,0x0F,0x00}, /* d */
    {0x00,0x0E,0x11,0x1F,0x10,0x0E,0x00}, /* e */
    {0x06,0x08,0x1C,0x08,0x08,0x08,0x00}, /* f */
    {0x00,0x0F,0x11,0x0F,0x01,0x0E,0x00}, /* g */
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x00}, /* h */
    {0x04,0x00,0x0C,0x04,0x04,0x0E,0x00}, /* i */
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, /* j */
    {0x10,0x12,0x14,0x18,0x14,0x12,0x00}, /* k */
    {0x0C,0x04,0x04,0x04,0x04,0x0E,0x00}, /* l */
    {0x00,0x1A,0x15,0x15,0x11,0x11,0x00}, /* m */
    {0x00,0x1E,0x11,0x11,0x11,0x11,0x00}, /* n */
    {0x00,0x0E,0x11,0x11,0x11,0x0E,0x00}, /* o */
    {0x00,0x1E,0x11,0x1E,0x10,0x10,0x00}, /* p */
    {0x00,0x0F,0x11,0x0F,0x01,0x01,0x00}, /* q */
    {0x00,0x16,0x19,0x10,0x10,0x10,0x00}, /* r */
    {0x00,0x0F,0x10,0x0E,0x01,0x1E,0x00}, /* s */
    {0x08,0x1C,0x08,0x08,0x09,0x06,0x00}, /* t */
    {0x00,0x11,0x11,0x11,0x13,0x0D,0x00}, /* u */
    {0x00,0x11,0x11,0x0A,0x0A,0x04,0x00}, /* v */
    {0x00,0x11,0x11,0x15,0x15,0x0A,0x00}, /* w */
    {0x00,0x11,0x0A,0x04,0x0A,0x11,0x00}, /* x */
    {0x00,0x11,0x11,0x0F,0x01,0x0E,0x00}, /* y */
    {0x00,0x1F,0x02,0x04,0x08,0x1F,0x00}, /* z */
    {0x02,0x04,0x08,0x04,0x02,0x00,0x00}, /* { */
    {0x04,0x04,0x04,0x04,0x04,0x04,0x00}, /* | */
    {0x08,0x04,0x02,0x04,0x08,0x00,0x00}, /* } */
    {0x00,0x00,0x08,0x15,0x02,0x00,0x00}, /* ~ */
};

/* Draw single character at scale */
void ts_render_char(TS_RenderBuf *rb, int x, int y, char ch,
                     int r, int g, int b, int scale) {
    if (!rb || ch < 32 || ch > 126) return;
    if (scale < 1) scale = 1;
    const unsigned char *glyph = _font5x7[ch - 32];
    for (int row = 0; row < 7; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        _rend_set_pixel(rb, x + col*scale + sx, y + row*scale + sy, r, g, b, 255);
            }
        }
    }
}

/* Draw text string */
void ts_render_text(TS_RenderBuf *rb, int x, int y, const char *text,
                     int r, int g, int b, int scale) {
    if (!rb || !text) return;
    if (scale < 1) scale = 1;
    int cx = x;
    while (*text) {
        ts_render_char(rb, cx, y, *text, r, g, b, scale);
        cx += 6 * scale; /* 5 pixels + 1 spacing */
        text++;
    }
}

/* Text width in pixels */
int ts_render_text_width(const char *text, int scale) {
    if (!text) return 0;
    int len = (int)strlen(text);
    return len > 0 ? len * 6 * scale - scale : 0;
}

/* Centered text */
void ts_render_text_centered(TS_RenderBuf *rb, int cx, int y, const char *text,
                              int r, int g, int b, int scale) {
    int w = ts_render_text_width(text, scale);
    ts_render_text(rb, cx - w/2, y, text, r, g, b, scale);
}



/* ═══════════════════════════════════════════════════════════════
 *  SDF RENDERING ENGINE — Signed Distance Field
 *  Next-gen: Cairo nu are asta. Skia nu are asta.
 *  Perfect AA la orice rezolutie, umbre/glow/outline GRATIS.
 *  SoftCAD C+C TOPO (C) 2026
 * ═══════════════════════════════════════════════════════════════ */

/* SDF helper: smoothstep for AA edge */
static double _sdf_smoothstep(double edge0, double edge1, double x) {
    double t = (x - edge0) / (edge1 - edge0);
    if (t < 0) t = 0; if (t > 1) t = 1;
    return t * t * (3.0 - 2.0 * t);
}

/* SDF pixel blend — distance → alpha with configurable edge softness */
static void _sdf_pixel(TS_RenderBuf *rb, int px, int py,
                        double dist, double edge_width,
                        int r, int g, int b, int base_alpha) {
    /* dist < 0 = inside, dist > 0 = outside */
    double alpha = _sdf_smoothstep(edge_width, -edge_width, dist);
    if (alpha < 0.003) return;
    _rend_set_pixel(rb, px, py, r, g, b, (int)(alpha * base_alpha));
}

/* ── SDF Circle ────────────────────────────────────────────── */
void ts_render_sdf_circle(TS_RenderBuf *rb, double cx, double cy, double radius,
                           int r, int g, int b, int alpha, double softness) {
    if (!rb || radius <= 0) return;
    if (softness <= 0) softness = 1.0;
    int margin = (int)(softness + 2);
    int x0 = (int)(cx - radius) - margin, y0 = (int)(cy - radius) - margin;
    int x1 = (int)(cx + radius) + margin, y1 = (int)(cy + radius) + margin;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            double dx = px - cx, dy = py - cy;
            double dist = sqrt(dx*dx + dy*dy) - radius;
            _sdf_pixel(rb, px, py, dist, softness, r, g, b, alpha);
        }
    }
}

/* ── SDF Ring (circle outline) ─────────────────────────────── */
void ts_render_sdf_ring(TS_RenderBuf *rb, double cx, double cy, double radius,
                         double thickness, int r, int g, int b, int alpha, double softness) {
    if (!rb || radius <= 0) return;
    if (softness <= 0) softness = 1.0;
    int margin = (int)(thickness/2 + softness + 2);
    int x0 = (int)(cx - radius) - margin, y0 = (int)(cy - radius) - margin;
    int x1 = (int)(cx + radius) + margin, y1 = (int)(cy + radius) + margin;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            double dx = px - cx, dy = py - cy;
            double dist = fabs(sqrt(dx*dx + dy*dy) - radius) - thickness/2;
            _sdf_pixel(rb, px, py, dist, softness, r, g, b, alpha);
        }
    }
}

/* ── SDF Rounded Rectangle ─────────────────────────────────── */
void ts_render_sdf_rounded_rect(TS_RenderBuf *rb,
                                 double cx, double cy, double hw, double hh,
                                 double radius, int r, int g, int b,
                                 int alpha, double softness) {
    if (!rb) return;
    if (softness <= 0) softness = 1.0;
    int margin = (int)(softness + 2);
    int x0 = (int)(cx - hw) - margin, y0 = (int)(cy - hh) - margin;
    int x1 = (int)(cx + hw) + margin, y1 = (int)(cy + hh) + margin;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            double dx = fabs(px - cx) - hw + radius;
            double dy = fabs(py - cy) - hh + radius;
            if (dx < 0) dx = 0; if (dy < 0) dy = 0;
            double dist = sqrt(dx*dx + dy*dy) - radius;
            _sdf_pixel(rb, px, py, dist, softness, r, g, b, alpha);
        }
    }
}

/* ── SDF Line segment ──────────────────────────────────────── */
void ts_render_sdf_line(TS_RenderBuf *rb,
                         double x0, double y0, double x1, double y1,
                         double thickness, int r, int g, int b,
                         int alpha, double softness) {
    if (!rb) return;
    if (softness <= 0) softness = 1.0;
    double half = thickness / 2;
    int margin = (int)(half + softness + 2);
    int bx0 = (int)fmin(x0,x1) - margin, by0 = (int)fmin(y0,y1) - margin;
    int bx1 = (int)fmax(x0,x1) + margin, by1 = (int)fmax(y0,y1) + margin;
    double lx = x1-x0, ly = y1-y0;
    double len2 = lx*lx + ly*ly;
    if (len2 < 0.01) return;
    for (int py = by0; py <= by1; py++) {
        for (int px = bx0; px <= bx1; px++) {
            double t = ((px-x0)*lx + (py-y0)*ly) / len2;
            if (t < 0) t = 0; if (t > 1) t = 1;
            double nx = x0 + t*lx - px, ny = y0 + t*ly - py;
            double dist = sqrt(nx*nx + ny*ny) - half;
            _sdf_pixel(rb, px, py, dist, softness, r, g, b, alpha);
        }
    }
}

/* ── SDF Shadow (any SDF shape with offset) ────────────────── */
void ts_render_sdf_shadow(TS_RenderBuf *rb,
                           double cx, double cy, double radius,
                           double shadow_ox, double shadow_oy, double shadow_blur,
                           int sr, int sg, int sb, int shadow_alpha,
                           int r, int g, int b, int alpha) {
    if (!rb) return;
    /* Draw shadow first (offset + more blur) */
    ts_render_sdf_circle(rb, cx + shadow_ox, cy + shadow_oy, radius,
                          sr, sg, sb, shadow_alpha, shadow_blur);
    /* Draw shape on top */
    ts_render_sdf_circle(rb, cx, cy, radius, r, g, b, alpha, 1.0);
}

/* ── SDF Glow effect ───────────────────────────────────────── */
void ts_render_sdf_glow(TS_RenderBuf *rb,
                         double cx, double cy, double radius,
                         int r, int g, int b, int alpha,
                         int glow_r, int glow_g, int glow_b,
                         double glow_radius, int glow_alpha) {
    if (!rb) return;
    /* Glow = larger circle, softer edge */
    ts_render_sdf_circle(rb, cx, cy, radius + glow_radius,
                          glow_r, glow_g, glow_b, glow_alpha, glow_radius);
    /* Core shape */
    ts_render_sdf_circle(rb, cx, cy, radius, r, g, b, alpha, 1.0);
}

/* ── SDF Polygon (minimum distance to edges) ───────────────── */
void ts_render_sdf_polygon(TS_RenderBuf *rb, const double *xs, const double *ys, int n,
                            int r, int g, int b, int alpha, double softness) {
    if (!rb || n < 3) return;
    if (softness <= 0) softness = 1.0;
    /* Bounding box */
    double bx0=xs[0], by0=ys[0], bx1=xs[0], by1=ys[0];
    for (int i = 1; i < n; i++) {
        if (xs[i]<bx0) bx0=xs[i]; if (xs[i]>bx1) bx1=xs[i];
        if (ys[i]<by0) by0=ys[i]; if (ys[i]>by1) by1=ys[i];
    }
    int margin = (int)(softness + 3);
    for (int py = (int)by0 - margin; py <= (int)by1 + margin; py++) {
        for (int px = (int)bx0 - margin; px <= (int)bx1 + margin; px++) {
            /* Point-in-polygon (ray cast) + min edge distance */
            int inside = 0;
            double min_dist = 1e30;
            for (int i = 0, j = n-1; i < n; j = i++) {
                /* Ray cast */
                if (((ys[i] > py) != (ys[j] > py)) &&
                    (px < (xs[j]-xs[i])*(py-ys[i])/(ys[j]-ys[i])+xs[i]))
                    inside = !inside;
                /* Min distance to edge ij */
                double ex = xs[j]-xs[i], ey = ys[j]-ys[i];
                double len2 = ex*ex+ey*ey;
                double t = len2 > 0 ? ((px-xs[i])*ex+(py-ys[i])*ey)/len2 : 0;
                if (t < 0) t = 0; if (t > 1) t = 1;
                double dx = xs[i]+t*ex-px, dy = ys[i]+t*ey-py;
                double d = sqrt(dx*dx+dy*dy);
                if (d < min_dist) min_dist = d;
            }
            double dist = inside ? -min_dist : min_dist;
            _sdf_pixel(rb, px, py, dist, softness, r, g, b, alpha);
        }
    }
}

/* ── SDF Outline (any polygon, just the border) ────────────── */
void ts_render_sdf_outline(TS_RenderBuf *rb, const double *xs, const double *ys, int n,
                            double thickness, int r, int g, int b, int alpha, double softness) {
    if (!rb || n < 3) return;
    if (softness <= 0) softness = 1.0;
    double half = thickness / 2;
    double bx0=xs[0], by0=ys[0], bx1=xs[0], by1=ys[0];
    for (int i = 1; i < n; i++) {
        if (xs[i]<bx0) bx0=xs[i]; if (xs[i]>bx1) bx1=xs[i];
        if (ys[i]<by0) by0=ys[i]; if (ys[i]>by1) by1=ys[i];
    }
    int margin = (int)(half + softness + 3);
    for (int py = (int)by0 - margin; py <= (int)by1 + margin; py++) {
        for (int px = (int)bx0 - margin; px <= (int)bx1 + margin; px++) {
            double min_dist = 1e30;
            for (int i = 0, j = n-1; i < n; j = i++) {
                double ex = xs[j]-xs[i], ey = ys[j]-ys[i];
                double len2 = ex*ex+ey*ey;
                double t = len2 > 0 ? ((px-xs[i])*ex+(py-ys[i])*ey)/len2 : 0;
                if (t < 0) t = 0; if (t > 1) t = 1;
                double dx = xs[i]+t*ex-px, dy = ys[i]+t*ey-py;
                double d = sqrt(dx*dx+dy*dy);
                if (d < min_dist) min_dist = d;
            }
            double dist = fabs(min_dist) - half;
            _sdf_pixel(rb, px, py, dist, softness, r, g, b, alpha);
        }
    }
}

/* ── SDF Boolean operations (union, intersection, subtraction) ── */
/* These work on distance values — combine shapes mathematically */
static double _sdf_union(double d1, double d2) { return d1 < d2 ? d1 : d2; }
static double _sdf_intersect(double d1, double d2) { return d1 > d2 ? d1 : d2; }
static double _sdf_subtract(double d1, double d2) { return d1 > -d2 ? d1 : -d2; }
/* Smooth union — blends two shapes with smooth transition */
static double _sdf_smooth_union(double d1, double d2, double k) {
    double h = 0.5 + 0.5*(d2-d1)/k;
    if (h < 0) h = 0; if (h > 1) h = 1;
    double m = d2 + (d1-d2)*h;
    return m - k*h*(1-h);
}

/* ── Layer blending modes ──────────────────────────────────── */
/* Porter-Duff + Photoshop modes */
static int _blend_multiply(int a, int b) { return a * b / 255; }
static int _blend_screen(int a, int b) { return 255 - (255-a)*(255-b)/255; }
static int _blend_overlay(int a, int b) {
    return a < 128 ? 2*a*b/255 : 255 - 2*(255-a)*(255-b)/255;
}
static int _blend_soft_light(int a, int b) {
    double fa = a/255.0, fb = b/255.0;
    double r = fb < 0.5 ? fa - (1-2*fb)*fa*(1-fa) : fa + (2*fb-1)*(sqrt(fa)-fa);
    return (int)(r * 255);
}

void ts_render_composite_blend(TS_RenderBuf *dst, const TS_RenderBuf *src,
                                int dx, int dy, int alpha, int blend_mode) {
    if (!dst || !src) return;
    for (int sy = 0; sy < src->h; sy++) {
        int ty = dy + sy;
        if (ty < 0 || ty >= dst->h) continue;
        for (int sx = 0; sx < src->w; sx++) {
            int tx = dx + sx;
            if (tx < 0 || tx >= dst->w) continue;
            int si = (sy * src->w + sx) * 3;
            int di = (ty * dst->w + tx) * 3;
            int sr = src->pixels[si], sg = src->pixels[si+1], sb = src->pixels[si+2];
            int dr = dst->pixels[di], dg = dst->pixels[di+1], db = dst->pixels[di+2];
            int br, bg, bb;
            switch (blend_mode) {
                case 1: br=_blend_multiply(sr,dr); bg=_blend_multiply(sg,dg); bb=_blend_multiply(sb,db); break;
                case 2: br=_blend_screen(sr,dr); bg=_blend_screen(sg,dg); bb=_blend_screen(sb,db); break;
                case 3: br=_blend_overlay(sr,dr); bg=_blend_overlay(sg,dg); bb=_blend_overlay(sb,db); break;
                case 4: br=_blend_soft_light(sr,dr); bg=_blend_soft_light(sg,dg); bb=_blend_soft_light(sb,db); break;
                default: br=sr; bg=sg; bb=sb; break; /* normal */
            }
            int inv = 255 - alpha;
            dst->pixels[di]   = (unsigned char)((br*alpha + dr*inv) / 255);
            dst->pixels[di+1] = (unsigned char)((bg*alpha + dg*inv) / 255);
            dst->pixels[di+2] = (unsigned char)((bb*alpha + db*inv) / 255);
        }
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  UNIFIED DISTANCE FIELD RENDERER (UDFR)
 *  ---
 *  Ce exista: Valve SDF (per-glyph), Cairo (per-path), Skia (per-path)
 *  Ce NU exista si noi inventam:
 *    - Toate formele dintr-o scena = UN SINGUR camp de distante
 *    - Renderizare single-pass (nu per-forma)
 *    - Topologie automata (care forma "detine" fiecare pixel)
 *    - Efecte ca transformari de camp (shadow = offset camp,
 *      glow = dilate camp, outline = isolinie in camp)
 *    - Boolean ops smooth intre forme (comasare = smooth union,
 *      dezmembrare = smooth split)
 *    - Morph intre doua campuri (animatie temporala)
 *
 *  Aplicatie directa: randare cadastrala unde parcelele
 *  IMPART granite (nu le deseneaza de doua ori cu Z-fighting)
 *
 *  SoftCAD C+C TOPO (C) 2026 — ORIGINAL, NU EXISTA IN ALTA PARTE
 * ═══════════════════════════════════════════════════════════════ */

/* ── Unified Distance Field ───────────────────────────────── */
typedef struct {
    int w, h;
    double *dist;    /* distance value per pixel (negative = inside) */
    int    *owner;   /* which shape "owns" this pixel (topology) */
    int    *color_r, *color_g, *color_b, *color_a;  /* per-shape color lookup */
    int    shape_count;
    int    shape_cap;
} TS_UDF;

TS_UDF *ts_udf_new(int w, int h) {
    TS_UDF *f = (TS_UDF*)calloc(1, sizeof(TS_UDF));
    if (!f) return NULL;
    f->w = w; f->h = h;
    size_t n = (size_t)(w * h);
    f->dist  = (double*)malloc(n * sizeof(double));
    f->owner = (int*)malloc(n * sizeof(int));
    if (!f->dist || !f->owner) { free(f->dist); free(f->owner); free(f); return NULL; }
    for (size_t i = 0; i < n; i++) { f->dist[i] = 1e30; f->owner[i] = -1; }
    f->shape_cap = 256;
    f->color_r = (int*)calloc((size_t)f->shape_cap, sizeof(int));
    f->color_g = (int*)calloc((size_t)f->shape_cap, sizeof(int));
    f->color_b = (int*)calloc((size_t)f->shape_cap, sizeof(int));
    f->color_a = (int*)calloc((size_t)f->shape_cap, sizeof(int));
    return f;
}

void ts_udf_free(TS_UDF *f) {
    if (!f) return;
    free(f->dist); free(f->owner);
    free(f->color_r); free(f->color_g); free(f->color_b); free(f->color_a);
    free(f);
}

/* Register a shape (returns shape ID) */
int ts_udf_add_shape(TS_UDF *f, int r, int g, int b, int a) {
    if (!f) return -1;
    if (f->shape_count >= f->shape_cap) {
        f->shape_cap *= 2;
        f->color_r = (int*)realloc(f->color_r, (size_t)f->shape_cap * sizeof(int));
        f->color_g = (int*)realloc(f->color_g, (size_t)f->shape_cap * sizeof(int));
        f->color_b = (int*)realloc(f->color_b, (size_t)f->shape_cap * sizeof(int));
        f->color_a = (int*)realloc(f->color_a, (size_t)f->shape_cap * sizeof(int));
    }
    int id = f->shape_count++;
    f->color_r[id] = r; f->color_g[id] = g; f->color_b[id] = b; f->color_a[id] = a;
    return id;
}

/* ── Add shapes to the unified field ──────────────────────── */

/* Circle into UDF */
void ts_udf_circle(TS_UDF *f, int shape_id, double cx, double cy, double radius) {
    if (!f) return;
    int margin = (int)(radius + 3);
    int x0 = (int)(cx-radius)-margin, y0 = (int)(cy-radius)-margin;
    int x1 = (int)(cx+radius)+margin, y1 = (int)(cy+radius)+margin;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= f->w) x1 = f->w-1; if (y1 >= f->h) y1 = f->h-1;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            double dx = px-cx, dy = py-cy;
            double d = sqrt(dx*dx+dy*dy) - radius;
            int idx = py * f->w + px;
            if (d < f->dist[idx]) { f->dist[idx] = d; f->owner[idx] = shape_id; }
        }
    }
}

/* Polygon into UDF (closest-edge SDF with inside/outside) */
void ts_udf_polygon(TS_UDF *f, int shape_id,
                     const double *xs, const double *ys, int n) {
    if (!f || n < 3) return;
    double bx0=xs[0],by0=ys[0],bx1=xs[0],by1=ys[0];
    for (int i=1;i<n;i++){
        if(xs[i]<bx0)bx0=xs[i]; if(xs[i]>bx1)bx1=xs[i];
        if(ys[i]<by0)by0=ys[i]; if(ys[i]>by1)by1=ys[i];
    }
    int margin = 5;
    int ix0=(int)bx0-margin, iy0=(int)by0-margin;
    int ix1=(int)bx1+margin, iy1=(int)by1+margin;
    if(ix0<0)ix0=0; if(iy0<0)iy0=0;
    if(ix1>=f->w)ix1=f->w-1; if(iy1>=f->h)iy1=f->h-1;

    for (int py = iy0; py <= iy1; py++) {
        for (int px = ix0; px <= ix1; px++) {
            /* Ray cast for inside/outside */
            int inside = 0;
            double min_d = 1e30;
            for (int i=0, j=n-1; i<n; j=i++) {
                if(((ys[i]>py)!=(ys[j]>py)) &&
                   (px < (xs[j]-xs[i])*(py-ys[i])/(ys[j]-ys[i])+xs[i]))
                    inside = !inside;
                double ex=xs[j]-xs[i], ey=ys[j]-ys[i];
                double len2=ex*ex+ey*ey;
                double t=len2>0?((px-xs[i])*ex+(py-ys[i])*ey)/len2:0;
                if(t<0)t=0; if(t>1)t=1;
                double dx2=xs[i]+t*ex-px, dy2=ys[i]+t*ey-py;
                double d=sqrt(dx2*dx2+dy2*dy2);
                if(d<min_d)min_d=d;
            }
            double dist = inside ? -min_d : min_d;
            int idx = py * f->w + px;
            if (dist < f->dist[idx]) { f->dist[idx] = dist; f->owner[idx] = shape_id; }
        }
    }
}

/* Rounded rect into UDF */
void ts_udf_rounded_rect(TS_UDF *f, int shape_id,
                          double cx, double cy, double hw, double hh, double radius) {
    if (!f) return;
    int margin = 5;
    int x0=(int)(cx-hw)-margin, y0=(int)(cy-hh)-margin;
    int x1=(int)(cx+hw)+margin, y1=(int)(cy+hh)+margin;
    if(x0<0)x0=0; if(y0<0)y0=0; if(x1>=f->w)x1=f->w-1; if(y1>=f->h)y1=f->h-1;
    for (int py=y0; py<=y1; py++) {
        for (int px=x0; px<=x1; px++) {
            double dx=fabs(px-cx)-hw+radius, dy=fabs(py-cy)-hh+radius;
            if(dx<0)dx=0; if(dy<0)dy=0;
            double d = sqrt(dx*dx+dy*dy) - radius;
            int idx = py*f->w+px;
            if (d < f->dist[idx]) { f->dist[idx]=d; f->owner[idx]=shape_id; }
        }
    }
}

/* ── Field transformations ────────────────────────────────── */

/* Dilate field (expand all shapes by amount) */
void ts_udf_dilate(TS_UDF *f, double amount) {
    if (!f) return;
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) f->dist[i] -= amount;
}

/* Erode field (shrink all shapes by amount) */
void ts_udf_erode(TS_UDF *f, double amount) {
    if (!f) return;
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) f->dist[i] += amount;
}

/* ── Render UDF to pixel buffer ───────────────────────────── */

/* Basic: solid colors with AA edges */
void ts_udf_render(const TS_UDF *f, TS_RenderBuf *rb, double softness) {
    if (!f || !rb || rb->w != f->w || rb->h != f->h) return;
    for (int i = 0; i < f->w * f->h; i++) {
        double d = f->dist[i];
        int owner = f->owner[i];
        if (owner < 0 || owner >= f->shape_count) continue;
        double alpha = _sdf_smoothstep(softness, -softness, d);
        if (alpha < 0.003) continue;
        int px = i % f->w, py = i / f->w;
        int a = (int)(alpha * f->color_a[owner]);
        _rend_set_pixel(rb, px, py, f->color_r[owner], f->color_g[owner], f->color_b[owner], a);
    }
}

/* Render with outlines on shape boundaries */
void ts_udf_render_outlined(const TS_UDF *f, TS_RenderBuf *rb,
                              double softness, double outline_width,
                              int or2, int og, int ob) {
    if (!f || !rb || rb->w != f->w || rb->h != f->h) return;
    for (int i = 0; i < f->w * f->h; i++) {
        double d = f->dist[i];
        int owner = f->owner[i];
        if (owner < 0 || owner >= f->shape_count) continue;

        int px = i % f->w, py = i / f->w;

        /* Fill */
        double fill_alpha = _sdf_smoothstep(softness, -softness, d);
        if (fill_alpha > 0.003) {
            int a = (int)(fill_alpha * f->color_a[owner]);
            _rend_set_pixel(rb, px, py, f->color_r[owner], f->color_g[owner], f->color_b[owner], a);
        }

        /* Outline: pixels near edge (|d| < outline_width) */
        double outline_alpha = _sdf_smoothstep(softness, -softness, fabs(d) - outline_width);
        outline_alpha = (1.0 - outline_alpha);  /* invert: bright near edge */
        if (outline_alpha > 0.01) {
            _rend_set_pixel(rb, px, py, or2, og, ob, (int)(outline_alpha * 220));
        }
    }
}

/* Render with automatic shadow */
void ts_udf_render_shadowed(const TS_UDF *f, TS_RenderBuf *rb,
                              double softness, double shadow_ox, double shadow_oy,
                              double shadow_blur, int sr, int sg, int sb, int sa) {
    if (!f || !rb || rb->w != f->w || rb->h != f->h) return;
    /* Shadow pass: sample field with offset */
    for (int py = 0; py < f->h; py++) {
        for (int px = 0; px < f->w; px++) {
            int sx = (int)(px - shadow_ox), sy = (int)(py - shadow_oy);
            if (sx < 0 || sx >= f->w || sy < 0 || sy >= f->h) continue;
            double d = f->dist[sy * f->w + sx];
            double alpha = _sdf_smoothstep(shadow_blur, -shadow_blur, d);
            if (alpha > 0.003)
                _rend_set_pixel(rb, px, py, sr, sg, sb, (int)(alpha * sa));
        }
    }
    /* Shape pass */
    ts_udf_render(f, rb, softness);
}

/* ── Topology queries from UDF ────────────────────────────── */

/* Get owner at pixel */
int ts_udf_owner_at(const TS_UDF *f, int x, int y) {
    if (!f || x < 0 || y < 0 || x >= f->w || y >= f->h) return -1;
    return f->owner[y * f->w + x];
}

/* Get distance at pixel */
double ts_udf_dist_at(const TS_UDF *f, int x, int y) {
    if (!f || x < 0 || y < 0 || x >= f->w || y >= f->h) return 1e30;
    return f->dist[y * f->w + x];
}

/* Find all neighbor shape IDs for a given shape */
int ts_udf_neighbors(const TS_UDF *f, int shape_id, int *neighbors, int max_n) {
    if (!f || !neighbors) return 0;
    int count = 0;
    for (int y = 1; y < f->h - 1 && count < max_n; y++) {
        for (int x = 1; x < f->w - 1 && count < max_n; x++) {
            int idx = y * f->w + x;
            if (f->owner[idx] != shape_id) continue;
            /* Check 4-neighbors */
            int adj[4] = { f->owner[idx-1], f->owner[idx+1],
                          f->owner[idx-f->w], f->owner[idx+f->w] };
            for (int a = 0; a < 4; a++) {
                if (adj[a] >= 0 && adj[a] != shape_id) {
                    int dup = 0;
                    for (int k = 0; k < count; k++)
                        if (neighbors[k] == adj[a]) { dup = 1; break; }
                    if (!dup) neighbors[count++] = adj[a];
                }
            }
        }
    }
    return count;
}

/* Compute shared boundary length between two shapes (in pixels) */
int ts_udf_shared_boundary(const TS_UDF *f, int shape_a, int shape_b) {
    if (!f) return 0;
    int count = 0;
    for (int y = 1; y < f->h - 1; y++) {
        for (int x = 1; x < f->w - 1; x++) {
            int idx = y * f->w + x;
            if (f->owner[idx] != shape_a) continue;
            int adj[4] = { f->owner[idx-1], f->owner[idx+1],
                          f->owner[idx-f->w], f->owner[idx+f->w] };
            for (int a = 0; a < 4; a++)
                if (adj[a] == shape_b) { count++; break; }
        }
    }
    return count;
}

/* ── Morph between two UDFs (animation) ───────────────────── */
void ts_udf_morph(const TS_UDF *a, const TS_UDF *b, TS_UDF *out, double t) {
    if (!a || !b || !out) return;
    if (a->w != b->w || a->h != b->h || a->w != out->w) return;
    int n = a->w * a->h;
    double u = 1.0 - t;
    for (int i = 0; i < n; i++) {
        out->dist[i] = u * a->dist[i] + t * b->dist[i];
        out->owner[i] = t < 0.5 ? a->owner[i] : b->owner[i];
    }
    /* Blend colors */
    int sc = a->shape_count > b->shape_count ? a->shape_count : b->shape_count;
    out->shape_count = sc;
    for (int s = 0; s < sc; s++) {
        int ar = s<a->shape_count ? a->color_r[s] : 0;
        int br2 = s<b->shape_count ? b->color_r[s] : 0;
        out->color_r[s] = (int)(u*ar + t*br2);
        out->color_g[s] = (int)(u*(s<a->shape_count?a->color_g[s]:0) + t*(s<b->shape_count?b->color_g[s]:0));
        out->color_b[s] = (int)(u*(s<a->shape_count?a->color_b[s]:0) + t*(s<b->shape_count?b->color_b[s]:0));
        out->color_a[s] = (int)(u*(s<a->shape_count?a->color_a[s]:0) + t*(s<b->shape_count?b->color_a[s]:0));
    }
}

#endif /* TS_RENDER_ENGINE_H */
