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
/* ── ts_gfx_extra.h — Advanced 2D features for TITAN GFX Engine ──────
 * Alpha blending, PNG export/load, Bezier curves, gradients,
 * rounded rects, dash patterns, image paste.
 * Single-header C99. PNG requires zlib.
 *
 * SoftCAD C+C TOPO © 2026
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Detect zlib independently */
#ifndef TS_GFX_HAS_ZLIB
  #ifdef TS_GFX_HAS_ZLIB
    #define TS_GFX_HAS_ZLIB TS_GFX_HAS_ZLIB
  #elif !defined(TS_PDF_NO_ZLIB)
    #include <zlib.h>
    #define TS_GFX_HAS_ZLIB 1
  #else
    #define TS_GFX_HAS_ZLIB 0
  #endif
#endif

/* ── Byte-reading helpers for PNG parsing ───────────────────────── */
static unsigned int _gfx_u32(const unsigned char *p) {
    return (unsigned int)((p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]);
}

/* ── Alpha Blending ────────────────────────────────────────────────── */

static inline uint32_t _ts_blend(uint32_t dst, uint32_t src) {
    int sa = (src >> 24) & 0xFF;
    if (sa == 255) return src;
    if (sa == 0)   return dst;
    int sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    int dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    int da = (dst >> 24) & 0xFF;
    int r = (sr * sa + dr * (255 - sa)) / 255;
    int g = (sg * sa + dg * (255 - sa)) / 255;
    int b = (sb * sa + db * (255 - sa)) / 255;
    int a = sa + (da * (255 - sa)) / 255;
    return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
}

void ts_canvas_set_pixel_blend(TS_Canvas *c, int x, int y, uint32_t color) {
    if (x < 0 || x >= c->width || y < 0 || y >= c->height) return;
    int idx = y * c->width + x;
    c->pixels[idx] = _ts_blend(c->pixels[idx], color);
}

void ts_draw_rect_filled_alpha(TS_Canvas *c, int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            ts_canvas_set_pixel_blend(c, x + dx, y + dy, color);
}

void ts_draw_circle_filled_alpha(TS_Canvas *c, int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx*dx + dy*dy <= r*r)
                ts_canvas_set_pixel_blend(c, cx + dx, cy + dy, color);
}

/* ── PNG Export ─────────────────────────────────────────────────────── */
/* Uses stb_image_write.h if available, otherwise manual PNG with zlib */

#if TS_GFX_HAS_ZLIB
int ts_canvas_save_png(const TS_Canvas *c, const char *path) {
    int w = c->width, h = c->height;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    /* Pixelii canvas-ului sunt AABBGGRR (roșul în octetul de JOS) — citiți cu
       ts_r/ts_g/ts_b/ts_a, ca în tot motorul. Vechiul cod presupunea ARGB
       (roșul la >>16): PNG-ul ieșea cu R și B inversate, iar cititorul PNG
       (corect) le inversa înapoi greșit la dus-întors. Și scria doar RGB —
       canvasurile transparente ieșeau opace. Acum: RGBA (color type 6), cu
       culoarea readusă la alfa DREPT (amestecul peste transparent o lasă
       înmulțită cu alfa). Raportat de TitanCAD, 4.59. */
    int stride = w * 4;
    int raw_size = (1 + stride) * h;
    unsigned char *raw = (unsigned char *)malloc((size_t)raw_size);
    if (!raw) { fclose(f); return 0; }

    for (int y = 0; y < h; y++) {
        raw[y * (1 + stride)] = 0; /* filter: None */
        for (int x = 0; x < w; x++) {
            uint32_t px = c->pixels[y * w + x];
            unsigned int r = ts_r(px), g = ts_g(px), b = ts_b(px), a = ts_a(px);
            if (a == 0) { r = g = b = 0; }
            else if (a < 255) {
                r = (r * 255u + a / 2u) / a; if (r > 255u) r = 255u;
                g = (g * 255u + a / 2u) / a; if (g > 255u) g = 255u;
                b = (b * 255u + a / 2u) / a; if (b > 255u) b = 255u;
            }
            unsigned char *o = raw + y * (1 + stride) + 1 + x * 4;
            o[0] = (unsigned char)r; o[1] = (unsigned char)g;
            o[2] = (unsigned char)b; o[3] = (unsigned char)a;
        }
    }

    /* Compress with zlib */
    uLongf comp_size = compressBound((uLong)raw_size);
    unsigned char *comp = (unsigned char *)malloc(comp_size);
    if (!comp) { free(raw); fclose(f); return 0; }
    compress2(comp, &comp_size, raw, (uLong)raw_size, 6);
    free(raw);

    /* Write PNG file */
    /* Signature */
    unsigned char sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);

    /* Helper to write chunk */
    #define _PNG_CHUNK(tag, data, len) do { \
        unsigned char _hdr[8]; \
        _hdr[0]=((len)>>24)&0xFF; _hdr[1]=((len)>>16)&0xFF; \
        _hdr[2]=((len)>>8)&0xFF;  _hdr[3]=(len)&0xFF; \
        _hdr[4]=tag[0]; _hdr[5]=tag[1]; _hdr[6]=tag[2]; _hdr[7]=tag[3]; \
        fwrite(_hdr, 1, 8, f); \
        unsigned long _crc = crc32(0L, _hdr+4, 4); \
        if((len)>0 && (data)!=NULL) { \
            fwrite((data), 1, (size_t)(len), f); \
            _crc = crc32(_crc, (const unsigned char*)(data), (unsigned)(len)); \
        } \
        unsigned char _cb[4] = {(unsigned char)((_crc>>24)&0xFF), (unsigned char)((_crc>>16)&0xFF), \
                                (unsigned char)((_crc>>8)&0xFF), (unsigned char)(_crc&0xFF)}; \
        fwrite(_cb, 1, 4, f); \
    } while(0)

    /* IHDR: 13 bytes */
    unsigned char ihdr[13];
    ihdr[0]=(w>>24)&0xFF; ihdr[1]=(w>>16)&0xFF; ihdr[2]=(w>>8)&0xFF; ihdr[3]=w&0xFF;
    ihdr[4]=(h>>24)&0xFF; ihdr[5]=(h>>16)&0xFF; ihdr[6]=(h>>8)&0xFF; ihdr[7]=h&0xFF;
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 6;  /* color type: RGBA (4.59; era 2 = RGB, fără alfa) */
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */
    _PNG_CHUNK("IHDR", ihdr, 13);

    /* IDAT */
    _PNG_CHUNK("IDAT", comp, (int)comp_size);
    free(comp);

    /* IEND */
    /* IEND: empty chunk (no data) */
    {unsigned char _ie[8]={0,0,0,0,'I','E','N','D'};
     fwrite(_ie, 1, 8, f);
     unsigned long _ic=crc32(0L,_ie+4,4);
     unsigned char _ib[4]={(unsigned char)((_ic>>24)&0xFF),(unsigned char)((_ic>>16)&0xFF),
                           (unsigned char)((_ic>>8)&0xFF),(unsigned char)(_ic&0xFF)};
     fwrite(_ib, 1, 4, f);}

    #undef _PNG_CHUNK
    fclose(f);
    return 1;
}
#else
int ts_canvas_save_png(const TS_Canvas *c, const char *path) {
    (void)c; (void)path; return 0;
}
#endif

/* ── Image Load (PNG → Canvas pixels) ──────────────────────────────── */

#if TS_GFX_HAS_ZLIB
int ts_canvas_load_png(TS_Canvas *c, const char *path, int dx, int dy) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 57) { fclose(f); return 0; }

    unsigned char *raw = (unsigned char *)malloc((size_t)fsize);
    if (!raw) { fclose(f); return 0; }
    size_t rd = fread(raw, 1, (size_t)fsize, f);
    fclose(f);
    if ((long)rd != fsize || raw[0] != 0x89) { free(raw); return 0; }

    int w = (int)_gfx_u32(raw + 16);
    int h2 = (int)_gfx_u32(raw + 20);
    int depth = raw[24], ctype = raw[25];
    if (depth != 8 || (ctype != 2 && ctype != 6)) { free(raw); return 0; }
    int ch = (ctype == 6) ? 4 : 3;

    /* Collect IDAT */
    unsigned char *idat = (unsigned char *)malloc((size_t)fsize);
    int ilen = 0;
    long pos = 8;
    while (pos + 12 <= fsize) {
        int cl = (int)_gfx_u32(raw + pos);
        if (raw[pos+4]=='I'&&raw[pos+5]=='D'&&raw[pos+6]=='A'&&raw[pos+7]=='T') {
            memcpy(idat + ilen, raw + pos + 8, (size_t)cl);
            ilen += cl;
        }
        if (raw[pos+4]=='I'&&raw[pos+5]=='E'&&raw[pos+6]=='N'&&raw[pos+7]=='D') break;
        pos += 12 + cl;
    }
    free(raw);

    int raw_sz = (1 + w * ch) * h2;
    unsigned char *dec = (unsigned char *)malloc((size_t)raw_sz + 1024);
    uLongf dl = (uLongf)(raw_sz + 1024);
    if (uncompress(dec, &dl, idat, (uLong)ilen) != Z_OK) {
        free(idat); free(dec); return 0;
    }
    free(idat);

    /* Un-filter and blit to canvas */
    int stride = w * ch;
    for (int row = 0; row < h2; row++) {
        unsigned char *src = dec + row * (1 + stride);
        int filter = src[0];
        unsigned char *cur = src + 1;
        unsigned char *prev = (row > 0) ? dec + (row-1) * (1 + stride) + 1 : NULL;
        for (int i = 0; i < stride; i++) {
            unsigned char a = (i >= ch) ? cur[i - ch] : 0;
            unsigned char b = prev ? prev[i] : 0;
            unsigned char cv = (prev && i >= ch) ? prev[i - ch] : 0;
            switch (filter) {
                case 1: cur[i] += a; break;
                case 2: cur[i] += b; break;
                case 3: cur[i] += (unsigned char)((a + b) / 2); break;
                case 4: { int pa=abs(b-cv),pb=abs(a-cv),pc=abs(a+b-2*cv);
                          cur[i]+=(pa<=pb&&pa<=pc)?a:(pb<=pc)?b:cv; break; }
            }
        }
        for (int x = 0; x < w; x++) {
            int px = dx + x, py = dy + row;
            if (px >= 0 && px < c->width && py >= 0 && py < c->height) {
                uint32_t color;
                if (ch == 4)
                    color = ts_rgba(cur[x*4], cur[x*4+1], cur[x*4+2], cur[x*4+3]);
                else
                    color = ts_rgb(cur[x*3], cur[x*3+1], cur[x*3+2]);
                if (ch == 4)
                    ts_canvas_set_pixel_blend(c, px, py, color);
                else
                    c->pixels[py * c->width + px] = color;
            }
        }
    }
    free(dec);
    return 1;
}
#else
int ts_canvas_load_png(TS_Canvas *c, const char *path, int dx, int dy) {
    (void)c; (void)path; (void)dx; (void)dy; return 0;
}
#endif

/* ── Bezier Curve (cubic, De Casteljau) ────────────────────────────── */

void ts_draw_bezier(TS_Canvas *c, float x0, float y0, float x1, float y1,
                     float x2, float y2, float x3, float y3, int segments) {
    if (segments < 4) segments = 20;
    float px = x0, py = y0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float u = 1.0f - t;
        float x = u*u*u*x0 + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3;
        float y = u*u*u*y0 + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3;
        ts_draw_line(c, (int)px, (int)py, (int)x, (int)y);
        px = x; py = y;
    }
}

void ts_draw_bezier_aa(TS_Canvas *c, float x0, float y0, float x1, float y1,
                        float x2, float y2, float x3, float y3, int segments) {
    if (segments < 4) segments = 30;
    float px = x0, py = y0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float u = 1.0f - t;
        float x = u*u*u*x0 + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3;
        float y = u*u*u*y0 + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3;
        ts_draw_line_aa(c, (int)px, (int)py, (int)x, (int)y);
        px = x; py = y;
    }
}

/* ── Quadratic Bezier ──────────────────────────────────────────────── */

void ts_draw_bezier_quad(TS_Canvas *c, float x0, float y0,
                          float cx, float cy, float x1, float y1, int segments) {
    if (segments < 4) segments = 20;
    float px = x0, py = y0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float u = 1.0f - t;
        float x = u*u*x0 + 2*u*t*cx + t*t*x1;
        float y = u*u*y0 + 2*u*t*cy + t*t*y1;
        ts_draw_line(c, (int)px, (int)py, (int)x, (int)y);
        px = x; py = y;
    }
}

/* ── Rounded Rectangle ─────────────────────────────────────────────── */

void ts_draw_rounded_rect(TS_Canvas *c, int x, int y, int w, int h, int r) {
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    /* Top/bottom edges */
    ts_draw_line(c, x+r, y, x+w-r, y);
    ts_draw_line(c, x+r, y+h, x+w-r, y+h);
    /* Left/right edges */
    ts_draw_line(c, x, y+r, x, y+h-r);
    ts_draw_line(c, x+w, y+r, x+w, y+h-r);
    /* Corner arcs */
    ts_draw_arc(c, x+r,   y+r,   r, 180.0f, 270.0f);  /* top-left */
    ts_draw_arc(c, x+w-r, y+r,   r, 270.0f, 360.0f);  /* top-right */
    ts_draw_arc(c, x+w-r, y+h-r, r, 0.0f,   90.0f);   /* bottom-right */
    ts_draw_arc(c, x+r,   y+h-r, r, 90.0f,  180.0f);  /* bottom-left */
}

void ts_draw_rounded_rect_filled(TS_Canvas *c, int x, int y, int w, int h, int r) {
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    /* Center rect */
    ts_draw_rect_filled(c, x+r, y, w-2*r, h);
    /* Side rects */
    ts_draw_rect_filled(c, x, y+r, r, h-2*r);
    ts_draw_rect_filled(c, x+w-r, y+r, r, h-2*r);
    /* Corner circles */
    ts_draw_circle_filled(c, x+r,   y+r,   r);
    ts_draw_circle_filled(c, x+w-r, y+r,   r);
    ts_draw_circle_filled(c, x+w-r, y+h-r, r);
    ts_draw_circle_filled(c, x+r,   y+h-r, r);
}

/* ── Gradient Fill (linear, horizontal) ────────────────────────────── */

void ts_draw_gradient_h(TS_Canvas *c, int x, int y, int w, int h,
                         uint32_t color1, uint32_t color2) {
    for (int dx = 0; dx < w; dx++) {
        float t = (float)dx / (float)(w - 1);
        int r = (int)((1-t) * ts_r(color1) + t * ts_r(color2));
        int g = (int)((1-t) * ts_g(color1) + t * ts_g(color2));
        int b = (int)((1-t) * ts_b(color1) + t * ts_b(color2));
        uint32_t col = ts_rgb(r, g, b);
        for (int dy = 0; dy < h; dy++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < c->width && py >= 0 && py < c->height)
                c->pixels[py * c->width + px] = col;
        }
    }
}

/* Vertical gradient */
void ts_draw_gradient_v(TS_Canvas *c, int x, int y, int w, int h,
                         uint32_t color1, uint32_t color2) {
    for (int dy = 0; dy < h; dy++) {
        float t = (float)dy / (float)(h - 1);
        int r = (int)((1-t) * ts_r(color1) + t * ts_r(color2));
        int g = (int)((1-t) * ts_g(color1) + t * ts_g(color2));
        int b = (int)((1-t) * ts_b(color1) + t * ts_b(color2));
        uint32_t col = ts_rgb(r, g, b);
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < c->width && py >= 0 && py < c->height)
                c->pixels[py * c->width + px] = col;
        }
    }
}

/* ── Dash Pattern Line ─────────────────────────────────────────────── */

void ts_draw_line_dashed(TS_Canvas *c, int x0, int y0, int x1, int y1,
                          int dash_len, int gap_len) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int len = (int)sqrtf((float)(dx*dx + dy*dy));
    if (len == 0) return;
    int cycle = dash_len + gap_len;
    float fx = (float)(x1 - x0) / (float)len;
    float fy = (float)(y1 - y0) / (float)len;

    int seg_start = 0;
    while (seg_start < len) {
        int seg_end = seg_start + dash_len;
        if (seg_end > len) seg_end = len;
        int sx = x0 + (int)(seg_start * fx);
        int sy = y0 + (int)(seg_start * fy);
        int ex = x0 + (int)(seg_end * fx);
        int ey = y0 + (int)(seg_end * fy);
        ts_draw_line(c, sx, sy, ex, ey);
        seg_start += cycle;
    }
}

/* ── Flood Fill ────────────────────────────────────────────────────── */

void ts_draw_flood_fill(TS_Canvas *c, int x, int y, uint32_t fill_color) {
    if (x < 0 || x >= c->width || y < 0 || y >= c->height) return;
    uint32_t target = c->pixels[y * c->width + x];
    if (target == fill_color) return;

    /* Stack-based flood fill (avoids recursion stack overflow) */
    int *stack = (int *)malloc((size_t)(c->width * c->height * 2) * sizeof(int));
    if (!stack) return;
    int sp = 0;
    stack[sp++] = x; stack[sp++] = y;

    while (sp > 0) {
        int cy2 = stack[--sp];
        int cx = stack[--sp];
        if (cx < 0 || cx >= c->width || cy2 < 0 || cy2 >= c->height) continue;
        int idx = cy2 * c->width + cx;
        if (c->pixels[idx] != target) continue;
        c->pixels[idx] = fill_color;
        stack[sp++] = cx+1; stack[sp++] = cy2;
        stack[sp++] = cx-1; stack[sp++] = cy2;
        stack[sp++] = cx;   stack[sp++] = cy2+1;
        stack[sp++] = cx;   stack[sp++] = cy2-1;
    }
    free(stack);
}
