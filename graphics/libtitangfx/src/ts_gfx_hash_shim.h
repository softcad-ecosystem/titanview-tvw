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
/* ts_gfx_hash_shim.h — hashing generic (SHA-256) pentru ediția grafică deschisă.
   SHA-256 = algoritm public (FIPS 180-4), NU IP SoftCAD. Reutilizează nucleul pur
   ts_crypto_sha256.c (init/update/final/ts_sha256) + wrapperele hex + file.
   Fără AES / RC4 / PDF / cripto grea. Inclus devreme, înaintea modulelor care-l cheamă. */
#ifndef TS_GFX_HASH_SHIM_H
#define TS_GFX_HASH_SHIM_H
#include <stdio.h>
#include <string.h>
#include "ts_crypto_sha256.c"   /* ts_sha256_init/update/final + ts_sha256 (pur) */

/* bytes[32] → hex[65] (null-terminated) */
void ts_sha256_hex(const unsigned char hash[32], char hex[65]) {
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { hex[i*2] = H[hash[i] >> 4]; hex[i*2+1] = H[hash[i] & 0x0f]; }
    hex[64] = '\0';
}

/* SHA-256 al unui fișier (streaming) */
int ts_sha256_file(const char *path, unsigned char hash[32]) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    ts_sha256_ctx ctx; ts_sha256_init(&ctx);
    unsigned char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) ts_sha256_update(&ctx, buf, (long)n);
    fclose(f);
    ts_sha256_final(&ctx, hash);
    return 1;
}

/* octeți aleatori — generic (UUID/jitter). /dev/urandom cu fallback. NU cripto-tier. */
#include <stdlib.h>
int ts_random_bytes(void *buf, int len) {
    if (!buf || len <= 0) return 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) { size_t n = fread(buf, 1, (size_t)len, f); fclose(f); if ((int)n == len) return 1; }
    unsigned char *p = (unsigned char*)buf;
    for (int i = 0; i < len; i++) p[i] = (unsigned char)(rand() & 0xff);
    return 1;
}
#endif /* TS_GFX_HASH_SHIM_H */
