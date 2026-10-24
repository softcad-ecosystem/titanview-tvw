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
 * ts_gfx_raster3d.h — 3D Triangle Rasterizer with Z-Buffer
 * ===========================================================
 * Software 3D renderer: z-buffer, flat/Gouraud shading, wireframe.
 * Single-header C99. Zero dependencies.
 *
 * #define TS_GFX_RASTER3D_IMPLEMENTATION in ONE .c file before #include.
 *
 * SoftCAD C+C TOPO © 2026 — Titan Sentinel Script GFX Engine
 */
#ifndef TS_GFX_RASTER3D_H
#define TS_GFX_RASTER3D_H

#include "ts_gfx_canvas.h"
#include "ts_gfx_3d.h"

/* ── Z-Buffer ──────────────────────────────────────────────────────────── */

typedef struct {
    float    *depth;    /* depth buffer (1 float per pixel) */
    int       width, height;
} TS_ZBuffer;

TS_ZBuffer *ts_zbuf_new(int w, int h);
void        ts_zbuf_free(TS_ZBuffer *z);
void        ts_zbuf_clear(TS_ZBuffer *z);

/* ── Triangle ──────────────────────────────────────────────────────────── */

typedef struct {
    TS_Vec3  v[3];      /* vertices (world space) */
    TS_Vec3  normal;    /* face normal (auto-computed if zero) */
    TS_Vec3  vn[3];     /* vertex normals (for Gouraud; zero = use face normal) */
    uint32_t color;     /* face color (flat shading) */
    uint32_t vc[3];     /* vertex colors (Gouraud; 0 = use face color) */
} TS_Triangle;

/* ── Light ─────────────────────────────────────────────────────────────── */

typedef struct {
    TS_Vec3  direction;  /* normalized direction TO light */
    float    ambient;    /* 0.0–1.0 */
    float    diffuse;    /* 0.0–1.0 */
    uint32_t color;      /* light color (usually white) */
} TS_Light;

TS_Light ts_light_new(float dx, float dy, float dz, float ambient, float diffuse);

/* ── Rendering ─────────────────────────────────────────────────────────── */

/* Flat shading with z-buffer */
void ts_render_triangle(TS_Canvas *c, TS_ZBuffer *z, const TS_Camera *cam,
                         const TS_Triangle *tri, const TS_Light *light);

/* Gouraud shading (per-vertex color interpolation) with z-buffer */
void ts_render_triangle_gouraud(TS_Canvas *c, TS_ZBuffer *z, const TS_Camera *cam,
                                 const TS_Triangle *tri, const TS_Light *light);

/* Render wireframe triangle */
void ts_render_triangle_wireframe(TS_Canvas *c, const TS_Camera *cam,
                                   const TS_Triangle *tri);

/* ── Mesh helpers ──────────────────────────────────────────────────────── */

/* Extrude a 2D polygon to a 3D box (base_y to top_y).
   ring: array of (x,z) pairs, n: number of vertices.
   out_tris: output triangle array (must have space for 2*(n-2) + 2*n triangles).
   Returns number of triangles written. */
int ts_mesh_extrude(const float *ring_xz, int n,
                     float base_y, float top_y,
                     uint32_t top_color, uint32_t side_color,
                     TS_Triangle *out_tris);

/* Create a flat terrain quad grid.
   Returns number of triangles (cols*rows*2). */
int ts_mesh_terrain_grid(float x0, float z0, float x1, float z1,
                          int cols, int rows,
                          const float *heights, /* cols*rows height values, or NULL for flat */
                          uint32_t color,
                          TS_Triangle *out_tris);

/* ── Implementation ────────────────────────────────────────────────────── */
#ifdef TS_GFX_RASTER3D_IMPLEMENTATION

TS_ZBuffer *ts_zbuf_new(int w, int h) {
    TS_ZBuffer *z = (TS_ZBuffer *)calloc(1, sizeof(TS_ZBuffer));
    if (!z) return NULL;
    z->width = w; z->height = h;
    z->depth = (float *)malloc((size_t)(w * h) * sizeof(float));
    if (!z->depth) { free(z); return NULL; }
    ts_zbuf_clear(z);
    return z;
}

void ts_zbuf_free(TS_ZBuffer *z) {
    if (z) { free(z->depth); free(z); }
}

void ts_zbuf_clear(TS_ZBuffer *z) {
    int n = z->width * z->height;
    for (int i = 0; i < n; i++) z->depth[i] = 1e30f;
}

TS_Light ts_light_new(float dx, float dy, float dz, float ambient, float diffuse) {
    TS_Light l;
    l.direction = ts_v3_norm(ts_v3(dx, dy, dz));
    l.ambient   = ambient;
    l.diffuse   = diffuse;
    l.color     = 0xFFFFFFFF;
    return l;
}

/* ── Compute face normal ───────────────────────────────────────────────── */

static TS_Vec3 _face_normal(const TS_Triangle *tri) {
    TS_Vec3 e1 = ts_v3_sub(tri->v[1], tri->v[0]);
    TS_Vec3 e2 = ts_v3_sub(tri->v[2], tri->v[0]);
    return ts_v3_norm(ts_v3_cross(e1, e2));
}

/* ── Flat-shaded lighting ──────────────────────────────────────────────── */

static uint32_t _shade(uint32_t base_color, TS_Vec3 normal, const TS_Light *light) {
    float ndl = ts_v3_dot(normal, light->direction);
    if (ndl < 0) ndl = 0;
    float intensity = light->ambient + light->diffuse * ndl;
    if (intensity > 1.0f) intensity = 1.0f;

    int r = (int)(ts_r(base_color) * intensity);
    int g = (int)(ts_g(base_color) * intensity);
    int b = (int)(ts_b(base_color) * intensity);
    return ts_rgba(r, g, b, ts_a(base_color));
}

/* ── Scanline triangle rasterizer with z-buffer ────────────────────────── */

void ts_render_triangle(TS_Canvas *c, TS_ZBuffer *z, const TS_Camera *cam,
                         const TS_Triangle *tri, const TS_Light *light) {
    int W = c->width, H = c->height;

    /* Project vertices to screen */
    float sx[3], sy[3], sd[3];
    for (int i = 0; i < 3; i++) {
        if (!ts_camera_project(cam, tri->v[i], W, H, &sx[i], &sy[i], &sd[i]))
            return; /* clipped */
    }

    /* Back-face culling */
    float cross_z = (sx[1]-sx[0])*(sy[2]-sy[0]) - (sy[1]-sy[0])*(sx[2]-sx[0]);
    if (cross_z > 0) return; /* facing away */

    /* Compute shaded color */
    TS_Vec3 normal = _face_normal(tri);
    uint32_t color = light ? _shade(tri->color, normal, light) : tri->color;

    /* Sort by Y */
    int idx[3] = {0, 1, 2};
    if (sy[idx[0]] > sy[idx[1]]) { int t = idx[0]; idx[0] = idx[1]; idx[1] = t; }
    if (sy[idx[1]] > sy[idx[2]]) { int t = idx[1]; idx[1] = idx[2]; idx[2] = t; }
    if (sy[idx[0]] > sy[idx[1]]) { int t = idx[0]; idx[0] = idx[1]; idx[1] = t; }

    float x0 = sx[idx[0]], y0 = sy[idx[0]], d0 = sd[idx[0]];
    float x1 = sx[idx[1]], y1 = sy[idx[1]], d1 = sd[idx[1]];
    float x2 = sx[idx[2]], y2 = sy[idx[2]], d2 = sd[idx[2]];

    /* Rasterize scanlines */
    int iy_top = (int)y0;
    int iy_mid = (int)y1;
    int iy_bot = (int)y2;
    if (iy_top < 0) iy_top = 0;
    if (iy_bot >= H) iy_bot = H - 1;

    for (int iy = iy_top; iy <= iy_bot; iy++) {
        float fy = (float)iy + 0.5f;
        float xa, xb, da, db;

        /* Long edge (0 → 2) */
        float t02 = (y2 - y0) > 0.001f ? (fy - y0) / (y2 - y0) : 0;
        xa = x0 + t02 * (x2 - x0);
        da = d0 + t02 * (d2 - d0);

        /* Short edges */
        if (fy < y1) {
            float t01 = (y1 - y0) > 0.001f ? (fy - y0) / (y1 - y0) : 0;
            xb = x0 + t01 * (x1 - x0);
            db = d0 + t01 * (d1 - d0);
        } else {
            float t12 = (y2 - y1) > 0.001f ? (fy - y1) / (y2 - y1) : 0;
            xb = x1 + t12 * (x2 - x1);
            db = d1 + t12 * (d2 - d1);
        }

        /* Ensure xa <= xb */
        if (xa > xb) { float t = xa; xa = xb; xb = t; t = da; da = db; db = t; }

        int ix0 = (int)xa, ix1 = (int)xb;
        if (ix0 < 0) ix0 = 0;
        if (ix1 >= W) ix1 = W - 1;

        float dx_span = (xb - xa) > 0.001f ? 1.0f / (xb - xa) : 0;

        for (int ix = ix0; ix <= ix1; ix++) {
            float t = ((float)ix - xa) * dx_span;
            float depth = da + t * (db - da);
            int zi = iy * W + ix;
            if (depth < z->depth[zi]) {
                z->depth[zi] = depth;
                c->pixels[zi] = color;
            }
        }
    }
}

/* ── Gouraud shading (per-vertex interpolation) ────────────────────────── */

static uint32_t _lerp_color(uint32_t c0, uint32_t c1, float t) {
    int r = (int)(ts_r(c0) * (1-t) + ts_r(c1) * t);
    int g = (int)(ts_g(c0) * (1-t) + ts_g(c1) * t);
    int b = (int)(ts_b(c0) * (1-t) + ts_b(c1) * t);
    int a = (int)(ts_a(c0) * (1-t) + ts_a(c1) * t);
    return ts_rgba(r, g, b, a);
}

void ts_render_triangle_gouraud(TS_Canvas *c, TS_ZBuffer *z, const TS_Camera *cam,
                                 const TS_Triangle *tri, const TS_Light *light) {
    int W = c->width, H = c->height;

    float sx[3], sy[3], sd[3];
    for (int i = 0; i < 3; i++) {
        if (!ts_camera_project(cam, tri->v[i], W, H, &sx[i], &sy[i], &sd[i]))
            return;
    }

    /* Back-face culling */
    float cross_z = (sx[1]-sx[0])*(sy[2]-sy[0]) - (sy[1]-sy[0])*(sx[2]-sx[0]);
    if (cross_z > 0) return;

    /* Per-vertex shaded colors */
    uint32_t vc[3];
    for (int i = 0; i < 3; i++) {
        uint32_t base = tri->vc[i] ? tri->vc[i] : tri->color;
        TS_Vec3 vn = (ts_v3_len(tri->vn[i]) > 0.01f) ? tri->vn[i] : _face_normal(tri);
        vc[i] = light ? _shade(base, vn, light) : base;
    }

    /* Sort by Y */
    int idx[3] = {0, 1, 2};
    if (sy[idx[0]] > sy[idx[1]]) { int t = idx[0]; idx[0] = idx[1]; idx[1] = t; }
    if (sy[idx[1]] > sy[idx[2]]) { int t = idx[1]; idx[1] = idx[2]; idx[2] = t; }
    if (sy[idx[0]] > sy[idx[1]]) { int t = idx[0]; idx[0] = idx[1]; idx[1] = t; }

    float x0 = sx[idx[0]], y0_f = sy[idx[0]], d0 = sd[idx[0]]; uint32_t c0 = vc[idx[0]];
    float x1 = sx[idx[1]], y1_f = sy[idx[1]], d1 = sd[idx[1]]; uint32_t c1 = vc[idx[1]];
    float x2 = sx[idx[2]], y2_f = sy[idx[2]], d2 = sd[idx[2]]; uint32_t c2 = vc[idx[2]];

    int iy_top = (int)y0_f;
    int iy_bot = (int)y2_f;
    if (iy_top < 0) iy_top = 0;
    if (iy_bot >= H) iy_bot = H - 1;

    for (int iy = iy_top; iy <= iy_bot; iy++) {
        float fy = (float)iy + 0.5f;
        float xa, xb, da, db;
        uint32_t ca, cb;

        /* Long edge */
        float t02 = (y2_f - y0_f) > 0.001f ? (fy - y0_f) / (y2_f - y0_f) : 0;
        xa = x0 + t02 * (x2 - x0);
        da = d0 + t02 * (d2 - d0);
        ca = _lerp_color(c0, c2, t02);

        /* Short edges */
        if (fy < y1_f) {
            float t01 = (y1_f - y0_f) > 0.001f ? (fy - y0_f) / (y1_f - y0_f) : 0;
            xb = x0 + t01 * (x1 - x0);
            db = d0 + t01 * (d1 - d0);
            cb = _lerp_color(c0, c1, t01);
        } else {
            float t12 = (y2_f - y1_f) > 0.001f ? (fy - y1_f) / (y2_f - y1_f) : 0;
            xb = x1 + t12 * (x2 - x1);
            db = d1 + t12 * (d2 - d1);
            cb = _lerp_color(c1, c2, t12);
        }

        if (xa > xb) { float t=xa; xa=xb; xb=t; t=da; da=db; db=t; uint32_t tc=ca; ca=cb; cb=tc; }

        int ix0 = (int)xa, ix1 = (int)xb;
        if (ix0 < 0) ix0 = 0;
        if (ix1 >= W) ix1 = W - 1;

        float dx_span = (xb - xa) > 0.001f ? 1.0f / (xb - xa) : 0;

        for (int ix = ix0; ix <= ix1; ix++) {
            float t = ((float)ix - xa) * dx_span;
            float depth = da + t * (db - da);
            int zi = iy * W + ix;
            if (depth < z->depth[zi]) {
                z->depth[zi] = depth;
                c->pixels[zi] = _lerp_color(ca, cb, t);
            }
        }
    }
}

/* ── Wireframe ─────────────────────────────────────────────────────────── */

void ts_render_triangle_wireframe(TS_Canvas *c, const TS_Camera *cam,
                                   const TS_Triangle *tri) {
    int W = c->width, H = c->height;
    float sx[3], sy[3], sd[3];
    for (int i = 0; i < 3; i++) {
        if (!ts_camera_project(cam, tri->v[i], W, H, &sx[i], &sy[i], &sd[i]))
            return;
    }
    c->stroke_color = tri->color;
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        ts_draw_line(c, (int)sx[i], (int)sy[i], (int)sx[j], (int)sy[j]);
    }
}

/* ── Mesh: Extrude polygon ─────────────────────────────────────────────── */

int ts_mesh_extrude(const float *ring_xz, int n,
                     float base_y, float top_y,
                     uint32_t top_color, uint32_t side_color,
                     TS_Triangle *out) {
    int count = 0;

    /* Top face (fan triangulation) */
    for (int i = 1; i < n - 1; i++) {
        TS_Triangle *t = &out[count++];
        t->v[0] = ts_v3(ring_xz[0],       top_y, ring_xz[1]);
        t->v[1] = ts_v3(ring_xz[i*2],     top_y, ring_xz[i*2+1]);
        t->v[2] = ts_v3(ring_xz[(i+1)*2], top_y, ring_xz[(i+1)*2+1]);
        t->color = top_color;
        t->normal = ts_v3(0, 1, 0);
    }

    /* Bottom face */
    for (int i = 1; i < n - 1; i++) {
        TS_Triangle *t = &out[count++];
        t->v[0] = ts_v3(ring_xz[0],       base_y, ring_xz[1]);
        t->v[2] = ts_v3(ring_xz[i*2],     base_y, ring_xz[i*2+1]);
        t->v[1] = ts_v3(ring_xz[(i+1)*2], base_y, ring_xz[(i+1)*2+1]);
        t->color = side_color;
        t->normal = ts_v3(0, -1, 0);
    }

    /* Side faces (2 triangles per edge) */
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float x0 = ring_xz[i*2], z0 = ring_xz[i*2+1];
        float x1 = ring_xz[j*2], z1 = ring_xz[j*2+1];

        /* Triangle 1 */
        TS_Triangle *t1 = &out[count++];
        t1->v[0] = ts_v3(x0, base_y, z0);
        t1->v[1] = ts_v3(x1, base_y, z1);
        t1->v[2] = ts_v3(x1, top_y,  z1);
        t1->color = side_color;

        /* Triangle 2 */
        TS_Triangle *t2 = &out[count++];
        t2->v[0] = ts_v3(x0, base_y, z0);
        t2->v[1] = ts_v3(x1, top_y,  z1);
        t2->v[2] = ts_v3(x0, top_y,  z0);
        t2->color = side_color;
    }

    return count;
}

/* ── Mesh: Terrain grid ────────────────────────────────────────────────── */

int ts_mesh_terrain_grid(float x0, float z0, float x1, float z1,
                          int cols, int rows,
                          const float *heights,
                          uint32_t color,
                          TS_Triangle *out) {
    float dx = (x1 - x0) / (float)cols;
    float dz = (z1 - z0) / (float)rows;
    int count = 0;

    for (int r = 0; r < rows; r++) {
        for (int c_i = 0; c_i < cols; c_i++) {
            float lx = x0 + c_i * dx, lz = z0 + r * dz;
            float h00 = heights ? heights[r * (cols+1) + c_i]         : 0;
            float h10 = heights ? heights[r * (cols+1) + c_i + 1]     : 0;
            float h01 = heights ? heights[(r+1) * (cols+1) + c_i]     : 0;
            float h11 = heights ? heights[(r+1) * (cols+1) + c_i + 1] : 0;

            TS_Triangle *t1 = &out[count++];
            t1->v[0] = ts_v3(lx,      h00, lz);
            t1->v[1] = ts_v3(lx + dx, h10, lz);
            t1->v[2] = ts_v3(lx + dx, h11, lz + dz);
            t1->color = color;

            TS_Triangle *t2 = &out[count++];
            t2->v[0] = ts_v3(lx,      h00, lz);
            t2->v[1] = ts_v3(lx + dx, h11, lz + dz);
            t2->v[2] = ts_v3(lx,      h01, lz + dz);
            t2->color = color;
        }
    }
    return count;
}

#endif /* TS_GFX_RASTER3D_IMPLEMENTATION */
#endif /* TS_GFX_RASTER3D_H */
