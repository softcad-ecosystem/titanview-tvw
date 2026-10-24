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
#ifndef TS_ENGINE_ABI_H
#define TS_ENGINE_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TS_ENGINE_ABI_MAJOR 1u
#define TS_ENGINE_ABI_MINOR 0u
#define TS_ENGINE_ABI_VERSION ((TS_ENGINE_ABI_MAJOR << 16) | TS_ENGINE_ABI_MINOR)

#define TS_FEATURE_CANVAS              (1ULL << 0)
#define TS_FEATURE_FRAMEBUFFER         (1ULL << 1)
#define TS_FEATURE_RENDER_MANAGER      (1ULL << 2)
#define TS_FEATURE_TILE_CACHE          (1ULL << 3)
#define TS_FEATURE_COMMAND_BUFFER      (1ULL << 4)
#define TS_FEATURE_UDF_SDF             (1ULL << 5)
#define TS_FEATURE_RASTER3D            (1ULL << 6)
#define TS_FEATURE_GLYPH_TINTED        (1ULL << 7)
#define TS_FEATURE_DYNAMIC_GRID        (1ULL << 8)
#define TS_FEATURE_SIGNED_IMAGE_DIFF   (1ULL << 9)
#define TS_FEATURE_RENDER_FUSION_V1    (1ULL << 10)
#define TS_FEATURE_PLOT_RASTER         (1ULL << 11)   /* 4.77: ts_plot_* (planșe -> TIFF) */

uint32_t ts_engine_abi_version(void);
uint64_t ts_engine_feature_flags(void);
const char *ts_engine_build_id(void);
int ts_engine_has_feature(uint64_t feature);

#ifdef TS_ENGINE_ABI_IMPLEMENTATION
uint32_t ts_engine_abi_version(void) { return TS_ENGINE_ABI_VERSION; }
uint64_t ts_engine_feature_flags(void) {
    return TS_FEATURE_CANVAS | TS_FEATURE_FRAMEBUFFER | TS_FEATURE_RENDER_MANAGER |
           TS_FEATURE_TILE_CACHE | TS_FEATURE_COMMAND_BUFFER | TS_FEATURE_UDF_SDF |
           TS_FEATURE_RASTER3D | TS_FEATURE_GLYPH_TINTED | TS_FEATURE_DYNAMIC_GRID |
           TS_FEATURE_SIGNED_IMAGE_DIFF | TS_FEATURE_RENDER_FUSION_V1 | TS_FEATURE_PLOT_RASTER;
}
const char *ts_engine_build_id(void) { return "titan-native-render-fusion-1"; }
int ts_engine_has_feature(uint64_t feature) {
    return (ts_engine_feature_flags() & feature) == feature ? 1 : 0;
}
#endif

#ifdef __cplusplus
}
#endif
#endif
