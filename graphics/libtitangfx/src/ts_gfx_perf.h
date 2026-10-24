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
/* ── ts_gfx_perf.h — Performance optimizations for large datasets ─────
 * Viewport culling, quad-tree spatial index, LOD simplification,
 * batch polygon rendering. Designed for 50K+ cadastral entities.
 * C99, zero dependencies.
 *
 * SoftCAD C+C TOPO © 2026
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════ */
/*  BOUNDING BOX + VIEWPORT CULLING                                    */
/* ═══════════════════════════════════════════════════════════════════ */

typedef struct { double xmin, ymin, xmax, ymax; } TS_GfxBBox;

static inline int _bbox_intersects(const TS_GfxBBox *a, const TS_GfxBBox *b) {
    return !(a->xmax < b->xmin || a->xmin > b->xmax ||
             a->ymax < b->ymin || a->ymin > b->ymax);
}

static inline int _bbox_contains_point(const TS_GfxBBox *bb, double x, double y) {
    return x >= bb->xmin && x <= bb->xmax && y >= bb->ymin && y <= bb->ymax;
}

TS_GfxBBox ts_bbox_from_points(const double *xy, int npts) {
    TS_GfxBBox bb = { 1e30, 1e30, -1e30, -1e30 };
    for (int i = 0; i < npts; i++) {
        if (xy[i*2] < bb.xmin) bb.xmin = xy[i*2];
        if (xy[i*2] > bb.xmax) bb.xmax = xy[i*2];
        if (xy[i*2+1] < bb.ymin) bb.ymin = xy[i*2+1];
        if (xy[i*2+1] > bb.ymax) bb.ymax = xy[i*2+1];
    }
    return bb;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  QUAD-TREE SPATIAL INDEX                                            */
/* ═══════════════════════════════════════════════════════════════════ */

#define QT_MAX_ITEMS  16
#define QT_MAX_DEPTH  10

typedef struct { int id; TS_GfxBBox bbox; } QT_Item;

typedef struct QT_Node {
    TS_GfxBBox          bounds;
    QT_Item         *items;
    int              item_count, item_cap;
    struct QT_Node  *children[4]; /* NW, NE, SW, SE */
    int              depth;
    int              is_split;
} QT_Node;

QT_Node *ts_qtree_new(double xmin, double ymin, double xmax, double ymax) {
    QT_Node *n = (QT_Node *)calloc(1, sizeof(QT_Node));
    if (!n) return NULL;
    n->bounds = (TS_GfxBBox){ xmin, ymin, xmax, ymax };
    return n;
}

static void _qt_split(QT_Node *n) {
    double mx = (n->bounds.xmin + n->bounds.xmax) / 2;
    double my = (n->bounds.ymin + n->bounds.ymax) / 2;
    n->children[0] = ts_qtree_new(n->bounds.xmin, my, mx, n->bounds.ymax); /* NW */
    n->children[1] = ts_qtree_new(mx, my, n->bounds.xmax, n->bounds.ymax); /* NE */
    n->children[2] = ts_qtree_new(n->bounds.xmin, n->bounds.ymin, mx, my); /* SW */
    n->children[3] = ts_qtree_new(mx, n->bounds.ymin, n->bounds.xmax, my); /* SE */
    for (int i = 0; i < 4; i++) if (n->children[i]) n->children[i]->depth = n->depth + 1;
    n->is_split = 1;
}

void ts_qtree_insert(QT_Node *n, int id, const TS_GfxBBox *bbox) {
    if (!_bbox_intersects(&n->bounds, bbox)) return;

    if (n->is_split) {
        for (int i = 0; i < 4; i++)
            if (n->children[i]) ts_qtree_insert(n->children[i], id, bbox);
        return;
    }

    /* Add to this node */
    if (n->item_count >= n->item_cap) {
        n->item_cap = n->item_cap ? n->item_cap * 2 : QT_MAX_ITEMS;
        QT_Item *_t = (QT_Item *)realloc(n->items, sizeof(QT_Item) * (size_t)n->item_cap);
        if (_t) n->items = _t; else return;
    }
    n->items[n->item_count++] = (QT_Item){ id, *bbox };

    /* Split if too many items and not too deep */
    if (n->item_count > QT_MAX_ITEMS && n->depth < QT_MAX_DEPTH) {
        _qt_split(n);
        /* Re-insert items into children */
        for (int i = 0; i < n->item_count; i++)
            for (int c = 0; c < 4; c++)
                if (n->children[c]) ts_qtree_insert(n->children[c], n->items[i].id, &n->items[i].bbox);
        n->item_count = 0; /* items moved to children */
    }
}

int ts_qtree_query(const QT_Node *n, const TS_GfxBBox *viewport, int *results, int max_results) {
    if (!_bbox_intersects(&n->bounds, viewport)) return 0;
    int count = 0;

    /* Check items in this node */
    for (int i = 0; i < n->item_count && count < max_results; i++) {
        if (_bbox_intersects(&n->items[i].bbox, viewport)) {
            /* Deduplicate */
            int dup = 0;
            for (int j = 0; j < count; j++) if (results[j] == n->items[i].id) { dup = 1; break; }
            if (!dup) results[count++] = n->items[i].id;
        }
    }

    /* Recurse into children */
    if (n->is_split) {
        for (int c = 0; c < 4; c++)
            if (n->children[c])
                count += ts_qtree_query(n->children[c], viewport, results + count, max_results - count);
    }
    return count;
}

int ts_qtree_count(const QT_Node *n) {
    int c = n->item_count;
    if (n->is_split)
        for (int i = 0; i < 4; i++)
            if (n->children[i]) c += ts_qtree_count(n->children[i]);
    return c;
}

void ts_qtree_free(QT_Node *n) {
    if (!n) return;
    for (int i = 0; i < 4; i++) ts_qtree_free(n->children[i]);
    free(n->items);
    free(n);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  LOD — Level of Detail (ring simplification by zoom)                */
/* ═══════════════════════════════════════════════════════════════════ */

/* Ramer-Douglas-Peucker simplification */
static double _rdp_dist(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-10) return sqrt((px-ax)*(px-ax) + (py-ay)*(py-ay));
    double t = ((px-ax)*dx + (py-ay)*dy) / len2;
    if (t < 0) t = 0; if (t > 1) t = 1;
    double nx = ax + t*dx, ny = ay + t*dy;
    return sqrt((px-nx)*(px-nx) + (py-ny)*(py-ny));
}

static void _rdp(const double *xy, int start, int end, double eps, int *keep) {
    double maxd = 0; int idx = 0;
    for (int i = start + 1; i < end; i++) {
        double d = _rdp_dist(xy[i*2], xy[i*2+1], xy[start*2], xy[start*2+1], xy[end*2], xy[end*2+1]);
        if (d > maxd) { maxd = d; idx = i; }
    }
    if (maxd > eps) {
        keep[idx] = 1;
        _rdp(xy, start, idx, eps, keep);
        _rdp(xy, idx, end, eps, keep);
    }
}

int ts_lod_simplify(const double *xy, int npts, double epsilon, double *out) {
    if (npts <= 2) {
        memcpy(out, xy, (size_t)(npts * 2) * sizeof(double));
        return npts;
    }
    int *keep = (int *)calloc((size_t)npts, sizeof(int));
    if (!keep) return 0;
    keep[0] = 1; keep[npts-1] = 1;
    _rdp(xy, 0, npts - 1, epsilon, keep);
    int count = 0;
    for (int i = 0; i < npts; i++) {
        if (keep[i]) {
            out[count*2] = xy[i*2];
            out[count*2+1] = xy[i*2+1];
            count++;
        }
    }
    free(keep);
    return count;
}

/* Adaptive epsilon based on zoom level */
double ts_lod_epsilon(double zoom, double base_epsilon) {
    /* At zoom 1.0: full detail. At zoom 0.1: heavy simplification */
    if (zoom <= 0.01) zoom = 0.01;
    return base_epsilon / zoom;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  BATCH POLYGON RENDERING                                            */
/* ═══════════════════════════════════════════════════════════════════ */

/* Render many polygons at once with viewport culling + LOD.
   Returns number of polygons actually rendered. */

typedef struct {
    const double *coords;  /* [x0,y0, x1,y1, ...] */
    int           npts;
    uint32_t      fill_color;
    uint32_t      stroke_color;
    TS_GfxBBox       bbox;     /* pre-computed */
} TS_BatchPoly;

int ts_render_batch_polygons(TS_Canvas *canvas, const TS_BatchPoly *polys, int count,
                              const TS_GfxBBox *viewport, double zoom,
                              float ox, float oy, float scale) {
    int rendered = 0;
    double lod_eps = ts_lod_epsilon(zoom, 1.0);

    for (int i = 0; i < count; i++) {
        /* Viewport culling */
        if (!_bbox_intersects(&polys[i].bbox, viewport)) continue;

        const double *coords = polys[i].coords;
        int npts = polys[i].npts;

        /* LOD simplification if zoomed out */
        double *simplified = NULL;
        if (zoom < 0.5 && npts > 6) {
            simplified = (double *)malloc((size_t)(npts * 2) * sizeof(double));
            if (simplified) {
                npts = ts_lod_simplify(coords, npts, lod_eps, simplified);
                coords = simplified;
            }
        }

        /* Transform to screen coordinates and render */
        float *screen = (float *)malloc((size_t)(npts * 2) * sizeof(float));
        if (screen) {
            for (int j = 0; j < npts; j++) {
                screen[j*2]   = (float)((coords[j*2] - ox) * scale);
                screen[j*2+1] = (float)((coords[j*2+1] - oy) * scale);
            }

            /* Fill */
            if ((polys[i].fill_color >> 24) > 0) {
                canvas->fill_color = polys[i].fill_color;
                ts_draw_polygon_filled(canvas, screen, npts);
            }

            /* Stroke */
            if ((polys[i].stroke_color >> 24) > 0) {
                canvas->fill_color = polys[i].stroke_color;
                ts_draw_polygon(canvas, screen, npts);
            }

            rendered++;
            free(screen);
        }
        free(simplified);
    }
    return rendered;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  BENCHMARK / STATS                                                  */
/* ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int    total_entities;
    int    rendered_entities;
    int    culled_entities;
    double render_ms;
    double simplification_ratio; /* avg pts after / before */
} TS_RenderStats;

/* Simple millisecond timer (portable, not high-res) */
#include <time.h>
static double _ts_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  TERRALIS COMPAT: tgfx_* wrappers over ts_geo_* / ts_transform_* */
/*  Eliminates need for separate titan_gfx_bridge.so                 */
/* ═══════════════════════════════════════════════════════════════════ */

/* World → Screen coordinate transform */
double tgfx_w2s_x(double wx, double scale, double offset) {
    return wx * scale + offset;
}
double tgfx_w2s_y(double wy, double scale, double offset) {
    return -wy * scale + offset;
}
double tgfx_s2w_x(double sx, double scale, double offset) {
    return (sx - offset) / scale;
}
double tgfx_s2w_y(double sy, double scale, double offset) {
    return -(sy - offset) / scale;
}

/* Viewport culling: returns 1 if bbox visible, 0 if culled */
long long tgfx_cull_visible(double xmin, double ymin, double xmax, double ymax,
                             double scale, double ox, double oy,
                             double vw, double vh) {
    double sx0 = xmin * scale + ox, sx1 = xmax * scale + ox;
    double sy0 = -ymax * scale + oy, sy1 = -ymin * scale + oy;
    double mg = 10.0;
    if (sx1 < -mg || sx0 > vw + mg || sy1 < -mg || sy0 > vh + mg) return 0;
    return 1;
}

/* Distance point to segment */
double tgfx_dist_pt_seg(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-12) return sqrt((px-ax)*(px-ax) + (py-ay)*(py-ay));
    double t = ((px-ax)*dx + (py-ay)*dy) / len2;
    if (t < 0) t = 0; if (t > 1) t = 1;
    double nx = ax + t*dx, ny = ay + t*dy;
    return sqrt((px-nx)*(px-nx) + (py-ny)*(py-ny));
}

double tgfx_closest_on_seg_x(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx-ax, dy = by-ay, len2 = dx*dx+dy*dy;
    if (len2 < 1e-12) return ax;
    double t = ((px-ax)*dx + (py-ay)*dy) / len2;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return ax + t * dx;
}

double tgfx_closest_on_seg_y(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx-ax, dy = by-ay, len2 = dx*dx+dy*dy;
    if (len2 < 1e-12) return ay;
    double t = ((px-ax)*dx + (py-ay)*dy) / len2;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return ay + t * dy;
}

/* Shoelace area from packed doubles (char* = double pairs) */
double tgfx_shoelace(const char *ring_bytes, int npts) {
    const double *pts = (const double *)ring_bytes;
    double area = 0;
    for (int i = 0; i < npts; i++) {
        int j = (i + 1) % npts;
        area += pts[i*2] * pts[j*2+1] - pts[j*2] * pts[i*2+1];
    }
    return area < 0 ? -area / 2.0 : area / 2.0;
}

double tgfx_perimeter(const char *ring_bytes, int npts) {
    const double *pts = (const double *)ring_bytes;
    double peri = 0;
    for (int i = 0; i < npts; i++) {
        int j = (i + 1) % npts;
        double dx = pts[j*2] - pts[i*2], dy = pts[j*2+1] - pts[i*2+1];
        peri += sqrt(dx*dx + dy*dy);
    }
    return peri;
}

double tgfx_centroid_x(const char *ring_bytes, int npts) {
    const double *pts = (const double *)ring_bytes;
    double cx = 0, area = 0;
    for (int i = 0; i < npts; i++) {
        int j = (i + 1) % npts;
        double cross = pts[i*2]*pts[j*2+1] - pts[j*2]*pts[i*2+1];
        cx += (pts[i*2] + pts[j*2]) * cross;
        area += cross;
    }
    area /= 2.0;
    return (fabs(area) < 1e-10) ? 0 : cx / (6.0 * area);
}

double tgfx_centroid_y(const char *ring_bytes, int npts) {
    const double *pts = (const double *)ring_bytes;
    double cy = 0, area = 0;
    for (int i = 0; i < npts; i++) {
        int j = (i + 1) % npts;
        double cross = pts[i*2]*pts[j*2+1] - pts[j*2]*pts[i*2+1];
        cy += (pts[i*2+1] + pts[j*2+1]) * cross;
        area += cross;
    }
    area /= 2.0;
    return (fabs(area) < 1e-10) ? 0 : cy / (6.0 * area);
}

int tgfx_point_in_poly(const char *ring_bytes, int npts, double px, double py) {
    const double *pts = (const double *)ring_bytes;
    int inside = 0;
    for (int i = 0, j = npts - 1; i < npts; j = i++) {
        double xi = pts[i*2], yi = pts[i*2+1];
        double xj = pts[j*2], yj = pts[j*2+1];
        if ((yi > py) != (yj > py) && px < (xj-xi)*(py-yi)/(yj-yi)+xi)
            inside = !inside;
    }
    return inside;
}

const char *tgfx_version(void) { return "TITAN GFX v2.1 (unified in ts_engines.so)"; }

/* ═══════════════════════════════════════════════════════════════════ */
/*  RING CACHE — eliminate ctypes marshaling overhead                  */
/*  Store rings C-side, query by ID. 10,000 point_in_poly calls       */
/*  go from 50ms (ctypes marshal each time) to 2ms (cached).          */
/* ═══════════════════════════════════════════════════════════════════ */

#define TS_RING_CACHE_MAX 256

typedef struct {
    double *pts;    /* x0,y0, x1,y1, ... */
    int     npts;
    TS_GfxBBox bbox;
    int     used;
} _TS_CachedRing;

static _TS_CachedRing _ring_cache[TS_RING_CACHE_MAX];
static int _ring_cache_init = 0;

static void _ring_cache_ensure(void) {
    if (!_ring_cache_init) {
        memset(_ring_cache, 0, sizeof(_ring_cache));
        _ring_cache_init = 1;
    }
}

/* Store a ring, returns ring_id (0-255). -1 on error. */
int ts_ring_cache_store(const double *xy, int npts) {
    _ring_cache_ensure();
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TS_RING_CACHE_MAX; i++) {
        if (!_ring_cache[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1; /* cache full */

    _TS_CachedRing *r = &_ring_cache[slot];
    r->pts = (double *)malloc((size_t)(npts * 2) * sizeof(double));
    if (!r->pts) return -1;
    memcpy(r->pts, xy, (size_t)(npts * 2) * sizeof(double));
    r->npts = npts;
    r->used = 1;

    /* Pre-compute bbox */
    r->bbox.xmin = r->bbox.xmax = xy[0];
    r->bbox.ymin = r->bbox.ymax = xy[1];
    for (int i = 1; i < npts; i++) {
        if (xy[i*2] < r->bbox.xmin) r->bbox.xmin = xy[i*2];
        if (xy[i*2] > r->bbox.xmax) r->bbox.xmax = xy[i*2];
        if (xy[i*2+1] < r->bbox.ymin) r->bbox.ymin = xy[i*2+1];
        if (xy[i*2+1] > r->bbox.ymax) r->bbox.ymax = xy[i*2+1];
    }
    return slot;
}

/* Point-in-polygon on cached ring. No marshaling overhead. */
int ts_ring_cache_point_in(int ring_id, double px, double py) {
    if (ring_id < 0 || ring_id >= TS_RING_CACHE_MAX) return 0;
    _TS_CachedRing *r = &_ring_cache[ring_id];
    if (!r->used) return 0;

    /* Quick bbox reject */
    if (px < r->bbox.xmin || px > r->bbox.xmax ||
        py < r->bbox.ymin || py > r->bbox.ymax) return 0;

    /* Ray casting */
    int inside = 0;
    const double *pts = r->pts;
    int n = r->npts;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = pts[i*2], yi = pts[i*2+1];
        double xj = pts[j*2], yj = pts[j*2+1];
        if ((yi > py) != (yj > py) && px < (xj-xi)*(py-yi)/(yj-yi)+xi)
            inside = !inside;
    }
    return inside;
}

/* Area of cached ring (no marshal) */
double ts_ring_cache_area(int ring_id) {
    if (ring_id < 0 || ring_id >= TS_RING_CACHE_MAX) return 0;
    _TS_CachedRing *r = &_ring_cache[ring_id];
    if (!r->used) return 0;
    double area = 0;
    for (int i = 0; i < r->npts; i++) {
        int j = (i + 1) % r->npts;
        area += r->pts[i*2] * r->pts[j*2+1] - r->pts[j*2] * r->pts[i*2+1];
    }
    return fabs(area) / 2.0;
}

/* Centroid of cached ring */
void ts_ring_cache_centroid(int ring_id, double *cx, double *cy) {
    if (ring_id < 0 || ring_id >= TS_RING_CACHE_MAX) { *cx=*cy=0; return; }
    _TS_CachedRing *r = &_ring_cache[ring_id];
    if (!r->used) { *cx=*cy=0; return; }
    double sx=0,sy=0,sa=0;
    for (int i = 0; i < r->npts; i++) {
        int j = (i + 1) % r->npts;
        double cross = r->pts[i*2]*r->pts[j*2+1] - r->pts[j*2]*r->pts[i*2+1];
        sx += (r->pts[i*2]+r->pts[j*2]) * cross;
        sy += (r->pts[i*2+1]+r->pts[j*2+1]) * cross;
        sa += cross;
    }
    sa /= 2.0;
    *cx = fabs(sa) < 1e-10 ? 0 : sx / (6.0 * sa);
    *cy = fabs(sa) < 1e-10 ? 0 : sy / (6.0 * sa);
}

/* Perimeter of cached ring */
double ts_ring_cache_perimeter(int ring_id) {
    if (ring_id < 0 || ring_id >= TS_RING_CACHE_MAX) return 0;
    _TS_CachedRing *r = &_ring_cache[ring_id];
    if (!r->used) return 0;
    double peri = 0;
    for (int i = 0; i < r->npts; i++) {
        int j = (i + 1) % r->npts;
        double dx = r->pts[j*2]-r->pts[i*2], dy = r->pts[j*2+1]-r->pts[i*2+1];
        peri += sqrt(dx*dx + dy*dy);
    }
    return peri;
}

/* Batch point-in-poly: test N points against cached ring */
int ts_ring_cache_points_in_batch(int ring_id, const double *points, int npoints,
                                    int *results) {
    int count = 0;
    for (int i = 0; i < npoints; i++) {
        results[i] = ts_ring_cache_point_in(ring_id, points[i*2], points[i*2+1]);
        if (results[i]) count++;
    }
    return count;
}

/* Free a cached ring */
void ts_ring_cache_free(int ring_id) {
    if (ring_id < 0 || ring_id >= TS_RING_CACHE_MAX) return;
    _TS_CachedRing *r = &_ring_cache[ring_id];
    free(r->pts);
    r->pts = NULL; r->npts = 0; r->used = 0;
}

/* Free all cached rings */
void ts_ring_cache_clear(void) {
    for (int i = 0; i < TS_RING_CACHE_MAX; i++) ts_ring_cache_free(i);
}

/* Count cached rings */
int ts_ring_cache_count(void) {
    _ring_cache_ensure();
    int c = 0;
    for (int i = 0; i < TS_RING_CACHE_MAX; i++) if (_ring_cache[i].used) c++;
    return c;
}

