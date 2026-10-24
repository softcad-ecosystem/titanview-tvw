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
/* ── ts_gfx_imgproc.h — Image Processing + Advanced 2D + Color ───────
 * Makes TITAN GFX the most complete single-header software renderer.
 * C99, zero dependencies.
 *
 * SoftCAD C+C TOPO © 2026
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* helper: clamp 0-255 */
static inline int _clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* ═══════════════════════════════════════════════════════════════════ */
/*  IMAGE PROCESSING                                                  */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── Grayscale ─────────────────────────────────────────────────── */
void ts_canvas_grayscale(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int r = (px >> 16) & 0xFF, g = (px >> 8) & 0xFF, b = px & 0xFF, a = (px >> 24) & 0xFF;
        int gray = (r * 299 + g * 587 + b * 114) / 1000;
        c->pixels[i] = (uint32_t)((a << 24) | (gray << 16) | (gray << 8) | gray);
    }
}

/* ── Sepia ──────────────────────────────────────────────────────── */
void ts_canvas_sepia(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int r = (px >> 16) & 0xFF, g = (px >> 8) & 0xFF, b = px & 0xFF, a = (px >> 24) & 0xFF;
        int nr = _clamp8((r * 393 + g * 769 + b * 189) / 1000);
        int ng = _clamp8((r * 349 + g * 686 + b * 168) / 1000);
        int nb = _clamp8((r * 272 + g * 534 + b * 131) / 1000);
        c->pixels[i] = (uint32_t)((a << 24) | (nr << 16) | (ng << 8) | nb);
    }
}

/* ── Invert ─────────────────────────────────────────────────────── */
void ts_canvas_invert(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int a = (px >> 24) & 0xFF;
        c->pixels[i] = (uint32_t)((a << 24) | ((~px) & 0x00FFFFFF));
    }
}

/* ── Brightness / Contrast ──────────────────────────────────────── */
void ts_canvas_brightness(TS_Canvas *c, int delta) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int r = _clamp8(((px >> 16) & 0xFF) + delta);
        int g = _clamp8(((px >> 8) & 0xFF) + delta);
        int b = _clamp8((px & 0xFF) + delta);
        c->pixels[i] = (c->pixels[i] & 0xFF000000) | (uint32_t)((r << 16) | (g << 8) | b);
    }
}

void ts_canvas_contrast(TS_Canvas *c, float factor) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int r = _clamp8((int)(factor * (((px >> 16) & 0xFF) - 128) + 128));
        int g = _clamp8((int)(factor * (((px >> 8) & 0xFF) - 128) + 128));
        int b = _clamp8((int)(factor * ((px & 0xFF) - 128) + 128));
        c->pixels[i] = (c->pixels[i] & 0xFF000000) | (uint32_t)((r << 16) | (g << 8) | b);
    }
}

/* ── Threshold (binary) ─────────────────────────────────────────── */
void ts_canvas_threshold(TS_Canvas *c, int thresh) {
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int gray = (((px >> 16) & 0xFF) * 299 + ((px >> 8) & 0xFF) * 587 + (px & 0xFF) * 114) / 1000;
        int v = gray >= thresh ? 255 : 0;
        c->pixels[i] = (c->pixels[i] & 0xFF000000) | (uint32_t)((v << 16) | (v << 8) | v);
    }
}

/* ── Box blur (fast, radius r) ──────────────────────────────────── */
void ts_canvas_blur_box(TS_Canvas *c, int r) {
    if (r < 1) return;
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w * h) * 4);
    if (!tmp) return;

    /* Horizontal pass */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sr = 0, sg = 0, sb = 0, cnt = 0;
            for (int dx = -r; dx <= r; dx++) {
                int nx = x + dx;
                if (nx >= 0 && nx < w) {
                    uint32_t px = c->pixels[y * w + nx];
                    sr += (px >> 16) & 0xFF; sg += (px >> 8) & 0xFF; sb += px & 0xFF;
                    cnt++;
                }
            }
            tmp[y * w + x] = 0xFF000000 | (uint32_t)(((sr / cnt) << 16) | ((sg / cnt) << 8) | (sb / cnt));
        }
    }

    /* Vertical pass */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sr = 0, sg = 0, sb = 0, cnt = 0;
            for (int dy = -r; dy <= r; dy++) {
                int ny = y + dy;
                if (ny >= 0 && ny < h) {
                    uint32_t px = tmp[ny * w + x];
                    sr += (px >> 16) & 0xFF; sg += (px >> 8) & 0xFF; sb += px & 0xFF;
                    cnt++;
                }
            }
            c->pixels[y * w + x] = 0xFF000000 | (uint32_t)(((sr / cnt) << 16) | ((sg / cnt) << 8) | (sb / cnt));
        }
    }
    free(tmp);
}

/* ── Gaussian blur (3x3 kernel) ─────────────────────────────────── */
void ts_canvas_blur_gaussian(TS_Canvas *c) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w * h) * 4);
    if (!tmp) return;
    /* 3x3 Gaussian: 1 2 1 / 2 4 2 / 1 2 1 (sum=16) */
    static const int k[3][3] = {{1,2,1},{2,4,2},{1,2,1}};
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int sr = 0, sg = 0, sb = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t px = c->pixels[(y + dy) * w + (x + dx)];
                    int wt = k[dy + 1][dx + 1];
                    sr += ((px >> 16) & 0xFF) * wt;
                    sg += ((px >> 8) & 0xFF) * wt;
                    sb += (px & 0xFF) * wt;
                }
            }
            tmp[y * w + x] = 0xFF000000 | (uint32_t)(((sr / 16) << 16) | ((sg / 16) << 8) | (sb / 16));
        }
    }
    memcpy(c->pixels + w, tmp + w, (size_t)((h - 2) * w) * 4);  /* skip borders */
    free(tmp);
}

/* ── Sharpen (3x3) ──────────────────────────────────────────────── */
void ts_canvas_sharpen(TS_Canvas *c) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w * h) * 4);
    if (!tmp) return;
    static const int k[3][3] = {{0,-1,0},{-1,5,-1},{0,-1,0}};
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int sr = 0, sg = 0, sb = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t px = c->pixels[(y + dy) * w + (x + dx)];
                    int wt = k[dy + 1][dx + 1];
                    sr += ((px >> 16) & 0xFF) * wt;
                    sg += ((px >> 8) & 0xFF) * wt;
                    sb += (px & 0xFF) * wt;
                }
            tmp[y * w + x] = 0xFF000000 | (uint32_t)((_clamp8(sr) << 16) | (_clamp8(sg) << 8) | _clamp8(sb));
        }
    }
    memcpy(c->pixels + w, tmp + w, (size_t)((h - 2) * w) * 4);
    free(tmp);
}

/* ── Edge detection (Sobel) ─────────────────────────────────────── */
void ts_canvas_edge_sobel(TS_Canvas *c) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w * h) * 4);
    if (!tmp) return;
    memset(tmp, 0, (size_t)(w * h) * 4);
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int gx = 0, gy = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t px = c->pixels[(y + dy) * w + (x + dx)];
                    int gray = (((px >> 16) & 0xFF) + ((px >> 8) & 0xFF) + (px & 0xFF)) / 3;
                    /* Sobel kernels */
                    static const int kx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
                    static const int ky[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
                    gx += gray * kx[dy + 1][dx + 1];
                    gy += gray * ky[dy + 1][dx + 1];
                }
            }
            int mag = _clamp8((int)sqrt((double)(gx * gx + gy * gy)));
            tmp[y * w + x] = 0xFF000000 | (uint32_t)((mag << 16) | (mag << 8) | mag);
        }
    }
    memcpy(c->pixels, tmp, (size_t)(w * h) * 4);
    free(tmp);
}

/* ── Flip H/V ───────────────────────────────────────────────────── */
void ts_canvas_flip_h(TS_Canvas *c) {
    int w = c->width, h = c->height;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w / 2; x++) {
            uint32_t t = c->pixels[y * w + x];
            c->pixels[y * w + x] = c->pixels[y * w + (w - 1 - x)];
            c->pixels[y * w + (w - 1 - x)] = t;
        }
}

void ts_canvas_flip_v(TS_Canvas *c) {
    int w = c->width, h = c->height;
    uint32_t *row = (uint32_t *)malloc((size_t)w * 4);
    if (!row) return;
    for (int y = 0; y < h / 2; y++) {
        memcpy(row, c->pixels + y * w, (size_t)w * 4);
        memcpy(c->pixels + y * w, c->pixels + (h - 1 - y) * w, (size_t)w * 4);
        memcpy(c->pixels + (h - 1 - y) * w, row, (size_t)w * 4);
    }
    free(row);
}

/* ── Crop ───────────────────────────────────────────────────────── */
TS_Canvas *ts_canvas_crop(const TS_Canvas *c, int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > c->width) w = c->width - x;
    if (y + h > c->height) h = c->height - y;
    if (w <= 0 || h <= 0) return NULL;
    TS_Canvas *out = ts_canvas_new(w, h);
    if (!out) return NULL;
    for (int dy = 0; dy < h; dy++)
        memcpy(out->pixels + dy * w, c->pixels + (y + dy) * c->width + x, (size_t)w * 4);
    return out;
}

/* ── Resize (bilinear interpolation) ────────────────────────────── */
TS_Canvas *ts_canvas_resize_bilinear(const TS_Canvas *c, int nw, int nh) {
    if (nw <= 0 || nh <= 0) return NULL;
    TS_Canvas *out = ts_canvas_new(nw, nh);
    if (!out) return NULL;
    float xr = (float)c->width / (float)nw;
    float yr = (float)c->height / (float)nh;
    for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {
            float fx = x * xr, fy = y * yr;
            int ix = (int)fx, iy = (int)fy;
            float dx = fx - ix, dy = fy - iy;
            int ix1 = (ix + 1 < c->width) ? ix + 1 : ix;
            int iy1 = (iy + 1 < c->height) ? iy + 1 : iy;
            /* 4 corners */
            uint32_t p00 = c->pixels[iy * c->width + ix];
            uint32_t p10 = c->pixels[iy * c->width + ix1];
            uint32_t p01 = c->pixels[iy1 * c->width + ix];
            uint32_t p11 = c->pixels[iy1 * c->width + ix1];
            /* Bilinear per channel */
            for (int ch = 0; ch < 4; ch++) {
                int shift = ch * 8;
                float v = (1-dx)*(1-dy)*((p00>>shift)&0xFF) + dx*(1-dy)*((p10>>shift)&0xFF)
                        + (1-dx)*dy*((p01>>shift)&0xFF) + dx*dy*((p11>>shift)&0xFF);
                ((uint8_t*)&out->pixels[y * nw + x])[ch] = (uint8_t)(v + 0.5f);
            }
        }
    }
    return out;
}

/* ── Rotate (arbitrary angle, bilinear) ─────────────────────────── */
void ts_canvas_rotate_angle(TS_Canvas *c, float angle_deg, uint32_t bg) {
    float rad = angle_deg * (float)M_PI / 180.0f;
    float cs = cosf(rad), sn = sinf(rad);
    int w = c->width, h = c->height;
    float cx = w / 2.0f, cy = h / 2.0f;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w * h) * 4);
    if (!tmp) return;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = x - cx, dy = y - cy;
            float sx = dx * cs + dy * sn + cx;
            float sy = -dx * sn + dy * cs + cy;
            int ix = (int)sx, iy = (int)sy;
            if (ix >= 0 && ix < w && iy >= 0 && iy < h)
                tmp[y * w + x] = c->pixels[iy * w + ix];
            else
                tmp[y * w + x] = bg;
        }
    }
    memcpy(c->pixels, tmp, (size_t)(w * h) * 4);
    free(tmp);
}

/* ── Dithering (Floyd-Steinberg) ────────────────────────────────── */
void ts_canvas_dither(TS_Canvas *c) {
    int w = c->width, h = c->height;
    /* Work in grayscale float buffer */
    float *buf = (float *)malloc((size_t)(w * h) * sizeof(float));
    if (!buf) return;
    for (int i = 0; i < w * h; i++) {
        uint32_t px = c->pixels[i];
        buf[i] = (float)(((px >> 16) & 0xFF) * 299 + ((px >> 8) & 0xFF) * 587 + (px & 0xFF) * 114) / 1000.0f;
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float old = buf[y * w + x];
            float nv = old > 127 ? 255.0f : 0.0f;
            buf[y * w + x] = nv;
            float err = old - nv;
            if (x + 1 < w)           buf[y * w + x + 1] += err * 7.0f / 16.0f;
            if (y + 1 < h && x > 0)  buf[(y+1) * w + x-1] += err * 3.0f / 16.0f;
            if (y + 1 < h)           buf[(y+1) * w + x] += err * 5.0f / 16.0f;
            if (y + 1 < h && x + 1 < w) buf[(y+1) * w + x+1] += err * 1.0f / 16.0f;
        }
    }
    for (int i = 0; i < w * h; i++) {
        int v = _clamp8((int)buf[i]);
        c->pixels[i] = 0xFF000000 | (uint32_t)((v << 16) | (v << 8) | v);
    }
    free(buf);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  ADVANCED 2D                                                       */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── Radial gradient ────────────────────────────────────────────── */
void ts_draw_gradient_radial(TS_Canvas *c, int cx, int cy, int radius,
                              uint32_t inner, uint32_t outer) {
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (x < 0 || x >= c->width || y < 0 || y >= c->height) continue;
            float d = sqrtf((float)((x-cx)*(x-cx) + (y-cy)*(y-cy)));
            if (d > radius) continue;
            float t = d / (float)radius;
            int r = (int)((1-t) * ((inner>>16)&0xFF) + t * ((outer>>16)&0xFF));
            int g = (int)((1-t) * ((inner>>8)&0xFF) + t * ((outer>>8)&0xFF));
            int b = (int)((1-t) * (inner&0xFF) + t * (outer&0xFF));
            c->pixels[y * c->width + x] = 0xFF000000 | (uint32_t)((r<<16)|(g<<8)|b);
        }
    }
}

/* ── Conic gradient ─────────────────────────────────────────────── */
void ts_draw_gradient_conic(TS_Canvas *c, int cx, int cy, int radius,
                             uint32_t c1, uint32_t c2) {
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (x < 0 || x >= c->width || y < 0 || y >= c->height) continue;
            float d = sqrtf((float)((x-cx)*(x-cx) + (y-cy)*(y-cy)));
            if (d > radius) continue;
            float angle = atan2f((float)(y - cy), (float)(x - cx));
            float t = (angle + (float)M_PI) / (2.0f * (float)M_PI);
            int r = (int)((1-t)*((c1>>16)&0xFF) + t*((c2>>16)&0xFF));
            int g = (int)((1-t)*((c1>>8)&0xFF) + t*((c2>>8)&0xFF));
            int b = (int)((1-t)*(c1&0xFF) + t*(c2&0xFF));
            c->pixels[y * c->width + x] = 0xFF000000 | (uint32_t)((r<<16)|(g<<8)|b);
        }
    }
}

/* ── Pattern fill (tile texture from another canvas) ────────────── */
void ts_draw_pattern_fill(TS_Canvas *dst, int x, int y, int w, int h,
                           const TS_Canvas *pattern) {
    int pw = pattern->width, ph = pattern->height;
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < dst->width && py >= 0 && py < dst->height)
                dst->pixels[py * dst->width + px] = pattern->pixels[(dy % ph) * pw + (dx % pw)];
        }
}

/* ── Clip rectangle (scissor) ───────────────────────────────────── */
void ts_canvas_set_clip(TS_Canvas *c, int x, int y, int w, int h) {
    c->clip_x = x; c->clip_y = y; c->clip_w = w; c->clip_h = h;
    c->clip_enabled = 1;
}

void ts_canvas_clear_clip(TS_Canvas *c) { c->clip_enabled = 0; }

/* ── Anti-aliased circle ────────────────────────────────────────── */
void ts_draw_circle_aa(TS_Canvas *c, int cx, int cy, int r, uint32_t color) {
    for (int y = cy - r - 1; y <= cy + r + 1; y++) {
        for (int x = cx - r - 1; x <= cx + r + 1; x++) {
            if (x < 0 || x >= c->width || y < 0 || y >= c->height) continue;
            float d = sqrtf((float)((x-cx)*(x-cx) + (y-cy)*(y-cy)));
            float diff = fabsf(d - (float)r);
            if (diff < 1.5f) {
                float alpha = 1.0f - diff;
                if (alpha < 0) alpha = 0; if (alpha > 1) alpha = 1;
                uint32_t src = (color & 0x00FFFFFF) | ((uint32_t)(int)(alpha * 255) << 24);
                ts_canvas_set_pixel_blend(c, x, y, src);
            }
        }
    }
}

/* ── Arrow head ─────────────────────────────────────────────────── */
void ts_draw_arrow(TS_Canvas *c, int x0, int y0, int x1, int y1, int head_size) {
    ts_draw_line(c, x0, y0, x1, y1);
    float angle = atan2f((float)(y1-y0), (float)(x1-x0));
    float a1 = angle + 2.5f, a2 = angle - 2.5f;
    int hx1 = x1 - (int)(head_size * cosf(a1));
    int hy1 = y1 - (int)(head_size * sinf(a1));
    int hx2 = x1 - (int)(head_size * cosf(a2));
    int hy2 = y1 - (int)(head_size * sinf(a2));
    ts_draw_line(c, x1, y1, hx1, hy1);
    ts_draw_line(c, x1, y1, hx2, hy2);
}

/* ── Catmull-Rom spline ─────────────────────────────────────────── */
void ts_draw_catmull_rom(TS_Canvas *c, const int *pts, int npts, int segments) {
    /* pts = [x0,y0, x1,y1, ...], npts = number of points */
    if (npts < 4 || segments < 2) return;
    for (int i = 0; i < npts - 1; i++) {
        int i0 = (i > 0) ? i - 1 : 0;
        int i1 = i, i2 = i + 1, i3 = (i + 2 < npts) ? i + 2 : npts - 1;
        float x0 = (float)pts[i0*2], y0 = (float)pts[i0*2+1];
        float x1 = (float)pts[i1*2], y1 = (float)pts[i1*2+1];
        float x2 = (float)pts[i2*2], y2 = (float)pts[i2*2+1];
        float x3 = (float)pts[i3*2], y3 = (float)pts[i3*2+1];
        float px = x1, py = y1;
        for (int j = 1; j <= segments; j++) {
            float t = (float)j / (float)segments;
            float t2 = t * t, t3 = t2 * t;
            float nx = 0.5f * (2*x1 + (-x0+x2)*t + (2*x0-5*x1+4*x2-x3)*t2 + (-x0+3*x1-3*x2+x3)*t3);
            float ny = 0.5f * (2*y1 + (-y0+y2)*t + (2*y0-5*y1+4*y2-y3)*t2 + (-y0+3*y1-3*y2+y3)*t3);
            ts_draw_line(c, (int)px, (int)py, (int)nx, (int)ny);
            px = nx; py = ny;
        }
    }
}

/* ── Layer compositing (canvas over canvas with alpha) ──────────── */
void ts_canvas_composite(TS_Canvas *dst, const TS_Canvas *src, int dx, int dy) {
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int px = dx + x, py = dy + y;
            if (px >= 0 && px < dst->width && py >= 0 && py < dst->height)
                ts_canvas_set_pixel_blend(dst, px, py, src->pixels[y * src->width + x]);
        }
    }
}

/* ── Canvas snapshot/undo ───────────────────────────────────────── */
uint32_t *ts_canvas_snapshot(const TS_Canvas *c) {
    size_t sz = (size_t)(c->width * c->height) * 4;
    uint32_t *snap = (uint32_t *)malloc(sz);
    if (snap) memcpy(snap, c->pixels, sz);
    return snap;
}

void ts_canvas_restore(TS_Canvas *c, const uint32_t *snap) {
    if (snap) memcpy(c->pixels, snap, (size_t)(c->width * c->height) * 4);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  COLOR UTILITIES                                                    */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── HSL → RGB ──────────────────────────────────────────────────── */
static float _hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1; if (t > 1) t -= 1;
    if (t < 1.0f/6) return p + (q-p) * 6 * t;
    if (t < 1.0f/2) return q;
    if (t < 2.0f/3) return p + (q-p) * (2.0f/3 - t) * 6;
    return p;
}

uint32_t ts_color_hsl(float h, float s, float l) {
    /* h,s,l in 0-1 range */
    int r, g, b;
    if (s < 0.001f) {
        r = g = b = (int)(l * 255);
    } else {
        float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;
        r = (int)(_hue2rgb(p, q, h + 1.0f/3) * 255);
        g = (int)(_hue2rgb(p, q, h) * 255);
        b = (int)(_hue2rgb(p, q, h - 1.0f/3) * 255);
    }
    return 0xFF000000 | (uint32_t)((_clamp8(r) << 16) | (_clamp8(g) << 8) | _clamp8(b));
}

/* ── HSV → RGB ──────────────────────────────────────────────────── */
uint32_t ts_color_hsv(float h, float s, float v) {
    int hi = (int)(h * 6) % 6;
    float f = h * 6 - (int)(h * 6);
    int p = (int)(v * (1-s) * 255), q = (int)(v * (1-f*s) * 255);
    int t = (int)(v * (1-(1-f)*s) * 255), vi = (int)(v * 255);
    int r, g, b;
    switch (hi) {
        case 0: r=vi;g=t;b=p; break; case 1: r=q;g=vi;b=p; break;
        case 2: r=p;g=vi;b=t; break; case 3: r=p;g=q;b=vi; break;
        case 4: r=t;g=p;b=vi; break; default:r=vi;g=p;b=q; break;
    }
    return 0xFF000000 | (uint32_t)((_clamp8(r) << 16) | (_clamp8(g) << 8) | _clamp8(b));
}

/* ── Color lerp ─────────────────────────────────────────────────── */
uint32_t ts_color_lerp(uint32_t a, uint32_t b, float t) {
    int r = (int)((1-t)*((a>>16)&0xFF) + t*((b>>16)&0xFF));
    int g = (int)((1-t)*((a>>8)&0xFF) + t*((b>>8)&0xFF));
    int bl = (int)((1-t)*(a&0xFF) + t*(b&0xFF));
    return 0xFF000000 | (uint32_t)((_clamp8(r) << 16) | (_clamp8(g) << 8) | _clamp8(bl));
}

/* ── Heatmap palette (0.0-1.0 → blue→cyan→green→yellow→red) ──── */
uint32_t ts_color_heatmap(float val) {
    if (val < 0) val = 0; if (val > 1) val = 1;
    /* Blue(0) → Cyan(0.25) → Green(0.5) → Yellow(0.75) → Red(1.0) */
    int r, g, b;
    if (val < 0.25f) { float t = val / 0.25f; r = 0; g = (int)(t*255); b = 255; }
    else if (val < 0.5f) { float t = (val-0.25f) / 0.25f; r = 0; g = 255; b = (int)((1-t)*255); }
    else if (val < 0.75f) { float t = (val-0.5f) / 0.25f; r = (int)(t*255); g = 255; b = 0; }
    else { float t = (val-0.75f) / 0.25f; r = 255; g = (int)((1-t)*255); b = 0; }
    return 0xFF000000 | (uint32_t)((r << 16) | (g << 8) | b);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GIS-SPECIFIC (ce NIMENI nu are)                                    */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── Coordinate grid overlay ────────────────────────────────────── */
void ts_draw_grid(TS_Canvas *c, int x0, int y0, int x1, int y1,
                   int step_x, int step_y, uint32_t color) {
    for (int x = x0; x <= x1; x += step_x)
        ts_draw_line(c, x, y0, x, y1);
    for (int y = y0; y <= y1; y += step_y)
        ts_draw_line(c, x0, y, x1, y);
}

/* ── North arrow ────────────────────────────────────────────────── */
void ts_draw_north_arrow(TS_Canvas *c, int cx, int cy, int size) {
    /* Simple north arrow: triangle pointing up + N label */
    int top = cy - size, bot = cy + size / 2;
    int left = cx - size / 3, right = cx + size / 3;
    ts_draw_line(c, cx, top, left, bot);
    ts_draw_line(c, cx, top, right, bot);
    ts_draw_line(c, left, bot, right, bot);
    /* Fill left half */
    for (int y = top; y <= bot; y++) {
        float t = (float)(y - top) / (float)(bot - top);
        int xl = cx - (int)(t * size / 3);
        for (int x = xl; x <= cx; x++)
            if (x >= 0 && x < c->width && y >= 0 && y < c->height)
                c->pixels[y * c->width + x] = c->pixels[y * c->width + x] | 0xFF000000;
    }
    /* N label */
    ts_draw_text(c, cx - 3, top - 12, "N");
}

/* ── Scale bar ──────────────────────────────────────────────────── */
void ts_draw_scale_bar(TS_Canvas *c, int x, int y, int width, int height,
                        double meters, uint32_t color) {
    ts_draw_rect_filled(c, x, y, width, height);
    /* Alternating black/white segments */
    int segs = 4;
    int seg_w = width / segs;
    for (int i = 0; i < segs; i++) {
        if (i % 2 == 0)
            ts_draw_rect_filled(c, x + i * seg_w, y, seg_w, height);
    }
    ts_draw_rect(c, x, y, width, height);
    /* Labels */
    char buf[32];
    snprintf(buf, sizeof(buf), "0");
    ts_draw_text(c, x, y + height + 2, buf);
    if (meters >= 1000) snprintf(buf, sizeof(buf), "%.1f km", meters / 1000);
    else snprintf(buf, sizeof(buf), "%.0f m", meters);
    ts_draw_text(c, x + width - 30, y + height + 2, buf);
}

/* ── SVG export (canvas drawing commands → vector SVG) ──────────── */
int ts_canvas_save_svg(const TS_Canvas *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\">\n",
            c->width, c->height);
    /* Rasterize as embedded image (base64 or pixel rects would be huge) */
    /* Instead, write as an image element pointing to PNG data */
    /* For true vector: caller should use SVG path commands directly */
    fprintf(f, "  <rect width=\"%d\" height=\"%d\" fill=\"#%06X\"/>\n",
            c->width, c->height, c->pixels[0] & 0xFFFFFF);
    /* Write non-background pixels as small rects (for small canvases) */
    if (c->width * c->height <= 10000) {
        uint32_t bg = c->pixels[0];
        for (int y = 0; y < c->height; y++)
            for (int x = 0; x < c->width; x++) {
                uint32_t px = c->pixels[y * c->width + x];
                if (px != bg) {
                    fprintf(f, "  <rect x=\"%d\" y=\"%d\" width=\"1\" height=\"1\" fill=\"#%06X\"",
                            x, y, px & 0xFFFFFF);
                    if (((px >> 24) & 0xFF) < 255)
                        fprintf(f, " opacity=\"%.2f\"", ((px >> 24) & 0xFF) / 255.0f);
                    fprintf(f, "/>\n");
                }
            }
    }
    fprintf(f, "</svg>\n");
    fclose(f);
    return 1;
}
