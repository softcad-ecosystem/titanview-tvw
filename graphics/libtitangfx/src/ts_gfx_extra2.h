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
/* ts_gfx_extra2.h — extensii grafică pentru paritate cu pattern _alpha existent.
 *
 * Adaugă variante _alpha pentru primitivele unde lipseau (polygon, polyline, ring).
 * Pattern-ul din engine: variantele _alpha primesc culoarea direct ca argument,
 * vs variantele care folosesc c->fill_color (state machine). Pattern util pentru
 * apel direct fără setteri.
 */
#ifndef TS_GFX_EXTRA2_H
#define TS_GFX_EXTRA2_H
#include <stdint.h>
#include "ts_gfx_canvas.h"   /* TS_Canvas typedef complet */

#ifdef _WIN32
  #ifdef TS_BUILDING
    #define TS_GFX_X2_API __declspec(dllexport)
  #else
    #define TS_GFX_X2_API __declspec(dllimport)
  #endif
#else
  #define TS_GFX_X2_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Variante _alpha pentru polygon (paritate cu rect_filled_alpha / circle_filled_alpha).
 * pts[] interleaved: [x0,y0, x1,y1, ...], npts = nr de PUNCTE (nu de floats). */
TS_GFX_X2_API void ts_draw_polygon_filled_alpha(TS_Canvas *c,
                                                  const float *pts, int npts,
                                                  uint32_t color);
TS_GFX_X2_API void ts_draw_polygon_stroke_alpha(TS_Canvas *c,
                                                  const float *pts, int npts,
                                                  uint32_t color);

#ifdef __cplusplus
}
#endif
#endif
