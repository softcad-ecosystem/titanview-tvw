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
/* ts_gfx_extra2.c — variantele _alpha lipsă pentru polygon.
 * Save/restore fill_color/stroke_color pe canvas, apoi delegă la varianta de bază.
 */
#define TS_BUILDING
#include "ts_gfx_extra2.h"
#include "ts_gfx_canvas.h"   /* TS_Canvas complet */

/* Forward decls — funcțiile de bază sunt în ts_gfx_draw.h (amalgamat) */
void ts_draw_polygon_filled(TS_Canvas *c, const float *pts, int npts);
void ts_draw_polygon(TS_Canvas *c, const float *pts, int npts);

void ts_draw_polygon_filled_alpha(TS_Canvas *c, const float *pts, int npts,
                                    uint32_t color) {
    if (!c || !pts || npts < 3) return;
    uint32_t saved = c->fill_color;
    c->fill_color = color;
    ts_draw_polygon_filled(c, pts, npts);
    c->fill_color = saved;
}

void ts_draw_polygon_stroke_alpha(TS_Canvas *c, const float *pts, int npts,
                                    uint32_t color) {
    if (!c || !pts || npts < 2) return;
    uint32_t saved = c->stroke_color;
    c->stroke_color = color;
    ts_draw_polygon(c, pts, npts);
    c->stroke_color = saved;
}
