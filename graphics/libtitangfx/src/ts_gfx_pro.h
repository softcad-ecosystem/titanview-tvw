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
/* ── ts_gfx_pro.h — Professional rendering, charts, GIS, compositing ──
 * Makes TITAN GFX the most complete single-header renderer on Earth.
 * C99, zero dependencies, software-only, no GPU required.
 * SoftCAD C+C TOPO © 2026
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════════ */
/*  PORTER-DUFF BLEND MODES                                           */
/* ═══════════════════════════════════════════════════════════════════ */

static inline uint8_t _bl_clamp(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
static inline uint8_t _ch(uint32_t c, int s) { return (uint8_t)((c >> s) & 0xFF); }

uint32_t ts_blend_multiply(uint32_t dst, uint32_t src) {
    return 0xFF000000 |
        (uint32_t)((_ch(dst,16)*_ch(src,16)/255) << 16) |
        (uint32_t)((_ch(dst,8)*_ch(src,8)/255) << 8) |
        (uint32_t)(_ch(dst,0)*_ch(src,0)/255);
}

uint32_t ts_blend_screen(uint32_t dst, uint32_t src) {
    return 0xFF000000 |
        (uint32_t)(_bl_clamp(_ch(dst,16)+_ch(src,16)-_ch(dst,16)*_ch(src,16)/255) << 16) |
        (uint32_t)(_bl_clamp(_ch(dst,8)+_ch(src,8)-_ch(dst,8)*_ch(src,8)/255) << 8) |
        (uint32_t)_bl_clamp(_ch(dst,0)+_ch(src,0)-_ch(dst,0)*_ch(src,0)/255);
}

uint32_t ts_blend_overlay(uint32_t dst, uint32_t src) {
    uint32_t r = 0xFF000000;
    for (int s = 0; s <= 16; s += 8) {
        int a = _ch(dst, s), b = _ch(src, s);
        int v = (a < 128) ? (2*a*b/255) : (255 - 2*(255-a)*(255-b)/255);
        r |= (uint32_t)(_bl_clamp(v) << s);
    }
    return r;
}

uint32_t ts_blend_soft_light(uint32_t dst, uint32_t src) {
    uint32_t r = 0xFF000000;
    for (int s = 0; s <= 16; s += 8) {
        int a = _ch(dst, s), b = _ch(src, s);
        int v = (b < 128) ? (a - (255-2*b)*a*(255-a)/(255*255))
                          : (a + (2*b-255)*((int)sqrt(a*255.0)-a)/255);
        r |= (uint32_t)(_bl_clamp(v) << s);
    }
    return r;
}

uint32_t ts_blend_hard_light(uint32_t dst, uint32_t src) {
    return ts_blend_overlay(src, dst); /* swap args */
}

uint32_t ts_blend_color_dodge(uint32_t dst, uint32_t src) {
    uint32_t r = 0xFF000000;
    for (int s = 0; s <= 16; s += 8) {
        int a = _ch(dst, s), b = _ch(src, s);
        r |= (uint32_t)(_bl_clamp(b >= 255 ? 255 : a * 255 / (255 - b + 1)) << s);
    }
    return r;
}

uint32_t ts_blend_color_burn(uint32_t dst, uint32_t src) {
    uint32_t r = 0xFF000000;
    for (int s = 0; s <= 16; s += 8) {
        int a = _ch(dst, s), b = _ch(src, s);
        r |= (uint32_t)(_bl_clamp(b <= 0 ? 0 : 255 - (255-a)*255/(b+1)) << s);
    }
    return r;
}

uint32_t ts_blend_difference(uint32_t dst, uint32_t src) {
    return 0xFF000000 |
        (uint32_t)(abs(_ch(dst,16)-_ch(src,16)) << 16) |
        (uint32_t)(abs(_ch(dst,8)-_ch(src,8)) << 8) |
        (uint32_t)abs(_ch(dst,0)-_ch(src,0));
}

/* Apply blend mode to entire canvas overlay */
void ts_canvas_blend(TS_Canvas *dst, const TS_Canvas *src, int dx, int dy,
                      uint32_t (*blend_fn)(uint32_t, uint32_t)) {
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int px = dx + x, py = dy + y;
            if (px >= 0 && px < dst->width && py >= 0 && py < dst->height) {
                uint32_t d = dst->pixels[py * dst->width + px];
                uint32_t s = src->pixels[y * src->width + x];
                dst->pixels[py * dst->width + px] = blend_fn(d, s);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  DATA VISUALIZATION — charts built into the engine!                */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── Axis with labels and grid ──────────────────────────────────── */
void ts_chart_axis(TS_Canvas *c, int x, int y, int w, int h,
                    double xmin, double xmax, double ymin, double ymax,
                    int grid_x, int grid_y, uint32_t axis_color, uint32_t grid_color) {
    /* Draw axis lines */
    uint32_t old = c->fill_color;
    c->fill_color = axis_color;
    ts_draw_line(c, x, y, x + w, y);         /* bottom */
    ts_draw_line(c, x, y, x, y - h);         /* left */

    /* Grid */
    c->fill_color = grid_color;
    if (grid_x > 0) for (int i = 1; i <= grid_x; i++) {
        int gx = x + i * w / grid_x;
        for (int gy = y; gy > y - h; gy -= 4) ts_pixel_set(c, gx, gy, grid_color);
    }
    if (grid_y > 0) for (int i = 1; i <= grid_y; i++) {
        int gy = y - i * h / grid_y;
        for (int gx = x; gx < x + w; gx += 4) ts_pixel_set(c, gx, gy, grid_color);
    }
    c->fill_color = old;
}

/* ── Bar chart ──────────────────────────────────────────────────── */
void ts_chart_bar(TS_Canvas *c, int x, int y, int w, int h,
                   const double *values, int n, uint32_t color, uint32_t bg) {
    if (n <= 0) return;
    double vmax = values[0];
    for (int i = 1; i < n; i++) if (values[i] > vmax) vmax = values[i];
    if (vmax <= 0) vmax = 1;

    int bar_w = w / n;
    int gap = bar_w > 4 ? 2 : 0;
    for (int i = 0; i < n; i++) {
        int bh = (int)(values[i] / vmax * h);
        int bx = x + i * bar_w + gap;
        int by = y - bh;
        ts_draw_rect_filled(c, bx, by, bar_w - gap * 2, bh);
    }
}

/* ── Pie chart ──────────────────────────────────────────────────── */
void ts_chart_pie(TS_Canvas *c, int cx, int cy, int radius,
                   const double *values, const uint32_t *colors, int n) {
    if (n <= 0) return;
    double total = 0;
    for (int i = 0; i < n; i++) total += values[i];
    if (total <= 0) return;

    double start = -M_PI / 2; /* start at top */
    for (int i = 0; i < n; i++) {
        double angle = values[i] / total * 2 * M_PI;
        double end = start + angle;
        /* Fill sector pixel by pixel */
        for (int y = cy - radius; y <= cy + radius; y++) {
            for (int x = cx - radius; x <= cx + radius; x++) {
                if ((x-cx)*(x-cx) + (y-cy)*(y-cy) > radius*radius) continue;
                double a = atan2((double)(y - cy), (double)(x - cx));
                /* Normalize to start..end range */
                while (a < start) a += 2 * M_PI;
                if (a >= start && a < end)
                    ts_pixel_set(c, x, y, colors[i % n]);
            }
        }
        start = end;
    }
}

/* ── Line chart with markers ────────────────────────────────────── */
void ts_chart_line(TS_Canvas *c, int x, int y, int w, int h,
                    const double *values, int n, uint32_t line_color, int marker_r) {
    if (n < 2) return;
    double vmin = values[0], vmax = values[0];
    for (int i = 1; i < n; i++) {
        if (values[i] < vmin) vmin = values[i];
        if (values[i] > vmax) vmax = values[i];
    }
    double range = vmax - vmin; if (range <= 0) range = 1;

    uint32_t old = c->fill_color;
    c->fill_color = line_color;
    int px = -1, py = -1;
    for (int i = 0; i < n; i++) {
        int cx = x + i * w / (n - 1);
        int cy = y - (int)((values[i] - vmin) / range * h);
        if (px >= 0) ts_draw_line(c, px, py, cx, cy);
        if (marker_r > 0) ts_draw_circle_filled(c, cx, cy, marker_r);
        px = cx; py = cy;
    }
    c->fill_color = old;
}

/* ── Scatter plot ───────────────────────────────────────────────── */
void ts_chart_scatter(TS_Canvas *c, int x, int y, int w, int h,
                       const double *xvals, const double *yvals, int n,
                       uint32_t color, int dot_r) {
    if (n < 1) return;
    double xmin=xvals[0],xmax=xvals[0],ymin=yvals[0],ymax=yvals[0];
    for (int i = 1; i < n; i++) {
        if (xvals[i]<xmin)xmin=xvals[i]; if(xvals[i]>xmax)xmax=xvals[i];
        if (yvals[i]<ymin)ymin=yvals[i]; if(yvals[i]>ymax)ymax=yvals[i];
    }
    double xr=xmax-xmin,yr=ymax-ymin;
    if(xr<=0)xr=1; if(yr<=0)yr=1;

    uint32_t old = c->fill_color; c->fill_color = color;
    for (int i = 0; i < n; i++) {
        int px = x + (int)((xvals[i]-xmin)/xr * w);
        int py = y - (int)((yvals[i]-ymin)/yr * h);
        ts_draw_circle_filled(c, px, py, dot_r > 0 ? dot_r : 3);
    }
    c->fill_color = old;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  GIS PROFESSIONAL                                                   */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── Hillshade from DEM grid ────────────────────────────────────── */
void ts_gis_hillshade(TS_Canvas *c, const float *dem, int dem_w, int dem_h,
                       float cell_size, float azimuth_deg, float altitude_deg) {
    float az = azimuth_deg * (float)M_PI / 180.0f;
    float alt = altitude_deg * (float)M_PI / 180.0f;

    for (int y = 1; y < dem_h - 1 && y < c->height; y++) {
        for (int x = 1; x < dem_w - 1 && x < c->width; x++) {
            /* 3x3 window for slope/aspect */
            float dzdx = ((dem[(y-1)*dem_w+x+1] + 2*dem[y*dem_w+x+1] + dem[(y+1)*dem_w+x+1])
                        - (dem[(y-1)*dem_w+x-1] + 2*dem[y*dem_w+x-1] + dem[(y+1)*dem_w+x-1]))
                        / (8 * cell_size);
            float dzdy = ((dem[(y+1)*dem_w+x-1] + 2*dem[(y+1)*dem_w+x] + dem[(y+1)*dem_w+x+1])
                        - (dem[(y-1)*dem_w+x-1] + 2*dem[(y-1)*dem_w+x] + dem[(y-1)*dem_w+x+1]))
                        / (8 * cell_size);
            float slope = atanf(sqrtf(dzdx*dzdx + dzdy*dzdy));
            float aspect = atan2f(dzdy, -dzdx);
            float hs = cosf(alt)*cosf(slope) + sinf(alt)*sinf(slope)*cosf(az - aspect);
            if (hs < 0) hs = 0; if (hs > 1) hs = 1;
            int v = (int)(hs * 255);
            c->pixels[y * c->width + x] = 0xFF000000 | (uint32_t)((v<<16)|(v<<8)|v);
        }
    }
}

/* ── Slope map from DEM ─────────────────────────────────────────── */
void ts_gis_slope_map(TS_Canvas *c, const float *dem, int dem_w, int dem_h,
                       float cell_size) {
    for (int y = 1; y < dem_h - 1 && y < c->height; y++) {
        for (int x = 1; x < dem_w - 1 && x < c->width; x++) {
            float dzdx = (dem[y*dem_w+x+1] - dem[y*dem_w+x-1]) / (2*cell_size);
            float dzdy = (dem[(y+1)*dem_w+x] - dem[(y-1)*dem_w+x]) / (2*cell_size);
            float slope = atanf(sqrtf(dzdx*dzdx + dzdy*dzdy)) * 180.0f / (float)M_PI;
            float t = slope / 45.0f; if (t > 1) t = 1;
            /* Green(flat) → Red(steep) */
            int r = (int)(t * 255), g = (int)((1-t) * 255);
            c->pixels[y * c->width + x] = 0xFF000000 | (uint32_t)((r<<16)|(g<<8)|0);
        }
    }
}

/* ── Contour lines from DEM (marching squares) ──────────────────── */
void ts_gis_contours(TS_Canvas *c, const float *dem, int dem_w, int dem_h,
                      float interval, float ox, float oy, float scale,
                      uint32_t color) {
    uint32_t old = c->fill_color; c->fill_color = color;
    /* Find min/max */
    float zmin = dem[0], zmax = dem[0];
    for (int i = 1; i < dem_w * dem_h; i++) {
        if (dem[i] < zmin) zmin = dem[i];
        if (dem[i] > zmax) zmax = dem[i];
    }
    /* For each contour level */
    for (float level = ((int)(zmin/interval)+1)*interval; level < zmax; level += interval) {
        for (int y = 0; y < dem_h - 1; y++) {
            for (int x = 0; x < dem_w - 1; x++) {
                float v00 = dem[y*dem_w+x] - level;
                float v10 = dem[y*dem_w+x+1] - level;
                float v01 = dem[(y+1)*dem_w+x] - level;
                float v11 = dem[(y+1)*dem_w+x+1] - level;
                /* Simple: draw line if sign changes across cell edge */
                if ((v00 > 0) != (v10 > 0)) {
                    float t = v00 / (v00 - v10);
                    int sx = (int)((x + t) * scale + ox), sy = (int)(y * scale + oy);
                    ts_pixel_set(c, sx, sy, color);
                }
                if ((v00 > 0) != (v01 > 0)) {
                    float t = v00 / (v00 - v01);
                    int sx = (int)(x * scale + ox), sy = (int)((y + t) * scale + oy);
                    ts_pixel_set(c, sx, sy, color);
                }
            }
        }
    }
    c->fill_color = old;
}

/* ── Label placement with collision avoidance ───────────────────── */
typedef struct { int x, y, w, h; } _LabelRect;
static _LabelRect _placed[1024];
static int _placed_count = 0;

void ts_gis_label_reset(void) { _placed_count = 0; }

int ts_gis_label(TS_Canvas *c, int x, int y, const char *text, uint32_t color) {
    int tw = (int)strlen(text) * 6, th = 10;
    /* Check collision with already placed labels */
    _LabelRect r = { x, y - th, tw + 4, th + 4 };
    for (int i = 0; i < _placed_count; i++) {
        _LabelRect *p = &_placed[i];
        if (!(r.x > p->x + p->w || r.x + r.w < p->x ||
              r.y > p->y + p->h || r.y + r.h < p->y))
            return 0; /* collision, skip */
    }
    if (_placed_count < 1024) _placed[_placed_count++] = r;
    ts_draw_text(c, x, y, text);
    return 1;
}

/* ── Hatch patterns (cadastral standard fills) ──────────────────── */
void ts_gis_hatch_lines(TS_Canvas *c, int x, int y, int w, int h,
                          int spacing, float angle_deg, uint32_t color) {
    float rad = angle_deg * (float)M_PI / 180.0f;
    float cs = cosf(rad), sn = sinf(rad);
    uint32_t old = c->fill_color; c->fill_color = color;
    int diag = (int)sqrtf((float)(w*w + h*h));
    for (int d = -diag; d < diag; d += spacing) {
        int x0 = x + (int)(d * cs - diag * sn);
        int y0 = y + (int)(d * sn + diag * cs);
        int x1 = x + (int)(d * cs + diag * sn);
        int y1 = y + (int)(d * sn - diag * cs);
        /* Clip to rect and draw */
        ts_draw_line(c, x0, y0, x1, y1);
    }
    c->fill_color = old;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  ADVANCED IMAGE PROCESSING                                         */
/* ═══════════════════════════════════════════════════════════════════ */

/* ── Morphological erode (min filter) ───────────────────────────── */
void ts_canvas_erode(TS_Canvas *c, int radius) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w*h)*4);
    if (!tmp) return;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int rmin=255,gmin=255,bmin=255;
        for (int dy = -radius; dy <= radius; dy++) for (int dx = -radius; dx <= radius; dx++) {
            int nx=x+dx,ny=y+dy;
            if (nx>=0 && nx<w && ny>=0 && ny<h) {
                uint32_t px = c->pixels[ny*w+nx];
                int r=(px>>16)&0xFF,g=(px>>8)&0xFF,b=px&0xFF;
                if(r<rmin)rmin=r; if(g<gmin)gmin=g; if(b<bmin)bmin=b;
            }
        }
        tmp[y*w+x] = 0xFF000000|(uint32_t)((rmin<<16)|(gmin<<8)|bmin);
    }
    memcpy(c->pixels, tmp, (size_t)(w*h)*4);
    free(tmp);
}

/* ── Morphological dilate (max filter) ──────────────────────────── */
void ts_canvas_dilate(TS_Canvas *c, int radius) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)(w*h)*4);
    if (!tmp) return;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int rmax=0,gmax=0,bmax=0;
        for (int dy = -radius; dy <= radius; dy++) for (int dx = -radius; dx <= radius; dx++) {
            int nx=x+dx,ny=y+dy;
            if (nx>=0 && nx<w && ny>=0 && ny<h) {
                uint32_t px = c->pixels[ny*w+nx];
                int r=(px>>16)&0xFF,g=(px>>8)&0xFF,b=px&0xFF;
                if(r>rmax)rmax=r; if(g>gmax)gmax=g; if(b>bmax)bmax=b;
            }
        }
        tmp[y*w+x] = 0xFF000000|(uint32_t)((rmax<<16)|(gmax<<8)|bmax);
    }
    memcpy(c->pixels, tmp, (size_t)(w*h)*4);
    free(tmp);
}

/* ── Histogram equalization ─────────────────────────────────────── */
void ts_canvas_equalize(TS_Canvas *c) {
    int w = c->width, h = c->height, total = w * h;
    int hist[256] = {0};
    /* Build histogram on luminance */
    for (int i = 0; i < total; i++) {
        uint32_t px = c->pixels[i];
        int gray = (((px>>16)&0xFF)*299 + ((px>>8)&0xFF)*587 + (px&0xFF)*114) / 1000;
        hist[gray]++;
    }
    /* CDF */
    int cdf[256]; cdf[0] = hist[0];
    for (int i = 1; i < 256; i++) cdf[i] = cdf[i-1] + hist[i];
    int cdf_min = 0;
    for (int i = 0; i < 256; i++) if (cdf[i] > 0) { cdf_min = cdf[i]; break; }
    /* Map */
    int map[256];
    for (int i = 0; i < 256; i++)
        map[i] = (total > cdf_min) ? (cdf[i] - cdf_min) * 255 / (total - cdf_min) : 0;
    /* Apply */
    for (int i = 0; i < total; i++) {
        uint32_t px = c->pixels[i];
        int r = (px>>16)&0xFF, g = (px>>8)&0xFF, b = px&0xFF;
        int gray = (r*299 + g*587 + b*114) / 1000;
        float ratio = gray > 0 ? (float)map[gray] / (float)gray : 1.0f;
        r = _bl_clamp((int)(r * ratio));
        g = _bl_clamp((int)(g * ratio));
        b = _bl_clamp((int)(b * ratio));
        c->pixels[i] = 0xFF000000 | (uint32_t)((r<<16)|(g<<8)|b);
    }
}

/* ── Image difference (change detection) ────────────────────────── */
void ts_canvas_difference(TS_Canvas *dst, const TS_Canvas *other) {
    int n = dst->width * dst->height;
    if (other->width * other->height < n) n = other->width * other->height;
    for (int i = 0; i < n; i++) {
        uint32_t a = dst->pixels[i], b = other->pixels[i];
        int ar = (int)((a >> 16) & 0xFFu), br = (int)((b >> 16) & 0xFFu);
        int ag = (int)((a >> 8) & 0xFFu),  bg = (int)((b >> 8) & 0xFFu);
        int ab = (int)(a & 0xFFu),         bb = (int)(b & 0xFFu);
        dst->pixels[i] = 0xFF000000u |
            ((uint32_t)abs(ar - br) << 16) |
            ((uint32_t)abs(ag - bg) << 8) |
            (uint32_t)abs(ab - bb);
    }
}

/* ── Adaptive threshold (Otsu's method) ─────────────────────────── */
void ts_canvas_threshold_otsu(TS_Canvas *c) {
    int w = c->width, h = c->height, total = w * h;
    int hist[256] = {0};
    for (int i = 0; i < total; i++) {
        uint32_t px = c->pixels[i];
        hist[(((px>>16)&0xFF)*299 + ((px>>8)&0xFF)*587 + (px&0xFF)*114) / 1000]++;
    }
    /* Otsu: find threshold that maximizes inter-class variance */
    double sum = 0;
    for (int i = 0; i < 256; i++) sum += i * hist[i];
    double sumB = 0; int wB = 0;
    double max_var = 0; int threshold = 0;
    for (int t = 0; t < 256; t++) {
        wB += hist[t]; if (wB == 0) continue;
        int wF = total - wB; if (wF == 0) break;
        sumB += t * hist[t];
        double mB = sumB / wB, mF = (sum - sumB) / wF;
        double var = (double)wB * wF * (mB - mF) * (mB - mF);
        if (var > max_var) { max_var = var; threshold = t; }
    }
    /* Apply */
    for (int i = 0; i < total; i++) {
        uint32_t px = c->pixels[i];
        int gray = (((px>>16)&0xFF)*299 + ((px>>8)&0xFF)*587 + (px&0xFF)*114) / 1000;
        int v = gray >= threshold ? 255 : 0;
        c->pixels[i] = 0xFF000000 | (uint32_t)((v<<16)|(v<<8)|v);
    }
}

/* ── Color channel operations ───────────────────────────────────── */
void ts_canvas_channel_red(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++)
        c->pixels[i] = 0xFF000000 | (c->pixels[i] & 0x00FF0000);
}
void ts_canvas_channel_green(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++)
        c->pixels[i] = 0xFF000000 | (c->pixels[i] & 0x0000FF00);
}
void ts_canvas_channel_blue(TS_Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++)
        c->pixels[i] = 0xFF000000 | (c->pixels[i] & 0x000000FF);
}

/* ── Posterize (reduce color levels) ────────────────────────────── */
void ts_canvas_posterize(TS_Canvas *c, int levels) {
    if (levels < 2) levels = 2;
    int step = 256 / levels;
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int r = (((px>>16)&0xFF)/step)*step*255/(step*(levels-1));
        int g = (((px>>8)&0xFF)/step)*step*255/(step*(levels-1));
        int b = ((px&0xFF)/step)*step*255/(step*(levels-1));
        c->pixels[i] = 0xFF000000 | (uint32_t)((_bl_clamp(r)<<16)|(_bl_clamp(g)<<8)|_bl_clamp(b));
    }
}

/* ── Pixelate ───────────────────────────────────────────────────── */
void ts_canvas_pixelate(TS_Canvas *c, int block_size) {
    if (block_size < 2) return;
    int w = c->width, h = c->height;
    for (int by = 0; by < h; by += block_size) {
        for (int bx = 0; bx < w; bx += block_size) {
            int sr=0,sg=0,sb=0,cnt=0;
            for (int y=by; y<by+block_size && y<h; y++)
                for (int x=bx; x<bx+block_size && x<w; x++) {
                    uint32_t px = c->pixels[y*w+x];
                    sr+=(px>>16)&0xFF; sg+=(px>>8)&0xFF; sb+=px&0xFF; cnt++;
                }
            if (cnt > 0) {
                uint32_t avg = 0xFF000000|(uint32_t)(((sr/cnt)<<16)|((sg/cnt)<<8)|(sb/cnt));
                for (int y=by; y<by+block_size && y<h; y++)
                    for (int x=bx; x<bx+block_size && x<w; x++)
                        c->pixels[y*w+x] = avg;
            }
        }
    }
}

/* ── Vignette effect ────────────────────────────────────────────── */
void ts_canvas_vignette(TS_Canvas *c, float strength) {
    int w = c->width, h = c->height;
    float cx = w / 2.0f, cy = h / 2.0f;
    float max_d = sqrtf(cx*cx + cy*cy);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float d = sqrtf((x-cx)*(x-cx) + (y-cy)*(y-cy)) / max_d;
            float factor = 1.0f - d * d * strength;
            if (factor < 0) factor = 0;
            uint32_t px = c->pixels[y*w+x];
            int r = (int)(((px>>16)&0xFF) * factor);
            int g = (int)(((px>>8)&0xFF) * factor);
            int b = (int)((px&0xFF) * factor);
            c->pixels[y*w+x] = 0xFF000000|(uint32_t)((r<<16)|(g<<8)|b);
        }
    }
}

/* ── Emboss ─────────────────────────────────────────────────────── */
void ts_canvas_emboss(TS_Canvas *c) {
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t*)malloc((size_t)(w*h)*4);
    if (!tmp) return;
    static const int k[3][3] = {{-2,-1,0},{-1,1,1},{0,1,2}};
    for (int y = 1; y < h-1; y++) for (int x = 1; x < w-1; x++) {
        int sr=128,sg=128,sb=128;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++) {
            uint32_t px = c->pixels[(y+dy)*w+(x+dx)];
            int wt = k[dy+1][dx+1];
            sr += ((px>>16)&0xFF)*wt;
            sg += ((px>>8)&0xFF)*wt;
            sb += (px&0xFF)*wt;
        }
        tmp[y*w+x] = 0xFF000000|(uint32_t)((_bl_clamp(sr)<<16)|(_bl_clamp(sg)<<8)|_bl_clamp(sb));
    }
    memcpy(c->pixels+w, tmp+w, (size_t)((h-2)*w)*4);
    free(tmp);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  PATH BUILDER (SVG-like moveTo/lineTo/curveTo/close)               */
/* ═══════════════════════════════════════════════════════════════════ */

#define TS_PATH_MAX 4096

typedef struct {
    float pts[TS_PATH_MAX * 2];
    int   cmds[TS_PATH_MAX]; /* 0=move, 1=line, 2=close */
    int   count;
} TS_Path;

void ts_path_reset(TS_Path *p) { p->count = 0; }

void ts_path_move_to(TS_Path *p, float x, float y) {
    if (p->count >= TS_PATH_MAX) return;
    p->pts[p->count*2] = x; p->pts[p->count*2+1] = y;
    p->cmds[p->count] = 0; p->count++;
}

void ts_path_line_to(TS_Path *p, float x, float y) {
    if (p->count >= TS_PATH_MAX) return;
    p->pts[p->count*2] = x; p->pts[p->count*2+1] = y;
    p->cmds[p->count] = 1; p->count++;
}

void ts_path_close(TS_Path *p) {
    if (p->count >= TS_PATH_MAX || p->count < 2) return;
    p->pts[p->count*2] = p->pts[0]; p->pts[p->count*2+1] = p->pts[1];
    p->cmds[p->count] = 2; p->count++;
}

void ts_path_stroke(TS_Canvas *c, const TS_Path *p) {
    for (int i = 1; i < p->count; i++) {
        if (p->cmds[i] == 0) continue; /* skip move-to */
        ts_draw_line(c, (int)p->pts[(i-1)*2], (int)p->pts[(i-1)*2+1],
                        (int)p->pts[i*2], (int)p->pts[i*2+1]);
    }
}

/* ── Variable-width polyline (thick stroke) ─────────────────────── */
void ts_draw_thick_line(TS_Canvas *c, int x0, int y0, int x1, int y1, int thickness) {
    float dx = (float)(x1-x0), dy = (float)(y1-y0);
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.1f) return;
    float nx = -dy/len * thickness/2.0f, ny = dx/len * thickness/2.0f;
    /* Filled quad */
    float pts[8] = {
        x0+nx, y0+ny, x1+nx, y1+ny,
        x1-nx, y1-ny, x0-nx, y0-ny
    };
    ts_draw_polygon_filled(c, pts, 4);
}

void ts_draw_thick_polyline(TS_Canvas *c, const float *pts, int npts, int thickness) {
    for (int i = 0; i < npts - 1; i++) {
        ts_draw_thick_line(c, (int)pts[i*2], (int)pts[i*2+1],
                              (int)pts[(i+1)*2], (int)pts[(i+1)*2+1], thickness);
    }
}

/* ── Perlin noise generator ─────────────────────────────────────── */
static int _perm[512];
static int _perm_init = 0;

static void _init_perm(void) {
    if (_perm_init) return;
    for (int i = 0; i < 256; i++) _perm[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = _perm[i]; _perm[i] = _perm[j]; _perm[j] = t;
    }
    for (int i = 0; i < 256; i++) _perm[256+i] = _perm[i];
    _perm_init = 1;
}

static float _fade(float t) { return t*t*t*(t*(t*6-15)+10); }
static float _lerp_f(float t, float a, float b) { return a + t*(b-a); }
static float _grad(int hash, float x, float y) {
    int h = hash & 3;
    float u = h < 2 ? x : y, v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float ts_perlin_noise(float x, float y) {
    _init_perm();
    int xi = (int)floorf(x) & 255, yi = (int)floorf(y) & 255;
    float xf = x - floorf(x), yf = y - floorf(y);
    float u = _fade(xf), v = _fade(yf);
    int aa = _perm[_perm[xi]+yi], ab = _perm[_perm[xi]+yi+1];
    int ba = _perm[_perm[xi+1]+yi], bb = _perm[_perm[xi+1]+yi+1];
    return _lerp_f(v,
        _lerp_f(u, _grad(aa, xf, yf), _grad(ba, xf-1, yf)),
        _lerp_f(u, _grad(ab, xf, yf-1), _grad(bb, xf-1, yf-1)));
}

void ts_canvas_perlin(TS_Canvas *c, float scale, float intensity) {
    for (int y = 0; y < c->height; y++) {
        for (int x = 0; x < c->width; x++) {
            float n = ts_perlin_noise(x * scale, y * scale) * intensity;
            int v = _bl_clamp((int)((n + 1.0f) * 127.5f));
            c->pixels[y * c->width + x] = 0xFF000000 | (uint32_t)((v<<16)|(v<<8)|v);
        }
    }
}

/* ── Canvas tint (color overlay with opacity) ───────────────────── */
void ts_canvas_tint(TS_Canvas *c, uint32_t color, float opacity) {
    int tr = (color>>16)&0xFF, tg = (color>>8)&0xFF, tb = color&0xFF;
    float inv = 1.0f - opacity;
    for (int i = 0; i < c->width * c->height; i++) {
        uint32_t px = c->pixels[i];
        int r = (int)(((px>>16)&0xFF)*inv + tr*opacity);
        int g = (int)(((px>>8)&0xFF)*inv + tg*opacity);
        int b = (int)((px&0xFF)*inv + tb*opacity);
        c->pixels[i] = 0xFF000000 | (uint32_t)((_bl_clamp(r)<<16)|(_bl_clamp(g)<<8)|_bl_clamp(b));
    }
}

/* ── Motion blur ────────────────────────────────────────────────── */
void ts_canvas_blur_motion(TS_Canvas *c, int dx, int dy, int steps) {
    int w = c->width, h = c->height;
    uint32_t *orig = (uint32_t*)malloc((size_t)(w*h)*4);
    if (!orig) return;
    memcpy(orig, c->pixels, (size_t)(w*h)*4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int sr=0,sg=0,sb=0,cnt=0;
        for (int s = 0; s < steps; s++) {
            int nx = x + dx*s/steps, ny = y + dy*s/steps;
            if (nx>=0 && nx<w && ny>=0 && ny<h) {
                uint32_t px = orig[ny*w+nx];
                sr+=(px>>16)&0xFF; sg+=(px>>8)&0xFF; sb+=px&0xFF; cnt++;
            }
        }
        if (cnt > 0)
            c->pixels[y*w+x] = 0xFF000000|(uint32_t)(((sr/cnt)<<16)|((sg/cnt)<<8)|(sb/cnt));
    }
    free(orig);
}

/* ── Stipple pattern fill ───────────────────────────────────────── */
void ts_draw_stipple_rect(TS_Canvas *c, int x, int y, int w, int h,
                            int density, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            if (((dx + dy * 7) * 31337) % 100 < density) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < c->width && py >= 0 && py < c->height)
                    c->pixels[py * c->width + px] = color;
            }
        }
    }
}

/* ── Lens distortion (barrel/pincushion) ────────────────────────── */
void ts_canvas_lens_distort(TS_Canvas *c, float k1, float k2) {
    int w = c->width, h = c->height;
    float cx = w / 2.0f, cy = h / 2.0f;
    float maxr = sqrtf(cx*cx + cy*cy);
    uint32_t *orig = (uint32_t*)malloc((size_t)(w*h)*4);
    if (!orig) return;
    memcpy(orig, c->pixels, (size_t)(w*h)*4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        float dx = (x - cx) / maxr, dy = (y - cy) / maxr;
        float r2 = dx*dx + dy*dy;
        float factor = 1.0f + k1*r2 + k2*r2*r2;
        int sx = (int)(cx + dx * factor * maxr);
        int sy = (int)(cy + dy * factor * maxr);
        if (sx >= 0 && sx < w && sy >= 0 && sy < h)
            c->pixels[y*w+x] = orig[sy*w+sx];
        else
            c->pixels[y*w+x] = 0xFF000000;
    }
    free(orig);
}

/* ── Color quantize (reduce to N colors, median cut style) ──────── */
void ts_canvas_quantize(TS_Canvas *c, int num_colors) {
    if (num_colors < 2) return;
    int n = c->width * c->height;
    int bits = 0, nc = num_colors;
    while (nc > 1) { nc >>= 1; bits++; }
    int mask = ~((1 << (8 - bits)) - 1);
    for (int i = 0; i < n; i++) {
        uint32_t px = c->pixels[i];
        int r = ((px>>16)&0xFF) & mask;
        int g = ((px>>8)&0xFF) & mask;
        int b = (px&0xFF) & mask;
        c->pixels[i] = 0xFF000000 | (uint32_t)((r<<16)|(g<<8)|b);
    }
}

/* ── Draw ellipse (Bresenham) ───────────────────────────────────── */
void ts_draw_ellipse_bresenham(TS_Canvas *c, int cx, int cy, int rx, int ry) {
    int x = 0, y = ry;
    long rx2 = (long)rx*rx, ry2 = (long)ry*ry;
    long px = 0, py = 2*rx2*y;
    long p = ry2 - rx2*ry + rx2/4;
    while (px < py) {
        ts_pixel_set(c, cx+x, cy+y, c->fill_color);
        ts_pixel_set(c, cx-x, cy+y, c->fill_color);
        ts_pixel_set(c, cx+x, cy-y, c->fill_color);
        ts_pixel_set(c, cx-x, cy-y, c->fill_color);
        x++; px += 2*ry2;
        p += ry2 + px;
        if (p >= 0) { y--; py -= 2*rx2; p -= py; }
    }
    p = ry2*(x*x+x) + rx2*(y-1)*(y-1) - rx2*ry2;
    while (y >= 0) {
        ts_pixel_set(c, cx+x, cy+y, c->fill_color);
        ts_pixel_set(c, cx-x, cy+y, c->fill_color);
        ts_pixel_set(c, cx+x, cy-y, c->fill_color);
        ts_pixel_set(c, cx-x, cy-y, c->fill_color);
        y--; py -= 2*rx2;
        p += rx2 - py;
        if (p <= 0) { x++; px += 2*ry2; p += px; }
    }
}

/* ── Draw ellipse filled ────────────────────────────────────────── */
void ts_draw_ellipse_filled_fast(TS_Canvas *c, int cx, int cy, int rx, int ry) {
    for (int y = -ry; y <= ry; y++) {
        int xw = (int)(rx * sqrtf(1.0f - (float)(y*y)/(float)(ry*ry)));
        for (int x = -xw; x <= xw; x++) {
            int px = cx + x, py = cy + y;
            if (px >= 0 && px < c->width && py >= 0 && py < c->height)
                c->pixels[py * c->width + px] = c->fill_color;
        }
    }
}
