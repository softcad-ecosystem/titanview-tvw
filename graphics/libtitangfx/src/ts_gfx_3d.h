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
 * ts_gfx_3d.h — 3D Math, Camera & Projection
 * =============================================
 * Vec3, Mat4, perspective/orthographic projection, camera orbit.
 * Single-header C99. Zero dependencies.
 *
 * #define TS_GFX_3D_IMPLEMENTATION in ONE .c file before #include.
 *
 * SoftCAD C+C TOPO © 2026 — Titan Sentinel Script GFX Engine
 */
#ifndef TS_GFX_3D_H
#define TS_GFX_3D_H

#include <math.h>
#include <string.h>

/* ── Vec3 ──────────────────────────────────────────────────────────────── */

typedef struct { float x, y, z; } TS_Vec3;

static inline TS_Vec3 ts_v3(float x, float y, float z) {
    TS_Vec3 v = {x, y, z}; return v;
}
static inline TS_Vec3 ts_v3_add(TS_Vec3 a, TS_Vec3 b) {
    return ts_v3(a.x+b.x, a.y+b.y, a.z+b.z);
}
static inline TS_Vec3 ts_v3_sub(TS_Vec3 a, TS_Vec3 b) {
    return ts_v3(a.x-b.x, a.y-b.y, a.z-b.z);
}
static inline TS_Vec3 ts_v3_scale(TS_Vec3 v, float s) {
    return ts_v3(v.x*s, v.y*s, v.z*s);
}
static inline float ts_v3_dot(TS_Vec3 a, TS_Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
static inline TS_Vec3 ts_v3_cross(TS_Vec3 a, TS_Vec3 b) {
    return ts_v3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
static inline float ts_v3_len(TS_Vec3 v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}
static inline TS_Vec3 ts_v3_norm(TS_Vec3 v) {
    float l = ts_v3_len(v);
    if (l < 1e-8f) return ts_v3(0,0,0);
    return ts_v3_scale(v, 1.0f / l);
}

/* ── Vec4 (for homogeneous coordinates) ────────────────────────────────── */

typedef struct { float x, y, z, w; } TS_Vec4;

static inline TS_Vec4 ts_v4(float x, float y, float z, float w) {
    TS_Vec4 v = {x, y, z, w}; return v;
}

/* ── Mat4 (4x4 column-major) ───────────────────────────────────────────── */

typedef struct { float m[16]; } TS_Mat4;

TS_Mat4 ts_mat4_identity(void);
TS_Mat4 ts_mat4_mul(TS_Mat4 a, TS_Mat4 b);
TS_Vec4 ts_mat4_mulv(TS_Mat4 m, TS_Vec4 v);

/* Projection */
TS_Mat4 ts_mat4_perspective(float fov_deg, float aspect, float znear, float zfar);
TS_Mat4 ts_mat4_ortho(float left, float right, float bottom, float top, float znear, float zfar);

/* View (look-at) */
TS_Mat4 ts_mat4_lookat(TS_Vec3 eye, TS_Vec3 center, TS_Vec3 up);

/* Transform */
TS_Mat4 ts_mat4_translate(float x, float y, float z);
TS_Mat4 ts_mat4_scale(float x, float y, float z);
TS_Mat4 ts_mat4_rotate_y(float degrees);
TS_Mat4 ts_mat4_rotate_x(float degrees);

/* ── Camera ────────────────────────────────────────────────────────────── */

typedef struct {
    TS_Vec3 position;     /* camera position in world */
    TS_Vec3 target;       /* look-at target */
    TS_Vec3 up;           /* up vector */
    float   fov;          /* field of view (degrees) */
    float   aspect;       /* width / height */
    float   znear, zfar;  /* clipping planes */
    int     ortho;        /* 1 = orthographic, 0 = perspective */
    float   ortho_size;   /* orthographic half-height */
    TS_Mat4 view;         /* computed view matrix */
    TS_Mat4 proj;         /* computed projection matrix */
    TS_Mat4 vp;           /* view * projection (combined) */
} TS_Camera;

TS_Camera ts_camera_new(float fov, float aspect);
TS_Camera ts_camera_ortho_new(float size, float aspect);
void ts_camera_set_position(TS_Camera *cam, float x, float y, float z);
void ts_camera_set_target(TS_Camera *cam, float x, float y, float z);
void ts_camera_orbit(TS_Camera *cam, float azimuth_deg, float elevation_deg, float distance);
void ts_camera_update(TS_Camera *cam);

/* Project 3D point → screen coords (returns 1 if visible, 0 if clipped) */
int ts_camera_project(const TS_Camera *cam, TS_Vec3 world,
                       int screen_w, int screen_h,
                       float *sx, float *sy, float *depth);

/* ── Implementation ────────────────────────────────────────────────────── */
#ifdef TS_GFX_3D_IMPLEMENTATION

TS_Mat4 ts_mat4_identity(void) {
    TS_Mat4 m; memset(&m, 0, sizeof(m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

TS_Mat4 ts_mat4_mul(TS_Mat4 a, TS_Mat4 b) {
    TS_Mat4 r; memset(&r, 0, sizeof(r));
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                r.m[c*4+row] += a.m[k*4+row] * b.m[c*4+k];
    return r;
}

TS_Vec4 ts_mat4_mulv(TS_Mat4 m, TS_Vec4 v) {
    return ts_v4(
        m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z  + m.m[12]*v.w,
        m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z  + m.m[13]*v.w,
        m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]*v.w,
        m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15]*v.w
    );
}

TS_Mat4 ts_mat4_perspective(float fov_deg, float aspect, float znear, float zfar) {
    TS_Mat4 m; memset(&m, 0, sizeof(m));
    float f = 1.0f / tanf(fov_deg * 0.5f * 0.0174532925f);
    m.m[0]  = f / aspect;
    m.m[5]  = f;
    m.m[10] = (zfar + znear) / (znear - zfar);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return m;
}

TS_Mat4 ts_mat4_ortho(float l, float r, float b, float t, float n, float f) {
    TS_Mat4 m; memset(&m, 0, sizeof(m));
    m.m[0]  = 2.0f / (r - l);
    m.m[5]  = 2.0f / (t - b);
    m.m[10] = -2.0f / (f - n);
    m.m[12] = -(r + l) / (r - l);
    m.m[13] = -(t + b) / (t - b);
    m.m[14] = -(f + n) / (f - n);
    m.m[15] = 1.0f;
    return m;
}

TS_Mat4 ts_mat4_lookat(TS_Vec3 eye, TS_Vec3 center, TS_Vec3 up) {
    TS_Vec3 f = ts_v3_norm(ts_v3_sub(center, eye));
    TS_Vec3 s = ts_v3_norm(ts_v3_cross(f, up));
    TS_Vec3 u = ts_v3_cross(s, f);
    TS_Mat4 m = ts_mat4_identity();
    m.m[0] = s.x;  m.m[4] = s.y;  m.m[8]  = s.z;
    m.m[1] = u.x;  m.m[5] = u.y;  m.m[9]  = u.z;
    m.m[2] = -f.x; m.m[6] = -f.y; m.m[10] = -f.z;
    m.m[12] = -ts_v3_dot(s, eye);
    m.m[13] = -ts_v3_dot(u, eye);
    m.m[14] =  ts_v3_dot(f, eye);
    return m;
}

TS_Mat4 ts_mat4_translate(float x, float y, float z) {
    TS_Mat4 m = ts_mat4_identity();
    m.m[12] = x; m.m[13] = y; m.m[14] = z;
    return m;
}

TS_Mat4 ts_mat4_scale(float x, float y, float z) {
    TS_Mat4 m = ts_mat4_identity();
    m.m[0] = x; m.m[5] = y; m.m[10] = z;
    return m;
}

TS_Mat4 ts_mat4_rotate_y(float deg) {
    float r = deg * 0.0174532925f;
    float c = cosf(r), s = sinf(r);
    TS_Mat4 m = ts_mat4_identity();
    m.m[0] = c;  m.m[8] = s;
    m.m[2] = -s; m.m[10] = c;
    return m;
}

TS_Mat4 ts_mat4_rotate_x(float deg) {
    float r = deg * 0.0174532925f;
    float c = cosf(r), s = sinf(r);
    TS_Mat4 m = ts_mat4_identity();
    m.m[5] = c;  m.m[9] = -s;
    m.m[6] = s;  m.m[10] = c;
    return m;
}

/* ── Camera ────────────────────────────────────────────────────────────── */

TS_Camera ts_camera_new(float fov, float aspect) {
    TS_Camera cam;
    cam.position = ts_v3(0, 5, 10);
    cam.target   = ts_v3(0, 0, 0);
    cam.up       = ts_v3(0, 1, 0);
    cam.fov      = fov;
    cam.aspect   = aspect;
    cam.znear    = 0.1f;
    cam.zfar     = 10000.0f;
    cam.ortho    = 0;
    cam.ortho_size = 10.0f;
    cam.view     = ts_mat4_identity();
    cam.proj     = ts_mat4_identity();
    cam.vp       = ts_mat4_identity();
    return cam;
}

TS_Camera ts_camera_ortho_new(float size, float aspect) {
    TS_Camera cam = ts_camera_new(60, aspect);
    cam.ortho = 1;
    cam.ortho_size = size;
    return cam;
}

void ts_camera_set_position(TS_Camera *cam, float x, float y, float z) {
    cam->position = ts_v3(x, y, z);
}

void ts_camera_set_target(TS_Camera *cam, float x, float y, float z) {
    cam->target = ts_v3(x, y, z);
}

void ts_camera_orbit(TS_Camera *cam, float azimuth_deg, float elev_deg, float dist) {
    float az = azimuth_deg * 0.0174532925f;
    float el = elev_deg * 0.0174532925f;
    cam->position.x = cam->target.x + dist * cosf(el) * sinf(az);
    cam->position.y = cam->target.y + dist * sinf(el);
    cam->position.z = cam->target.z + dist * cosf(el) * cosf(az);
}

void ts_camera_update(TS_Camera *cam) {
    cam->view = ts_mat4_lookat(cam->position, cam->target, cam->up);
    if (cam->ortho) {
        float h = cam->ortho_size;
        float w = h * cam->aspect;
        cam->proj = ts_mat4_ortho(-w, w, -h, h, cam->znear, cam->zfar);
    } else {
        cam->proj = ts_mat4_perspective(cam->fov, cam->aspect, cam->znear, cam->zfar);
    }
    cam->vp = ts_mat4_mul(cam->proj, cam->view);
}

int ts_camera_project(const TS_Camera *cam, TS_Vec3 world,
                       int sw, int sh, float *sx, float *sy, float *depth) {
    TS_Vec4 clip = ts_mat4_mulv(cam->vp, ts_v4(world.x, world.y, world.z, 1.0f));
    if (clip.w <= 0.0001f) return 0;  /* behind camera */
    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;
    float ndc_z = clip.z / clip.w;
    if (ndc_x < -1.2f || ndc_x > 1.2f || ndc_y < -1.2f || ndc_y > 1.2f) return 0;
    *sx = (ndc_x * 0.5f + 0.5f) * sw;
    *sy = (1.0f - (ndc_y * 0.5f + 0.5f)) * sh;  /* Y flip */
    *depth = ndc_z;
    return 1;
}

#endif /* TS_GFX_3D_IMPLEMENTATION */
#endif /* TS_GFX_3D_H */
