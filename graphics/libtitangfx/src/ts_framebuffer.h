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
#ifndef TS_FRAMEBUFFER_H
#define TS_FRAMEBUFFER_H

/*
 * ts_framebuffer.h — Multi-layer Frame Buffer System
 * SoftCAD C+C TOPO © 2026
 *
 * CE FAC AUTODESK/QGIS/MAPBOX:
 *
 *   ┌─────────────────────────────────────────┐
 *   │  ECRAN (ce vede utilizatorul)            │
 *   ├─────────────────────────────────────────┤
 *   │  Layer 5: OVERLAY (selecție, highlight)  │ ← re-render doar la click
 *   │  Layer 4: LABELS (texte, numere cad.)    │ ← re-render doar la zoom
 *   │  Layer 3: BUILDINGS (construcții)        │ ← re-render doar la zoom
 *   │  Layer 2: PARCELS (terenuri)             │ ← re-render doar la zoom
 *   │  Layer 1: ROADS (drumuri, hotare)        │ ← re-render doar la zoom  
 *   │  Layer 0: BACKGROUND (grid, referință)   │ ← static
 *   └─────────────────────────────────────────┘
 *
 *   PAN: shift TOATE layers (memcpy + margin render) → INSTANT
 *   ZOOM: scale layers (bilinear) + async re-render → INSTANT blur, then sharp
 *   HOVER: re-render DOAR Layer 5 (overlay) → 1 entitate
 *   EDIT: re-render DOAR layer-ul afectat → restul intact
 *
 * DOUBLE BUFFERING:
 *   Front buffer = afișat pe ecran
 *   Back buffer = se randează
 *   Swap = pointer swap, zero copy
 *
 * VS CE FACEM NOI ACUM:
 *   Un singur buffer → re-render TOT la fiecare frame
 *   Pan = re-render 60K entități
 *   Hover = re-render 60K entități
 *   → FREEZE
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FB_MAX_LAYERS 12
#define FB_ALPHA_OPAQUE 255

#pragma pack(push, 1)

typedef struct {
    unsigned char *data;    /* RGBA pixels (w × h × 4) */
    int w, h;
    int dirty;              /* needs re-render? */
    int visible;
    unsigned char opacity;  /* 0-255 global layer opacity */
    char name[32];
    /* Transform (for pan without re-render) */
    int offset_x, offset_y; /* pixel offset from base position */
} FB_Layer;

typedef struct {
    FB_Layer layers[FB_MAX_LAYERS];
    int n_layers;
    int width, height;
    /* Double buffer */
    unsigned char *front;   /* displayed */
    unsigned char *back;    /* being rendered */
    int frame_count;
    /* Composition stats */
    int layers_composited;
    int pixels_blended;
    double compose_ms;
} FB_System;

#pragma pack(pop)

/* ═══════════════════════════════════════════════════ */
/*  CREATE / FREE                                      */
/* ═══════════════════════════════════════════════════ */

FB_System *ts_fb_new(int width, int height) {
    FB_System *fb = (FB_System*)calloc(1, sizeof(FB_System));
    fb->width = width;
    fb->height = height;
    fb->front = (unsigned char*)calloc(width * height * 4, 1);
    fb->back = (unsigned char*)calloc(width * height * 4, 1);
    return fb;
}

void ts_fb_free(FB_System *fb) {
    if (!fb) return;
    for (int i = 0; i < fb->n_layers; i++)
        free(fb->layers[i].data);
    free(fb->front);
    free(fb->back);
    free(fb);
}

/* ═══════════════════════════════════════════════════ */
/*  LAYER MANAGEMENT                                   */
/* ═══════════════════════════════════════════════════ */

int ts_fb_add_layer(FB_System *fb, const char *name, unsigned char opacity) {
    if (!fb || fb->n_layers >= FB_MAX_LAYERS) return -1;
    FB_Layer *l = &fb->layers[fb->n_layers];
    l->data = (unsigned char*)calloc(fb->width * fb->height * 4, 1);
    l->w = fb->width;
    l->h = fb->height;
    l->dirty = 1;
    l->visible = 1;
    l->opacity = opacity;
    l->offset_x = 0;
    l->offset_y = 0;
    strncpy(l->name, name, 31);
    return fb->n_layers++;
}

void ts_fb_layer_set_dirty(FB_System *fb, int layer_idx) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return;
    fb->layers[layer_idx].dirty = 1;
}

void ts_fb_layer_set_clean(FB_System *fb, int layer_idx) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return;
    fb->layers[layer_idx].dirty = 0;
}

void ts_fb_layer_set_visible(FB_System *fb, int layer_idx, int visible) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return;
    fb->layers[layer_idx].visible = visible;
}

void ts_fb_layer_set_opacity(FB_System *fb, int layer_idx, unsigned char opacity) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return;
    fb->layers[layer_idx].opacity = opacity;
}

int ts_fb_layer_is_dirty(const FB_System *fb, int layer_idx) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return 0;
    return fb->layers[layer_idx].dirty;
}

/* Get RGBA pixel buffer for drawing INTO layer */
unsigned char *ts_fb_layer_pixels(FB_System *fb, int layer_idx) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return NULL;
    return fb->layers[layer_idx].data;
}

/* Clear layer to transparent */
void ts_fb_layer_clear(FB_System *fb, int layer_idx) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return;
    memset(fb->layers[layer_idx].data, 0, fb->width * fb->height * 4);
}

/* Clear layer region to transparent (for partial update) */
void ts_fb_layer_clear_region(FB_System *fb, int layer_idx,
                                int x, int y, int w, int h) {
    if (!fb || layer_idx < 0 || layer_idx >= fb->n_layers) return;
    FB_Layer *l = &fb->layers[layer_idx];
    for (int py = y; py < y + h && py < l->h; py++) {
        if (py < 0) continue;
        int start = (py * l->w + (x < 0 ? 0 : x)) * 4;
        int end = (py * l->w + (x + w > l->w ? l->w : x + w)) * 4;
        if (start < end) memset(l->data + start, 0, end - start);
    }
}

/* ═══════════════════════════════════════════════════ */
/*  DRAW INTO LAYER — basic primitives                 */
/* ═══════════════════════════════════════════════════ */

void ts_fb_layer_pixel(FB_System *fb, int layer, int x, int y,
                         unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!fb || layer < 0 || layer >= fb->n_layers) return;
    FB_Layer *l = &fb->layers[layer];
    if (x < 0 || x >= l->w || y < 0 || y >= l->h) return;
    int idx = (y * l->w + x) * 4;
    l->data[idx] = r; l->data[idx+1] = g; l->data[idx+2] = b; l->data[idx+3] = a;
}

void ts_fb_layer_fill_rect(FB_System *fb, int layer,
                             int x, int y, int w, int h,
                             unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!fb || layer < 0 || layer >= fb->n_layers) return;
    FB_Layer *l = &fb->layers[layer];
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= l->h) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= l->w) continue;
            int idx = (py * l->w + px) * 4;
            l->data[idx] = r; l->data[idx+1] = g; l->data[idx+2] = b; l->data[idx+3] = a;
        }
    }
}

void ts_fb_layer_line(FB_System *fb, int layer,
                        int x0, int y0, int x1, int y1,
                        unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!fb || layer < 0 || layer >= fb->n_layers) return;
    /* Bresenham */
    int dx = abs(x1-x0), dy = abs(y1-y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        ts_fb_layer_pixel(fb, layer, x0, y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

/* ═══════════════════════════════════════════════════ */
/*  PAN: shift all layers (ZERO re-render!)            */
/* ═══════════════════════════════════════════════════ */

void ts_fb_pan(FB_System *fb, int dx, int dy) {
    if (!fb) return;
    for (int i = 0; i < fb->n_layers; i++) {
        fb->layers[i].offset_x += dx;
        fb->layers[i].offset_y += dy;
        /* Mark edges as dirty for progressive fill */
    }
}

void ts_fb_pan_reset(FB_System *fb) {
    if (!fb) return;
    for (int i = 0; i < fb->n_layers; i++) {
        fb->layers[i].offset_x = 0;
        fb->layers[i].offset_y = 0;
    }
}

/* ═══════════════════════════════════════════════════ */
/*  COMPOSE: alpha-blend all layers → back buffer      */
/* ═══════════════════════════════════════════════════ */

int ts_fb_compose(FB_System *fb) {
    if (!fb) return 0;

    /* Clear back buffer to white */
    memset(fb->back, 255, fb->width * fb->height * 4);

    int blended = 0;
    for (int li = 0; li < fb->n_layers; li++) {
        FB_Layer *l = &fb->layers[li];
        if (!l->visible || !l->data) continue;

        int ox = l->offset_x, oy = l->offset_y;
        unsigned char layer_alpha = l->opacity;

        for (int y = 0; y < fb->height; y++) {
            int src_y = y - oy;
            if (src_y < 0 || src_y >= l->h) continue;

            for (int x = 0; x < fb->width; x++) {
                int src_x = x - ox;
                if (src_x < 0 || src_x >= l->w) continue;

                int si = (src_y * l->w + src_x) * 4;
                unsigned char sa = l->data[si + 3];
                if (sa == 0) continue; /* fully transparent */

                /* Combined alpha = pixel alpha × layer opacity */
                int alpha = (sa * layer_alpha) / 255;
                if (alpha == 0) continue;

                int di = (y * fb->width + x) * 4;
                if (alpha == 255) {
                    /* Opaque — direct copy */
                    fb->back[di]   = l->data[si];
                    fb->back[di+1] = l->data[si+1];
                    fb->back[di+2] = l->data[si+2];
                    fb->back[di+3] = 255;
                } else {
                    /* Alpha blend: out = src × α + dst × (1-α) */
                    int inv = 255 - alpha;
                    fb->back[di]   = (l->data[si] * alpha + fb->back[di] * inv) / 255;
                    fb->back[di+1] = (l->data[si+1] * alpha + fb->back[di+1] * inv) / 255;
                    fb->back[di+2] = (l->data[si+2] * alpha + fb->back[di+2] * inv) / 255;
                    fb->back[di+3] = 255;
                }
                blended++;
            }
        }
    }

    fb->pixels_blended = blended;
    fb->layers_composited = fb->n_layers;
    return blended;
}

/* ═══════════════════════════════════════════════════ */
/*  SWAP: double buffer swap (pointer swap = instant)  */
/* ═══════════════════════════════════════════════════ */

void ts_fb_swap(FB_System *fb) {
    if (!fb) return;
    unsigned char *tmp = fb->front;
    fb->front = fb->back;
    fb->back = tmp;
    fb->frame_count++;
}

/* Get front buffer for display */
const unsigned char *ts_fb_front(const FB_System *fb) {
    return fb ? fb->front : NULL;
}

/* Get back buffer for direct manipulation */
unsigned char *ts_fb_back(FB_System *fb) {
    return fb ? fb->back : NULL;
}

/* ═══════════════════════════════════════════════════ */
/*  STATS                                              */
/* ═══════════════════════════════════════════════════ */

int ts_fb_layer_count(const FB_System *fb) { return fb ? fb->n_layers : 0; }
int ts_fb_frame_count(const FB_System *fb) { return fb ? fb->frame_count : 0; }
int ts_fb_dirty_layers(const FB_System *fb) {
    if (!fb) return 0;
    int n = 0;
    for (int i = 0; i < fb->n_layers; i++)
        if (fb->layers[i].dirty) n++;
    return n;
}

#endif /* TS_FRAMEBUFFER_H */
