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
/* ts_geom.h — motor geometric C99 pentru SoftCAD.
 *
 * Adaugă MUȘCHI la stiva geo-crypto: poligon-hex coverage EXACTĂ (Sutherland-
 * Hodgman, nu sampling), arie exactă pe int64, point-in-polygon vectorizat.
 * Înlocuiește `topohex_zkp.parcel_hex_coverage` care era O(bbox/step²) sampling
 * cu O(bbox/hex²) clipping exact.
 *
 * Stil ctypes-friendly identic cu ts_tcp.h. Zero dependențe (math.h).
 * Concept: Crinel Popa (SoftCAD).
 */
#ifndef TS_GEOM_H
#define TS_GEOM_H
#include <stdint.h>

#ifdef _WIN32
  #ifdef TS_GEOM_BUILDING
    #define TS_GEOM_API __declspec(dllexport)
  #else
    #define TS_GEOM_API __declspec(dllimport)
  #endif
#else
  #define TS_GEOM_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Arie semnată exactă pe coord cuantificate la μm (int64) ─────────── */
TS_GEOM_API int64_t ts_geom_signed_area_i64(
    const int64_t* xs, const int64_t* ys, int n);

/* ── Arie absolută în coord double ────────────────────────────────────── */
TS_GEOM_API double ts_geom_polygon_area(
    const double* xs, const double* ys, int n);

/* ── Point-in-polygon (ray cast). 1=interior, 0=exterior ──────────────── */
TS_GEOM_API int ts_geom_pip(const double* xs, const double* ys, int n,
                            double px, double py);

/* ── Sutherland-Hodgman: clip poligon (subject) cu poligon convex (clipper).
 * Clipper-ul TREBUIE să fie CCW. out_xs/out_ys: buffer alocat de caller. 
 * Returnează nr. vertexi în rezultat, 0 dacă goale, -1 la eroare alocare. */
TS_GEOM_API int ts_geom_clip_convex(
    const double* sub_xs, const double* sub_ys, int n_sub,
    const double* clip_xs, const double* clip_ys, int n_clip,
    double* out_xs, double* out_ys, int max_out);

/* ── Acoperire poligon ↔ rețea hexagonală (pointy-top) ────────────────── */
/* Pentru fiecare hex care intersectează poligonul cu arie > eps, scrie (q,r).
 * Returnează nr. hexe găsite. */
TS_GEOM_API int ts_geom_polygon_hex_coverage(
    const double* poly_xs, const double* poly_ys, int n,
    double hex_size,
    int32_t* out_q, int32_t* out_r, int max_hexes);

TS_GEOM_API const char* ts_geom_version(void);

#ifdef __cplusplus
}
#endif
#endif
