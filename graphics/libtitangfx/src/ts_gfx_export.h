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
 * ts_gfx_export.h — Image Export (BMP native, PNG via stb_image_write)
 * =====================================================================
 * BMP export: zero dependencies, pure C99.
 * PNG export: requires stb_image_write.h (optional, single-header).
 *
 * #define TS_GFX_EXPORT_IMPLEMENTATION in ONE .c file before #include.
 *
 * SoftCAD C+C TOPO © 2026 — Titan Sentinel Script GFX Engine
 */
#ifndef TS_GFX_EXPORT_H
#define TS_GFX_EXPORT_H

#include "ts_gfx_canvas.h"
#include <stdio.h>

/* ── API ───────────────────────────────────────────────────────────────── */

/* BMP export — zero dependencies, always available */
int ts_canvas_save_bmp(const TS_Canvas *c, const char *path);

/* PNG export: compile with -DTS_GFX_USE_STB_IMAGE_WRITE and provide stb_image_write.h */

/* PPM export — simplest format, debugging only */
int ts_canvas_save_ppm(const TS_Canvas *c, const char *path);

/* ── Implementation ────────────────────────────────────────────────────── */
#ifdef TS_GFX_EXPORT_IMPLEMENTATION

/* ── BMP Writer (zero deps) ────────────────────────────────────────────── */

static void _write_le16(FILE *f, uint16_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

static void _write_le32(FILE *f, uint32_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

int ts_canvas_save_bmp(const TS_Canvas *c, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    int w = c->width, h = c->height;
    int row_bytes = w * 3;
    int padding = (4 - (row_bytes % 4)) % 4;
    int data_size = (row_bytes + padding) * h;
    int file_size = 54 + data_size;

    /* BMP header (14 bytes) */
    fputc('B', f); fputc('M', f);
    _write_le32(f, (uint32_t)file_size);
    _write_le16(f, 0); _write_le16(f, 0); /* reserved */
    _write_le32(f, 54);                    /* data offset */

    /* DIB header (40 bytes — BITMAPINFOHEADER) */
    _write_le32(f, 40);                    /* header size */
    _write_le32(f, (uint32_t)w);
    _write_le32(f, (uint32_t)h);           /* positive = bottom-up */
    _write_le16(f, 1);                     /* planes */
    _write_le16(f, 24);                    /* bits per pixel */
    _write_le32(f, 0);                     /* compression (none) */
    _write_le32(f, (uint32_t)data_size);
    _write_le32(f, 2835); _write_le32(f, 2835); /* 72 DPI */
    _write_le32(f, 0); _write_le32(f, 0);

    /* Pixel data (bottom-up, BGR) */
    unsigned char pad[3] = {0, 0, 0};
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint32_t px = c->pixels[y * w + x];
            /* BMP uses BGR order */
            fputc(ts_b(px), f);
            fputc(ts_g(px), f);
            fputc(ts_r(px), f);
        }
        if (padding > 0) fwrite(pad, 1, (size_t)padding, f);
    }

    fclose(f);
    return 1;
}

/* ── PPM Writer (debug) ────────────────────────────────────────────────── */

int ts_canvas_save_ppm(const TS_Canvas *c, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%d %d\n255\n", c->width, c->height);
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        fputc(ts_r(px), f);
        fputc(ts_g(px), f);
        fputc(ts_b(px), f);
    }
    fclose(f);
    return 1;
}

/* PNG: enable with -DTS_GFX_USE_STB_IMAGE_WRITE + stb_image_write.h */

#endif /* TS_GFX_EXPORT_IMPLEMENTATION */
#endif /* TS_GFX_EXPORT_H */
