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
#define TS_GFX_ONLY 1
/* _GNU_SOURCE pentru memmem (folosit în ts_pdf_doctor.h, fix v2.8.6 Crinel) */
#define _GNU_SOURCE 1

/*
 * ts_engines_all.c — Single compilation unit for TitanScript Engine Suite v2.2
 * ==============================================================================
 * Compiles ALL engines into one shared library.
 *
 * Build (from directory containing this file and the .h headers):
 *   Linux:   gcc -O2 -shared -fPIC -o ts_engines.so ts_engines_all.c -lm -lz
 *   Windows: gcc -O2 -shared -o ts_engines.dll ts_engines_all.c -lm -lz -ladvapi32
 *
 * Build (from parent directory, headers in tss_gfx/):
 *   Linux:   gcc -O2 -shared -fPIC -Itss_gfx -o ts_engines.so tss_gfx/ts_engines_all.c -lm -lz
 *   Windows: gcc -O2 -shared -Itss_gfx -o ts_engines.dll tss_gfx/ts_engines_all.c -lm -lz -ladvapi32
 *
 * Debug (AddressSanitizer + UndefinedBehaviorSanitizer):
 *   gcc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
 *       -shared -fPIC -o ts_engines_asan.so ts_engines_all.c -lm -lz
 *
 * No zlib: add -DTS_PDF_NO_ZLIB (PDFs will be larger but functional)
 *
 * SoftCAD C+C TOPO © 2026
 */

/* ── Warning suppression for known-safe patterns ─────────────────────── */
/* These warnings are reviewed and accepted:                               */
/*   - sign-conversion: size_t ↔ int in allocation sizes (safe bounds)     */
/*   - conversion: float ↔ double in geometry/render (precision ok)        */
/*   - missing-field-initializers: calloc zeroes everything                 */
/*   - overlength-strings: PDF font metrics tables                         */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Woverlength-strings"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

/* ── GFX 2D (5 engines) ─────────────────────────────────────────────── */
#define TS_GFX_CANVAS_IMPLEMENTATION
#define TS_GFX_DRAW_IMPLEMENTATION
#define TS_GFX_TRANSFORM_IMPLEMENTATION
#define TS_GFX_TEXT_IMPLEMENTATION
#define TS_GFX_EXPORT_IMPLEMENTATION
#define TS_ENGINE_ABI_IMPLEMENTATION

/* ── GFX 3D (2 engines) ─────────────────────────────────────────────── */
#define TS_GFX_3D_IMPLEMENTATION
#define TS_GFX_RASTER3D_IMPLEMENTATION

/* ── Titan engines (10 engines) ──────────────────────────────────────── */
#define TS_TITAN_HASH_IMPLEMENTATION
/* Native Win32 events and rendering. Linux UI remains a separate build. */
#ifdef _WIN32
#define TS_TITAN_UI_IMPLEMENTATION
#endif

/* ── Includes ────────────────────────────────────────────────────────── */
#include "ts_engine_abi.h"
#include "ts_gfx_hash_shim.h"  /* hashing generic SHA-256 — ediția grafică deschisă */
#include "ts_gfx_canvas.h"

/* ── Windows cross-compile compatibility shims ─────────────────── */
#ifdef _WIN32
  /* clock_gettime pentru Windows. Până în 4.65 era clock() (ms de la pornirea
     procesului) → UUID v7 avea timestamp ~1970 și nu se sorta între procese,
     iar ts_timer_elapsed_us avea rezoluție de 1 ms. 4.66: REALTIME din
     GetSystemTimePreciseAsFileTime, MONOTONIC din QueryPerformanceCounter.
     Corpul e la sfârșitul unității (după <windows.h>). */
  #include <time.h>
  #ifndef CLOCK_REALTIME
    #define CLOCK_REALTIME 0
  #endif
  #ifndef CLOCK_MONOTONIC
    #define CLOCK_MONOTONIC 1
  #endif
  static int _ts_clock_gettime(int clk, struct timespec *ts);
  #define clock_gettime _ts_clock_gettime
  #ifndef memmem
    static void *memmem(const void *hay, size_t hlen, const void *needle, size_t nlen) {
        if (nlen == 0) return (void*)hay;
        if (hlen < nlen) return NULL;
        const char *h = (const char*)hay, *n = (const char*)needle;
        for (size_t i = 0; i <= hlen - nlen; i++)
            if (memcmp(h+i, n, nlen) == 0) return (void*)(h+i);
        return NULL;
    }
  #endif
#endif
#include "ts_gfx_draw.h"
#include "ts_gfx_transform.h"
#include "ts_gfx_text.h"
#include "ts_gfx_export.h"
#include "ts_gfx_3d.h"
#include "ts_gfx_raster3d.h"
#include "ts_gfx_extra.h"
#include "ts_gfx_imgproc.h"
#include "ts_gfx_perf.h"
#include "ts_gfx_pro.h"
#include "ts_utils.h"
#include "ts_titan_hash.h"
#include "ts_gfx_smart.h"

/* ── Universal free for malloc'd returns (WKT strings, etc.) ──────────── */
void ts_free(void *ptr) { free(ptr); }

#include "ts_render_engine.h"
#include "ts_spatial_index.h"
#include "ts_algorithms.h"
/* ts_utilities.h SCOS: business-logic non-grafic (CNP/CUI/IBAN/QR/date/template/unitati) nefolosit de motor */
#include "ts_text_placement.h"
#include "ts_tile_cache.h"
#include "ts_render_manager.h"
#include "ts_framebuffer.h"
#include "ts_smart_buffer.h"
#include "ts_spatial_ops.h"
/* TCP — Procedeul Crinel Popa (forced cadastral split) v5.2.1 */

/* ── Polygon simplification (boss design — triangulation + iterative
   elimination, mai 2026) ──────────────────────────────────────────── */
#define TS_POLYGON_SIMPLIFY_IMPLEMENTATION
#include "ts_polygon_simplify.h"

/* ── Rasterizatorul planșelor DXF -> TIFF (4.77: fără matplotlib/Pillow) ── */
#define TS_PLOT_RASTER_IMPLEMENTATION
#include "ts_plot_raster.h"

#include "ts_geo_extra2.c"
#include "ts_rs.c"
#include "ts_gfx_extra2.c"
#include "ts_geom.c"
#include "ts_blake3.h"
#include "ts_crc32c.h"
#ifdef _WIN32
#include "ts_titan_ui.h"
#endif

/* ── clock_gettime pentru Windows (declarat sus, în blocul de compatibilitate) ── */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static int _ts_clock_gettime(int clk, struct timespec *ts) {
    if (!ts) return -1;
    if (clk == CLOCK_REALTIME) {
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        /* 100 ns de la 1601-01-01 → de la 1970-01-01 */
        unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32)
                             | (unsigned long long)ft.dwLowDateTime;
        t -= 116444736000000000ULL;
        ts->tv_sec  = (time_t)(t / 10000000ULL);
        ts->tv_nsec = (long)((t % 10000000ULL) * 100ULL);
        return 0;
    }
    if (clk == CLOCK_MONOTONIC) {
        static LONGLONG freq = 0;
        LARGE_INTEGER c;
        if (!freq) {
            LARGE_INTEGER f;
            if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0) return -1;
            freq = f.QuadPart;
        }
        if (!QueryPerformanceCounter(&c)) return -1;
        ts->tv_sec  = (time_t)(c.QuadPart / freq);
        ts->tv_nsec = (long)(((c.QuadPart % freq) * 1000000000LL) / freq);
        return 0;
    }
    /* CPU time: aproximarea veche (clock) */
    clock_t cc = clock();
    ts->tv_sec  = (time_t)(cc / CLOCKS_PER_SEC);
    ts->tv_nsec = (long)((cc % CLOCKS_PER_SEC) * (1000000000L / CLOCKS_PER_SEC));
    return 0;
}
#endif
