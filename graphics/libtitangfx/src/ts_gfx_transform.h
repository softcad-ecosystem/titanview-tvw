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
 * ts_gfx_transform.h — 2D Affine Transforms
 * ============================================
 * CRS (Stereo70, UTM, etc.) → pixel coordinate mapping.
 * Single-header C99. Zero dependencies.
 *
 * #define TS_GFX_TRANSFORM_IMPLEMENTATION in ONE .c file before #include.
 *
 * SoftCAD C+C TOPO © 2026 — Titan Sentinel Script GFX Engine
 */
#ifndef TS_GFX_TRANSFORM_H
#define TS_GFX_TRANSFORM_H

#include "ts_gfx_canvas.h"
#include <math.h>

typedef struct {
    float m[9]; /* 3x3 row-major: [a b tx; c d ty; 0 0 1] */
    /* Bounds for set_bounds/set_viewport/build */
    float wx0, wy0, wx1, wy1;
    int   vx, vy, vw, vh;
} TS_Transform;

/* Create identity transform */
TS_Transform ts_transform_new(void);

/* Set world bounds → viewport mapping (CRS → pixels) */
void ts_transform_set_bounds(TS_Transform *tf,
    float world_min_x, float world_min_y,
    float world_max_x, float world_max_y);
void ts_transform_set_viewport(TS_Transform *tf,
    int vp_x, int vp_y, int vp_w, int vp_h);

/* Build the complete world→screen transform (call after set_bounds + set_viewport) */
void ts_transform_build(TS_Transform *tf);

/* Manual transforms */
void ts_transform_translate(TS_Transform *tf, float dx, float dy);
void ts_transform_scale(TS_Transform *tf, float sx, float sy);
void ts_transform_rotate(TS_Transform *tf, float degrees);
void ts_transform_reset(TS_Transform *tf);

/* Apply transform to a point */
void ts_transform_point(const TS_Transform *tf, float wx, float wy,
                         float *sx, float *sy);

/* Apply transform to canvas (all subsequent draw calls use transformed coords) */
void ts_transform_apply(TS_Canvas *c, const TS_Transform *tf);

/* Inverse transform (screen → world) */
void ts_transform_inverse_point(const TS_Transform *tf, float sx, float sy,
                                 float *wx, float *wy);

/* ── Implementation ────────────────────────────────────────────────────── */
#ifdef TS_GFX_TRANSFORM_IMPLEMENTATION

TS_Transform ts_transform_new(void) {
    TS_Transform tf;
    tf.m[0] = 1; tf.m[1] = 0; tf.m[2] = 0;
    tf.m[3] = 0; tf.m[4] = 1; tf.m[5] = 0;
    tf.m[6] = 0; tf.m[7] = 0; tf.m[8] = 1;
    return tf;
}

void ts_transform_set_bounds(TS_Transform *tf,
    float world_min_x, float world_min_y,
    float world_max_x, float world_max_y) {
    tf->wx0 = world_min_x; tf->wy0 = world_min_y;
    tf->wx1 = world_max_x; tf->wy1 = world_max_y;
}

void ts_transform_set_viewport(TS_Transform *tf,
    int vp_x, int vp_y, int vp_w, int vp_h) {
    tf->vx = vp_x; tf->vy = vp_y; tf->vw = vp_w; tf->vh = vp_h;
}

void ts_transform_build(TS_Transform *tf) {
    float dw_x = tf->wx1 - tf->wx0;
    float dw_y = tf->wy1 - tf->wy0;
    if (dw_x == 0.0f) dw_x = 1.0f;
    if (dw_y == 0.0f) dw_y = 1.0f;

    /* Scale to fit, maintaining aspect ratio */
    float scale_x = (float)tf->vw / dw_x;
    float scale_y = (float)tf->vh / dw_y;
    float scale   = (scale_x < scale_y) ? scale_x : scale_y;

    /* Center in viewport */
    float cx = tf->vx + tf->vw * 0.5f;
    float cy = tf->vy + tf->vh * 0.5f;
    float wcx = (tf->wx0 + tf->wx1) * 0.5f;
    float wcy = (tf->wy0 + tf->wy1) * 0.5f;

    /* Matrix: translate(-wcx,-wcy), scale(s,-s), translate(cx,cy) */
    /* Y is inverted (world Y up, screen Y down) */
    tf->m[0] =  scale;   tf->m[1] = 0;       tf->m[2] = cx - wcx * scale;
    tf->m[3] =  0;       tf->m[4] = -scale;  tf->m[5] = cy + wcy * scale;
    tf->m[6] =  0;       tf->m[7] = 0;       tf->m[8] = 1;
}

void ts_transform_translate(TS_Transform *tf, float dx, float dy) {
    tf->m[2] += dx;
    tf->m[5] += dy;
}

void ts_transform_scale(TS_Transform *tf, float sx, float sy) {
    tf->m[0] *= sx; tf->m[1] *= sy; tf->m[2] *= sx;
    tf->m[3] *= sx; tf->m[4] *= sy; tf->m[5] *= sy;
}

void ts_transform_rotate(TS_Transform *tf, float degrees) {
    float rad = degrees * 0.0174532925f;
    float cs = cosf(rad), sn = sinf(rad);
    float a = tf->m[0], b = tf->m[1]; (void)b;
    float c_v = tf->m[3], d = tf->m[4];
    tf->m[0] = a*cs + b*sn;   tf->m[1] = -a*sn + b*cs;
    tf->m[3] = c_v*cs + d*sn; tf->m[4] = -c_v*sn + d*cs;
}

void ts_transform_reset(TS_Transform *tf) {
    tf->m[0] = 1; tf->m[1] = 0; tf->m[2] = 0;
    tf->m[3] = 0; tf->m[4] = 1; tf->m[5] = 0;
    tf->m[6] = 0; tf->m[7] = 0; tf->m[8] = 1;
}

void ts_transform_point(const TS_Transform *tf, float wx, float wy,
                         float *sx, float *sy) {
    *sx = tf->m[0] * wx + tf->m[1] * wy + tf->m[2];
    *sy = tf->m[3] * wx + tf->m[4] * wy + tf->m[5];
}

void ts_transform_apply(TS_Canvas *c, const TS_Transform *tf) {
    memcpy(c->tf, tf->m, 9 * sizeof(float));
    c->tf_active = 1;
}

void ts_transform_inverse_point(const TS_Transform *tf, float sx, float sy,
                                 float *wx, float *wy) {
    /* 2D affine inverse: [a b tx; c d ty] */
    float a = tf->m[0], b = tf->m[1], tx = tf->m[2];
    float c_v = tf->m[3], d = tf->m[4], ty = tf->m[5];
    float det = a * d - b * c_v;
    if (fabsf(det) < 1e-10f) { *wx = sx; *wy = sy; return; }
    float inv_det = 1.0f / det;
    *wx = inv_det * ( d * (sx - tx) - b * (sy - ty));
    *wy = inv_det * (-c_v * (sx - tx) + a * (sy - ty));
}

#endif /* TS_GFX_TRANSFORM_IMPLEMENTATION */
#endif /* TS_GFX_TRANSFORM_H */
