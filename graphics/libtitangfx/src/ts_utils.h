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
/* ── ts_utils.h — Universal utilities every developer needs ───────
 * String ops, math, date/time, file I/O, JSON, logging.
 * C99, zero dependencies. SoftCAD C+C TOPO © 2026
 */

#include "ts_outbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

/* ═══════════════════════════════════════════════════════════════════ */
/*  STRING UTILITIES                                                   */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_str_split(const char *str, char delim, char **parts, int max_parts) {
    int count = 0;
    const char *p = str;
    while (*p && count < max_parts) {
        const char *start = p;
        while (*p && *p != delim) p++;
        int len = (int)(p - start);
        parts[count] = (char *)malloc((size_t)(len + 1));
        if (parts[count]) { memcpy(parts[count], start, (size_t)len); parts[count][len] = 0; }
        count++;
        if (*p) p++;
    }
    return count;
}

int ts_str_replace(const char *str, const char *old, const char *rep, char *out, int cap) {
    int n = 0, olen = (int)strlen(old), rlen = (int)strlen(rep);
    if (olen == 0) { strncpy(out, str, (size_t)(cap-1)); out[cap-1]=0; return (int)strlen(out); }
    while (*str && n < cap - rlen - 1) {
        if (strncmp(str, old, (size_t)olen) == 0) {
            memcpy(out + n, rep, (size_t)rlen); n += rlen; str += olen;
        } else { out[n++] = *str++; }
    }
    out[n] = 0; return n;
}

void ts_str_upper(char *str) { for (; *str; str++) *str = (char)toupper((unsigned char)*str); }
void ts_str_lower(char *str) { for (; *str; str++) *str = (char)tolower((unsigned char)*str); }

void ts_str_trim(char *str) {
    char *s = str;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (s != str) memmove(str, s, strlen(s) + 1);
    int l = (int)strlen(str);
    while (l > 0 && (str[l-1]==' '||str[l-1]=='\t'||str[l-1]=='\n'||str[l-1]=='\r')) str[--l]=0;
}

int ts_str_starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int ts_str_ends_with(const char *str, const char *suffix) {
    int sl = (int)strlen(str), xl = (int)strlen(suffix);
    return sl >= xl && strcmp(str + sl - xl, suffix) == 0;
}

int ts_str_contains(const char *str, const char *sub) { return strstr(str, sub) != NULL; }

int ts_str_count(const char *str, const char *sub) {
    int c = 0, sl = (int)strlen(sub);
    if (sl == 0) return 0;
    const char *p = str;
    while ((p = strstr(p, sub))) { c++; p += sl; }
    return c;
}

int ts_str_join(const char **parts, int n, const char *delim, char *out, int cap) {
    int pos = 0, dl = (int)strlen(delim);
    for (int i = 0; i < n && pos < cap - 1; i++) {
        if (i > 0 && pos + dl < cap) { memcpy(out+pos, delim, (size_t)dl); pos += dl; }
        int pl = (int)strlen(parts[i]);
        if (pos + pl < cap) { memcpy(out+pos, parts[i], (size_t)pl); pos += pl; }
    }
    out[pos] = 0; return pos;
}

int ts_str_repeat(const char *str, int n, char *out, int cap) {
    int sl = (int)strlen(str), pos = 0;
    for (int i = 0; i < n && pos + sl < cap; i++) { memcpy(out+pos, str, (size_t)sl); pos += sl; }
    out[pos] = 0; return pos;
}

int ts_url_encode(const char *str, char *out, int cap) {
    int n = 0;
    for (; *str && n < cap - 4; str++) {
        if (isalnum((unsigned char)*str) || *str=='-' || *str=='_' || *str=='.' || *str=='~')
            out[n++] = *str;
        else n += ts__catf(out, cap, n, "%%%02X", (unsigned char)*str);
    }
    out[n] = 0; return n;
}

int ts_url_decode(const char *str, char *out, int cap) {
    int n = 0;
    for (; *str && n < cap - 1; str++) {
        if (*str == '%' && str[1] && str[2]) {
            unsigned int ch; sscanf(str+1, "%02x", &ch); out[n++] = (char)ch; str += 2;
        } else if (*str == '+') out[n++] = ' ';
        else out[n++] = *str;
    }
    out[n] = 0; return n;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  MATH UTILITIES                                                     */
/* ═══════════════════════════════════════════════════════════════════ */

double ts_math_lerp(double a, double b, double t) { return a + (b - a) * t; }
double ts_math_clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
double ts_math_map(double v, double in_lo, double in_hi, double out_lo, double out_hi) {
    return out_lo + (v - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}
double ts_math_deg2rad(double deg) { return deg * M_PI / 180.0; }
double ts_math_rad2deg(double rad) { return rad * 180.0 / M_PI; }
double ts_math_distance_2d(double x1, double y1, double x2, double y2) {
    return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}
double ts_math_distance_3d(double x1,double y1,double z1,double x2,double y2,double z2) {
    return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
}

void ts_math_linear_regression(const double *x, const double *y, int n,
                                 double *slope, double *intercept) {
    double sx=0,sy=0,sxy=0,sx2=0;
    for (int i=0;i<n;i++) { sx+=x[i]; sy+=y[i]; sxy+=x[i]*y[i]; sx2+=x[i]*x[i]; }
    double denom = n*sx2 - sx*sx;
    if (fabs(denom) < 1e-12) { *slope=0; *intercept=sy/n; return; }
    *slope = (n*sxy - sx*sy) / denom;
    *intercept = (sy - *slope * sx) / n;
}

void ts_math_moving_average(const double *vals, int n, int window, double *out) {
    for (int i = 0; i < n; i++) {
        double sum = 0; int cnt = 0;
        for (int j = i - window/2; j <= i + window/2; j++) {
            if (j >= 0 && j < n) { sum += vals[j]; cnt++; }
        }
        out[i] = cnt > 0 ? sum / cnt : 0;
    }
}

double ts_math_interpolate(const double *x, const double *y, int n, double at_x) {
    if (n < 2) return n > 0 ? y[0] : 0;
    if (at_x <= x[0]) return y[0];
    if (at_x >= x[n-1]) return y[n-1];
    for (int i = 0; i < n - 1; i++) {
        if (at_x >= x[i] && at_x <= x[i+1]) {
            double t = (at_x - x[i]) / (x[i+1] - x[i]);
            return y[i] + t * (y[i+1] - y[i]);
        }
    }
    return y[n-1];
}

double ts_math_percentile(const double *vals, int n, double p) {
    if (n <= 0) return 0;
    double *sorted = (double*)malloc((size_t)n*sizeof(double));
    if (!sorted) return 0;
    memcpy(sorted, vals, (size_t)n*sizeof(double));
    for (int i=0;i<n-1;i++) for (int j=i+1;j<n;j++)
        if (sorted[j]<sorted[i]) { double t=sorted[i]; sorted[i]=sorted[j]; sorted[j]=t; }
    double idx = p/100.0 * (n-1);
    int lo = (int)idx; int hi = lo + 1 < n ? lo + 1 : lo;
    double frac = idx - lo;
    double result = sorted[lo] + frac * (sorted[hi] - sorted[lo]);
    free(sorted); return result;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  DATE / TIME                                                        */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_time_now_iso(char *out, int cap) {
    time_t t = time(NULL); struct tm *tm = localtime(&t);
    return (int)strftime(out, (size_t)cap, "%Y-%m-%dT%H:%M:%S", tm);
}

long ts_time_now_unix(void) { return (long)time(NULL); }

int ts_time_format(long unix_time, const char *fmt, char *out, int cap) {
    time_t t = (time_t)unix_time; struct tm *tm = localtime(&t);
    return (int)strftime(out, (size_t)cap, fmt, tm);
}

long ts_time_parse_iso(const char *iso) {
    struct tm tm = {0};
    sscanf(iso, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900; tm.tm_mon -= 1;
    return (long)mktime(&tm);
}

double ts_time_diff_seconds(const char *iso_a, const char *iso_b) {
    return difftime((time_t)ts_time_parse_iso(iso_b), (time_t)ts_time_parse_iso(iso_a));
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  FILE OPERATIONS                                                    */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_file_exists(const char *path) {
    struct stat st; return stat(path, &st) == 0;
}

/* ts_file_size already in ts_titan_hash.h */

int ts_file_read_all(const char *path, char *buf, int cap) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    int n = (int)fread(buf, 1, (size_t)(cap-1), f); buf[n] = 0;
    fclose(f); return n;
}

int ts_file_write_all(const char *path, const char *data, int len) {
    FILE *f = fopen(path, "wb"); if (!f) return 0;
    int n = (int)fwrite(data, 1, (size_t)len, f); fclose(f); return n;
}

const char *ts_file_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot ? dot : "";
}

const char *ts_file_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    return slash ? slash + 1 : path;
}

int ts_file_copy(const char *src, const char *dst) {
    FILE *fs = fopen(src, "rb"); if (!fs) return 0;
    FILE *fd = fopen(dst, "wb"); if (!fd) { fclose(fs); return 0; }
    size_t n;
    char *buf = (char*)malloc(65536);
    if (!buf) return 0;
    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) fwrite(buf, 1, n, fd);
    fclose(fs); fclose(fd); return 1;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  JSON (lightweight key-value lookup — no full parser)               */
/* ═══════════════════════════════════════════════════════════════════ */

int ts_json_get_string(const char *json, const char *key, char *out, int cap) {
    char pattern[128]; snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return 0;
    p++;
    int n = 0;
    while (*p && *p != '"' && n < cap - 1) {
        if (*p == '\\' && p[1]) { p++; } /* skip escape */
        out[n++] = *p++;
    }
    out[n] = 0; return n;
}

double ts_json_get_number(const char *json, const char *key) {
    char pattern[128]; snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    return atof(p);
}

int ts_json_get_bool(const char *json, const char *key) {
    char pattern[128]; snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    return (*p == 't' || *p == 'T' || *p == '1');
}

int ts_json_build(const char **keys, const char **vals, int n, char *out, int cap) {
    int pos = snprintf(out, (size_t)cap, "{");
    for (int i = 0; i < n && pos < cap - 20; i++) {
        if (i > 0) pos += ts__catf(out, cap, pos, ", ");
        pos += ts__catf(out, cap, pos, "\"%s\": \"%s\"", keys[i], vals[i]);
    }
    pos += ts__catf(out, cap, pos, "}");
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  LOGGING                                                            */
/* ═══════════════════════════════════════════════════════════════════ */

static FILE *_ts_log_file = NULL;
static int _ts_log_level = 0; /* 0=debug, 1=info, 2=warn, 3=error */

void ts_log_init(const char *path, int level) {
    if (_ts_log_file && _ts_log_file != stderr) fclose(_ts_log_file);
    _ts_log_file = path ? fopen(path, "a") : stderr;
    _ts_log_level = level;
}

static void _ts_log(int level, const char *prefix, const char *msg) {
    if (level < _ts_log_level) return;
    FILE *f = _ts_log_file ? _ts_log_file : stderr;
    time_t t = time(NULL); struct tm *tm = localtime(&t);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(f, "[%s] %s: %s\n", ts, prefix, msg);
    fflush(f);
}

void ts_log_debug(const char *msg) { _ts_log(0, "DEBUG", msg); }
void ts_log_info(const char *msg)  { _ts_log(1, "INFO", msg); }
void ts_log_warn(const char *msg)  { _ts_log(2, "WARN", msg); }
void ts_log_error(const char *msg) { _ts_log(3, "ERROR", msg); }

void ts_log_close(void) {
    if (_ts_log_file && _ts_log_file != stderr) fclose(_ts_log_file);
    _ts_log_file = NULL;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  TIMER (high-resolution, for profiling)                             */
/* ═══════════════════════════════════════════════════════════════════ */

typedef struct { struct timespec start; } TS_Timer;

void ts_timer_start(TS_Timer *t) { clock_gettime(CLOCK_MONOTONIC, &t->start); }

double ts_timer_elapsed_ms(const TS_Timer *t) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - t->start.tv_sec) * 1000.0 +
           (now.tv_nsec - t->start.tv_nsec) / 1000000.0;
}

double ts_timer_elapsed_us(const TS_Timer *t) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - t->start.tv_sec) * 1000000.0 +
           (now.tv_nsec - t->start.tv_nsec) / 1000.0;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  ADDITIONAL UTILS: UUID, slug, number format, roman numerals       */
/* ═══════════════════════════════════════════════════════════════════ */

/* Generate UUID v4 (random) — format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx */
static unsigned int _uuid_counter = 0;
int ts_random_bytes(void *buf, int len);   /* ts_titan_crypto.h */

int ts_uuid_v4(char *out, int cap) {
    if (cap < 37) return 0;
    unsigned char bytes[16];
    /* High-entropy: nanosecond clock + counter + golden ratio hash */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    _uuid_counter += 1;
    /* 4.66: sursa OS prin ts_random_bytes (BCryptGenRandom pe Windows —
       înainte fopen("/dev/urandom") eșua acolo și rămânea DOAR splitmix din
       ceas; fread-ul nu era verificat). Splitmix rămâne doar ca rezervă. */
    if (ts_random_bytes(bytes, 16)) {
        /* ok */
    } else {
        /* Splitmix64 fallback */
        uint64_t h = ns ^ (_uuid_counter * 0x9E3779B97F4A7C15ULL);
        for (int i = 0; i < 16; i++) {
            h ^= h >> 17; h *= 0xBF58476D1CE4E5B9ULL; h ^= h >> 31;
            bytes[i] = (unsigned char)(h >> (i * 4));
            h += ns + _uuid_counter + (uint64_t)i;
        }
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40; /* version 4 */
    bytes[8] = (bytes[8] & 0x3F) | 0x80; /* variant 1 */
    snprintf(out, (size_t)cap,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
        bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return 36;
}

/* Create URL-safe slug from text: "Hello World!" → "hello-world" */
int ts_str_slug(const char *text, char *out, int cap) {
    int n = 0;
    int last_was_dash = 1; /* prevent leading dash */
    for (const char *p = text; *p && n < cap - 1; p++) {
        if (isalnum((unsigned char)*p)) {
            out[n++] = (char)tolower((unsigned char)*p);
            last_was_dash = 0;
        } else if (!last_was_dash && (*p == ' ' || *p == '_' || *p == '-' || *p == '.')) {
            out[n++] = '-';
            last_was_dash = 1;
        }
    }
    /* Remove trailing dash */
    if (n > 0 && out[n-1] == '-') n--;
    out[n] = 0;
    return n;
}

/* Number to Roman numerals (1-3999) */
int ts_str_roman(int num, char *out, int cap) {
    if (num < 1 || num > 3999 || cap < 16) { out[0]=0; return 0; }
    static const int vals[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
    static const char *syms[] = {"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};
    int n = 0;
    for (int i = 0; i < 13 && num > 0; i++) {
        while (num >= vals[i] && n < cap - 3) {
            int sl = (int)strlen(syms[i]);
            memcpy(out+n, syms[i], (size_t)sl);
            n += sl; num -= vals[i];
        }
    }
    out[n] = 0; return n;
}

/* Format number with thousands separator: 1234567 → "1.234.567" (RO) or "1,234,567" (EN) */
int ts_str_format_number(double num, int decimals, int sep, int dec_point,
                           char *out, int cap) {
    if (!out || cap < 2) return 0;
    if (sep == 0) sep = '.';
    if (dec_point == 0) dec_point = ',';

    /* Format into temp buffer first — never overflow caller's buffer */
    char raw[64], tmp[128];
    snprintf(raw, sizeof(raw), "%.*f", decimals, num);
    char *dot = strchr(raw, '.');
    int int_len = dot ? (int)(dot - raw) : (int)strlen(raw);
    int n = 0;

    int start = (raw[0] == '-') ? 1 : 0;
    if (start) tmp[n++] = '-';
    int digits = int_len - start;
    for (int i = start; i < int_len && n < 120; i++) {
        tmp[n++] = raw[i];
        int remaining = int_len - i - 1;
        if (remaining > 0 && remaining % 3 == 0) tmp[n++] = (char)sep;
    }
    if (dot && decimals > 0) {
        tmp[n++] = (char)dec_point;
        for (int i = 0; i < decimals && dot[1+i] && n < 120; i++) tmp[n++] = dot[1+i];
    }
    tmp[n] = 0;

    /* Check if result fits in caller's buffer */
    if (n >= cap) {
        out[0] = 0;
        return 0; /* buffer too small — return 0, empty buffer */
    }
    memcpy(out, tmp, (size_t)(n + 1));
    return n;
}

/* Pad string left/right to width */
int ts_str_pad_left(const char *str, int width, char pad_char, char *out, int cap) {
    int sl = (int)strlen(str);
    int padding = width > sl ? width - sl : 0;
    int n = 0;
    for (int i = 0; i < padding && n < cap-1; i++) out[n++] = pad_char;
    for (int i = 0; i < sl && n < cap-1; i++) out[n++] = str[i];
    out[n] = 0; return n;
}

int ts_str_pad_right(const char *str, int width, char pad_char, char *out, int cap) {
    int sl = (int)strlen(str);
    int n = 0;
    for (int i = 0; i < sl && n < cap-1; i++) out[n++] = str[i];
    while (n < width && n < cap-1) out[n++] = pad_char;
    out[n] = 0; return n;
}

/* Truncate with ellipsis: "Hello World" → "Hello W..." */
int ts_str_truncate(const char *str, int max_len, char *out, int cap) {
    /* 4.66: max_len > cap scria dincolo de out; tăietura nu mai rupe un
       caracter UTF-8 în două (ă/ș/ț au 2 octeți → apărea „�”) */
    if (!out || cap <= 0) return 0;
    if (!str) { out[0] = 0; return 0; }
    if (max_len > cap - 1) max_len = cap - 1;
    int sl = (int)strlen(str);
    if (sl <= max_len || max_len < 4) {
        int n = sl < cap - 1 ? sl : cap - 1;
        while (n > 0 && n < sl && ((unsigned char)str[n] & 0xC0) == 0x80) n--;
        memcpy(out, str, (size_t)n); out[n] = 0;
        return n;
    }
    int n = max_len - 3;
    while (n > 0 && ((unsigned char)str[n] & 0xC0) == 0x80) n--;
    memcpy(out, str, (size_t)n);
    out[n++] = '.'; out[n++] = '.'; out[n++] = '.'; out[n] = 0;
    return n;
}


/* UUID v4 with dashes removed (32 hex chars) */
int ts_uuid_v4_hex(char *out, int cap) {
    char full[40];
    ts_uuid_v4(full, 40);
    int n = 0;
    for (int i = 0; full[i] && n < cap-1; i++)
        if (full[i] != '-') out[n++] = full[i];
    out[n] = 0;
    return n;
}


/* Python-friendly format_number (takes int separators instead of char) */
int ts_str_format_number_i(double num, int decimals, int sep, int dec_point,
                             char *out, int cap) {
    return ts_str_format_number(num, decimals, sep, dec_point, out, cap);
}
