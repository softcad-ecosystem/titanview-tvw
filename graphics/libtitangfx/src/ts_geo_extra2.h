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
/* ts_geo_extra2.h — EXTENSIE pentru ts_engines.so. Adaugă piesele care chiar
 * LIPSESC (verificat prin nm — 0 simboluri pentru fiecare concept):
 *   ts_geo_canonical_ring  ts_geo_ring_hash         ts_geo_plan_signature
 *   ts_merkle_root         ts_merkle_proof          ts_merkle_verify
 *   ts_hex_cart_to_axial   ts_hex_axial_to_cart     ts_hex_vertices
 *   ts_hex_polygon_coverage
 *
 * Folosește funcții EXISTENTE din engine:
 *   ts_geo_ring_area, ts_geo_polygon_area, ts_geo_point_in_ring,
 *   ts_sha256_init/update/final  (din ts_pdf_real/ts_crypto_sha256.c)
 *
 * Pentru amalgamare în ts_engines_all.c — adaugă `#include "ts_geo_extra2.c"`
 * la sfîrșit. NU duplică nimic; doar extinde familia ts_geo_* și adaugă două
 * familii noi: ts_merkle_* și ts_hex_*.
 *
 * Concept: Crinel Popa (Geo-Sign + TopoHex-ZKP).
 */
#ifndef TS_GEO_EXTRA2_H
#define TS_GEO_EXTRA2_H
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
  #ifdef TS_BUILDING
    #define TS_API __declspec(dllexport)
  #else
    #define TS_API __declspec(dllimport)
  #endif
#else
  #define TS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Familia ts_geo_* (extensie) ─────────────────────────────────────── */

/* Forma canonică: cuantifică la μm (int64), orientează CCW, rotește la lex-min.
 * Două inele identice geometric → secvențe identice. */
TS_API int ts_geo_canonical_ring(
    const double* xs, const double* ys, int n,
    int64_t* out_xs_um, int64_t* out_ys_um);

/* SHA-256 al formei canonice (32 bytes). Folosește ts_sha256 din engine. */
TS_API int ts_geo_ring_hash(
    const double* xs, const double* ys, int n,
    uint8_t out_hash[32]);

/* Semnătură plan: hash inel per parcelă + Merkle root sortat. */
TS_API int ts_geo_plan_signature(
    const double* xs, const double* ys,
    const int* ring_sizes, int n_rings,
    uint8_t out_root[32]);

/* ── Familia ts_merkle_* (NOUĂ) ──────────────────────────────────────── */

/* Merkle root pe frunze sortate (fiecare 32 bytes). */
TS_API void ts_merkle_root(
    const uint8_t* leaves, int n_leaves, uint8_t out_root[32]);

/* Proof de apartenență. Returnează lungime proof (nr. sibling-uri).
 * out_proof: alocat de caller la (ceil(log2(n_leaves)) + 1) * 32 bytes.
 * out_sides: 0=left, 1=right. -1 dacă target nu-i în mulțime. */
TS_API int ts_merkle_proof(
    const uint8_t* leaves, int n_leaves,
    const uint8_t target[32],
    uint8_t* out_proof, uint8_t* out_sides);

/* Verifică proof de apartenență. Returnează 1=valid, 0=invalid. */
TS_API int ts_merkle_verify(
    const uint8_t target[32],
    const uint8_t* proof, const uint8_t* sides, int proof_len,
    const uint8_t root[32]);

/* ── Familia ts_hex_* (NOUĂ) ─────────────────────────────────────────── */

/* Pointy-top, axial (q, r). hex_size = circumradius. */
TS_API void ts_hex_cart_to_axial(double x, double y, double hex_size,
                                  int32_t* q, int32_t* r);
TS_API void ts_hex_axial_to_cart(int32_t q, int32_t r, double hex_size,
                                  double* x, double* y);
TS_API void ts_hex_vertices(int32_t q, int32_t r, double hex_size,
                             double* out_xs6, double* out_ys6);

/* Coverage EXACTĂ (Sutherland-Hodgman per hex). Returnează nr. hexe.
 * out_q, out_r: pre-alocate la max_hexes. */
TS_API int ts_hex_polygon_coverage(
    const double* poly_xs, const double* poly_ys, int n,
    double hex_size,
    int32_t* out_q, int32_t* out_r, int max_hexes);

#ifdef __cplusplus
}
#endif
#endif
