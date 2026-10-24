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
/* ts_crc32c — CRC-32C (Castagnoli, poli 0x1EDC6F41 reflectat = 0x82F63B78).
 *
 * TCB il foloseste pentru sumele de sectiune; varianta pur-Python costa
 * ~146ms pe o scriere Lopadea. Tabel generat la prima chemare, C99 pur.
 * Ancora canonica: ts_crc32c("123456789", 9, 0) == 0xE3069283.
 */
#ifndef TS_CRC32C_H
#define TS_CRC32C_H

#include <stddef.h>
#include <stdint.h>

static uint32_t ts__crc32c_table[256];
static int ts__crc32c_ready = 0;

static void ts__crc32c_build(void) {
    uint32_t i, j, value;
    for (i = 0; i < 256; i++) {
        value = i;
        for (j = 0; j < 8; j++)
            value = (value & 1) ? (value >> 1) ^ 0x82F63B78u : value >> 1;
        ts__crc32c_table[i] = value;
    }
    ts__crc32c_ready = 1;
}

uint32_t ts_crc32c(const unsigned char *data, size_t len, uint32_t seed) {
    uint32_t crc = ~seed;
    size_t i;
    if (!ts__crc32c_ready)
        ts__crc32c_build();
    for (i = 0; i < len; i++)
        crc = (crc >> 8) ^ ts__crc32c_table[(crc ^ data[i]) & 0xFF];
    return ~crc;
}

#endif /* TS_CRC32C_H */
