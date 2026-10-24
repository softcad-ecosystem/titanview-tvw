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
/*
 * ts_outbuf.h — scriere sigură în bufferul apelantului (4.66).
 *
 * Tiparul `n += snprintf(out + n, cap - n, ...)` e corect doar cât textul
 * încape: la prima trunchiere n trece de cap, `cap - n` devine negativ,
 * convertit la size_t = uriaș, iar următorul snprintf scrie DINCOLO de buffer.
 * Așa murea procesul (malloc(): invalid size) la dezmembrarea unei parcele cu
 * 300 de vârfuri. ts__catf păstrează aritmetica (n crește cu lungimea
 * completă, ca să se poată detecta trunchierea: n >= cap), dar nu scrie
 * niciodată în afara bufferului.
 */
#ifndef TS_OUTBUF_H
#define TS_OUTBUF_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

static inline int ts__catf(char *out, size_t cap, size_t pos, const char *fmt, ...)
    __attribute__((__format__(__printf__, 4, 5), __unused__));

static inline int ts__catf(char *out, size_t cap, size_t pos, const char *fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    if (out && pos < cap)
        r = vsnprintf(out + pos, cap - pos, fmt, ap);
    else
        r = vsnprintf(NULL, 0, fmt, ap);          /* doar măsoară */
    va_end(ap);
    return r < 0 ? 0 : r;
}

/* trunchiat? (n = lungimea cerută, cap = capacitatea bufferului) */
#define TS_OUT_TRUNCATED(n, cap) ((size_t)(n) >= (size_t)(cap))

/* O geometrie care nu încape NU se întoarce ciuntită (ar arăta validă):
 * bufferul se golește și rezultatul e -1 — apelantul mărește bufferul. */
static inline int ts__out_fail(char *out, size_t cap) __attribute__((__unused__));
static inline int ts__out_fail(char *out, size_t cap) {
    if (out && cap) out[0] = 0;
    return -1;
}
/* snprintf pentru un rezultat întreg: lungimea, sau -1 (buffer golit) dacă nu încape */
static inline int ts__out_snprintf(char *out, size_t cap, const char *fmt, ...)
    __attribute__((__format__(__printf__, 3, 4), __unused__));
static inline int ts__out_snprintf(char *out, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    if (r < 0) r = 0;
    return TS_OUT_TRUNCATED(r, cap) ? ts__out_fail(out, cap) : r;
}

#define TS_OUT_RESULT(out, n, cap) (TS_OUT_TRUNCATED((n), (cap)) ? ts__out_fail((out), (size_t)(cap)) : (n))

#endif
