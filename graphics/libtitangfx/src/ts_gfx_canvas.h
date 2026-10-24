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
 * ts_gfx_canvas.h — RGBA Framebuffer Canvas
 * ============================================
 * Single-header C99. Zero dependencies.
 *
 * #define TS_GFX_CANVAS_IMPLEMENTATION in ONE .c file before #include.
 *
 * SoftCAD C+C TOPO © 2026 — Titan Sentinel Script GFX Engine
 */
#ifndef TS_GFX_CANVAS_H
#define TS_GFX_CANVAS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── Types ─────────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t *pixels;       /* RGBA8888 framebuffer (row-major, top-left origin) */
    int       width;
    int       height;
    /* Current style state */
    uint32_t  stroke_color; /* RGBA packed */
    uint32_t  fill_color;
    float     line_width;
    float     opacity;      /* 0.0–1.0 global */
    /* Transform (set by ts_gfx_transform) */
    float     tf[9];        /* 3x3 affine matrix (row-major) */
    int       tf_active;    /* 1 if transform is set */
    /* v3.0: clip region */
    int clip_x, clip_y, clip_w, clip_h;
    int clip_enabled;
} TS_Canvas;

/* ── Color helpers ─────────────────────────────────────────────────────── */

static inline uint32_t ts_rgba(int r, int g, int b, int a) {
    return ((uint32_t)(a & 0xFF) << 24) |
           ((uint32_t)(b & 0xFF) << 16) |
           ((uint32_t)(g & 0xFF) << 8)  |
           ((uint32_t)(r & 0xFF));
}
static inline uint32_t ts_rgb(int r, int g, int b) {
    return ts_rgba(r, g, b, 255);
}
static inline int ts_r(uint32_t c) { return (int)(c & 0xFF); }
static inline int ts_g(uint32_t c) { return (int)((c >> 8) & 0xFF); }
static inline int ts_b(uint32_t c) { return (int)((c >> 16) & 0xFF); }
static inline int ts_a(uint32_t c) { return (int)((c >> 24) & 0xFF); }

static inline uint32_t ts_color_hex(const char *hex) {
    /* Parse "#RRGGBB" or "RRGGBB" */
    if (hex[0] == '#') hex++;
    unsigned int v = 0;
    for (int i = 0; i < 6; i++) {
        char c = hex[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
    }
    return ts_rgb((int)((v >> 16) & 0xFF), (int)((v >> 8) & 0xFF), (int)(v & 0xFF));
}

/* ── Alpha blending ────────────────────────────────────────────────────── */

static inline uint32_t ts_blend(uint32_t dst, uint32_t src) {
    int sa = ts_a(src);
    if (sa == 255) return src;
    if (sa == 0)   return dst;
    int inv = 255 - sa;
    int r = (ts_r(src) * sa + ts_r(dst) * inv) / 255;
    int g = (ts_g(src) * sa + ts_g(dst) * inv) / 255;
    int b = (ts_b(src) * sa + ts_b(dst) * inv) / 255;
    int a = sa + (ts_a(dst) * inv) / 255;
    return ts_rgba(r, g, b, a);
}

/* ── API declarations ──────────────────────────────────────────────────── */

TS_Canvas *ts_canvas_new(int width, int height);
void       ts_canvas_free(TS_Canvas *c);
void       ts_canvas_clear(TS_Canvas *c, uint32_t color);
void       ts_canvas_resize(TS_Canvas *c, int w, int h);
int        ts_canvas_width(const TS_Canvas *c);
int        ts_canvas_height(const TS_Canvas *c);

/* Pixel access */
void       ts_pixel_set(TS_Canvas *c, int x, int y, uint32_t color);
void       ts_pixel_blend(TS_Canvas *c, int x, int y, uint32_t color);
uint32_t   ts_pixel_get(const TS_Canvas *c, int x, int y);

/* Sub-canvas (blit) */
void       ts_canvas_blit(TS_Canvas *dst, int dx, int dy,
                           const TS_Canvas *src, int sx, int sy, int sw, int sh);

/* ── Implementation ────────────────────────────────────────────────────── */
#ifdef TS_GFX_CANVAS_IMPLEMENTATION

TS_Canvas *ts_canvas_new(int width, int height) {
    TS_Canvas *c = (TS_Canvas *)calloc(1, sizeof(TS_Canvas));
    if (!c) return NULL;
    c->width  = width;
    c->height = height;
    c->pixels = (uint32_t *)calloc((size_t)(width * height), sizeof(uint32_t));
    if (!c->pixels) { free(c); return NULL; }
    c->stroke_color = ts_rgb(0, 0, 0);
    c->fill_color   = ts_rgb(255, 255, 255);
    c->line_width   = 1.0f;
    c->opacity      = 1.0f;
    c->tf_active    = 0;
    /* Identity matrix */
    c->tf[0] = 1; c->tf[1] = 0; c->tf[2] = 0;
    c->tf[3] = 0; c->tf[4] = 1; c->tf[5] = 0;
    c->tf[6] = 0; c->tf[7] = 0; c->tf[8] = 1;
    return c;
}

void ts_canvas_free(TS_Canvas *c) {
    if (c) { free(c->pixels); free(c); }
}

void ts_canvas_clear(TS_Canvas *c, uint32_t color) {
    int n = c->width * c->height;
    for (int i = 0; i < n; i++) c->pixels[i] = color;
}

void ts_canvas_resize(TS_Canvas *c, int w, int h) {
    uint32_t *new_px = (uint32_t *)calloc((size_t)(w * h), sizeof(uint32_t));
    if (!new_px) return;
    /* Copy overlapping region */
    int copy_w = (w < c->width)  ? w : c->width;
    int copy_h = (h < c->height) ? h : c->height;
    for (int y = 0; y < copy_h; y++)
        memcpy(new_px + y * w, c->pixels + y * c->width, (size_t)copy_w * sizeof(uint32_t));
    free(c->pixels);
    c->pixels = new_px;
    c->width  = w;
    c->height = h;
}

int ts_canvas_width(const TS_Canvas *c)  { return c->width; }
int ts_canvas_height(const TS_Canvas *c) { return c->height; }

void ts_pixel_set(TS_Canvas *c, int x, int y, uint32_t color) {
    if (x >= 0 && x < c->width && y >= 0 && y < c->height)
        c->pixels[y * c->width + x] = color;
}

void ts_pixel_blend(TS_Canvas *c, int x, int y, uint32_t color) {
    if (x >= 0 && x < c->width && y >= 0 && y < c->height) {
        int idx = y * c->width + x;
        c->pixels[idx] = ts_blend(c->pixels[idx], color);
    }
}

uint32_t ts_pixel_get(const TS_Canvas *c, int x, int y) {
    if (x >= 0 && x < c->width && y >= 0 && y < c->height)
        return c->pixels[y * c->width + x];
    return 0;
}

void ts_canvas_blit(TS_Canvas *dst, int dx, int dy,
                     const TS_Canvas *src, int sx, int sy, int sw, int sh) {
    for (int y = 0; y < sh; y++) {
        int src_y = sy + y, dst_y = dy + y;
        if (src_y < 0 || src_y >= src->height || dst_y < 0 || dst_y >= dst->height) continue;
        for (int x = 0; x < sw; x++) {
            int src_x = sx + x, dst_x = dx + x;
            if (src_x < 0 || src_x >= src->width || dst_x < 0 || dst_x >= dst->width) continue;
            uint32_t sc = src->pixels[src_y * src->width + src_x];
            int didx = dst_y * dst->width + dst_x;
            dst->pixels[didx] = ts_blend(dst->pixels[didx], sc);
        }
    }
}

#endif /* TS_GFX_CANVAS_IMPLEMENTATION */
#endif /* TS_GFX_CANVAS_H */
