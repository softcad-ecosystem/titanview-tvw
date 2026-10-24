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
/* ts_rs.h — Reed-Solomon error correction (255, 223) peste GF(2^8).
 *
 * Standard: RS(n=255, k=223), 32 octeți paritate per bloc, vindecă pînă la 16
 * erori per bloc. Overhead ~14% (vs triplicare 200%).
 *
 * GF(2^8) cu polinom primitiv 0x11D (x^8 + x^4 + x^3 + x^2 + 1), α = 2.
 * Polinom generator g(x) = ∏(x - α^i) pentru i ∈ [0, 31].
 *
 * Public API:
 *   ts_rs_encode_block — un bloc de 223 octeți → codeword de 255 octeți
 *   ts_rs_decode_block — codeword de 255 → 223 (vindecă, returnează #erori sau -1)
 *   ts_rs_encode       — buffer arbitrar → codewords concatenate
 *   ts_rs_decode       — codewords → buffer original (vindecat)
 *   ts_rs_self_test    — verifică encode→corupere→decode round-trip
 */
#ifndef TS_RS_H
#define TS_RS_H
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
  #ifdef TS_BUILDING
    #define TS_RS_API __declspec(dllexport)
  #else
    #define TS_RS_API __declspec(dllimport)
  #endif
#else
  #define TS_RS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TS_RS_N 255
#define TS_RS_K 223
#define TS_RS_NSYM 32
#define TS_RS_T 16            /* corectează pînă la t = nsym/2 erori per bloc */

/* Un bloc: 223 octeți date → 255 octeți codeword (data || 32 paritate). */
TS_RS_API int ts_rs_encode_block(const uint8_t data[TS_RS_K], uint8_t codeword[TS_RS_N]);

/* Decodează un codeword (poate fi corupt). Pune datele în out_data.
 * Returnează numărul de erori corectate (0..16), sau -1 dacă irecuperabil. */
TS_RS_API int ts_rs_decode_block(const uint8_t codeword[TS_RS_N], uint8_t out_data[TS_RS_K]);

/* Encodează un buffer arbitrar. out trebuie să aibă cel puțin n_blocks*255 octeți,
 * unde n_blocks = ceil(data_len / 223). Padding cu zerouri la ultimul bloc.
 * Returnează numărul total de octeți scriși în out (n_blocks * 255), sau -1. */
TS_RS_API int ts_rs_encode(const uint8_t* data, size_t data_len,
                            uint8_t* out, size_t out_capacity, size_t* out_len);

/* Decodează buffer concatenat de codewords. Pune n_blocks*223 octeți în out_data.
 * Returnează numărul TOTAL de erori corectate, sau -1 dacă vreun bloc irecuperabil. */
TS_RS_API int ts_rs_decode(const uint8_t* codewords, size_t codewords_len,
                            uint8_t* out_data, size_t out_capacity, size_t* out_len);

/* Self-test intern: encode aleator → corupe k erori → decode → verifică egalitate.
 * Returnează 0 dacă toate testele trec, -1 la prima eșuare. Pentru cyber-fizical
 * test la build / la încărcare DLL. */
TS_RS_API int ts_rs_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
