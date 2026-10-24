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
/* ts_blake3 — BLAKE3 in engine, C99 pur, doar calea portabila.
 *
 * Sursa: implementarea oficiala BLAKE3-team (c/), vendorizata neatinsa in
 * blake3_vendored/. Dispatch-ul SIMD e taiat la compilare (portable only)
 * ca binarul sa ramana identic pe orice CPU si buildul dintr-o singura
 * unitate de translatie sa nu ceara nimic in plus.
 *
 * API in stilul casei:
 *   ts_blake3_hash(data, len, out32)        -> 0
 *   ts_blake3_hex(data, len, out_hex65)     -> 0 (65 = 64 hex + NUL)
 *   ts_blake3_stream_size()                 -> sizeof(blake3_hasher)
 *   ts_blake3_init(state) / _update(state, data, len) / _final(state, out32)
 *
 * Deblocat 2026-08-04: mingw-ul a intrat in container si regula
 * "niciun .so fara .dll" a putut fi respectata pentru prima data aici.
 */
#ifndef TS_BLAKE3_H
#define TS_BLAKE3_H

#define BLAKE3_NO_SSE2 1
#define BLAKE3_NO_SSE41 1
#define BLAKE3_NO_AVX2 1
#define BLAKE3_NO_AVX512 1
#define BLAKE3_USE_NEON 0

#include "blake3_vendored/blake3.c"
#include "blake3_vendored/blake3_dispatch.c"
#include "blake3_vendored/blake3_portable.c"

int ts_blake3_hash(const unsigned char *data, size_t len,
                   unsigned char out[32]) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data, len);
    blake3_hasher_finalize(&hasher, out, 32);
    return 0;
}

int ts_blake3_hex(const unsigned char *data, size_t len, char out[65]) {
    static const char digits[] = "0123456789abcdef";
    unsigned char raw[32];
    int i;
    ts_blake3_hash(data, len, raw);
    for (i = 0; i < 32; i++) {
        out[i * 2]     = digits[raw[i] >> 4];
        out[i * 2 + 1] = digits[raw[i] & 15];
    }
    out[64] = '\0';
    return 0;
}

size_t ts_blake3_stream_size(void) { return sizeof(blake3_hasher); }

int ts_blake3_init(void *state) {
    blake3_hasher_init((blake3_hasher *)state);
    return 0;
}

int ts_blake3_update(void *state, const unsigned char *data, size_t len) {
    blake3_hasher_update((blake3_hasher *)state, data, len);
    return 0;
}

int ts_blake3_final(void *state, unsigned char out[32]) {
    blake3_hasher_finalize((blake3_hasher *)state, out, 32);
    return 0;
}

#endif /* TS_BLAKE3_H */
