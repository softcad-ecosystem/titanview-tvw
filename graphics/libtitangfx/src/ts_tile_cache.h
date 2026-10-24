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
#ifndef TS_TILE_CACHE_H
#define TS_TILE_CACHE_H

/*
 * ts_tile_cache.h — Tile-based rendering cu cache multi-LOD
 * SoftCAD C+C TOPO © 2026
 *
 * PROBLEMA: 2900 poligoane × 15 vertecși × 30fps = 1.3M vertex/sec
 *           Pan = discard tot + re-render TOATE poligoanele
 *           Zoom = la fel
 *           Rezultat: FREEZE pe sectoare reale
 *
 * SOLUȚIA: Google Maps approach
 *
 *   ┌──────────────────────────────────────────┐
 *   │  VIEWPORT (ecran)                        │
 *   │  ┌────┬────┬────┬────┐                   │
 *   │  │T(0,0)│T(1,0)│T(2,0)│T(3,0)│           │
 *   │  ├────┼────┼────┼────┤                   │
 *   │  │T(0,1)│T(1,1)│T(2,1)│T(3,1)│           │
 *   │  └────┴────┴────┴────┘                   │
 *   └──────────────────────────────────────────┘
 *
 *   Fiecare tile = 256×256 pixeli, randat O DATĂ, cachat.
 *   Pan = shift grila + render DOAR tiles noi de la margine.
 *   Zoom = scale tiles existente (blur) + async re-render.
 *
 * MULTI-LOD:
 *   LOD 0: toate vertecșii (zoom > 1:500)
 *   LOD 1: Douglas-Peucker ε=0.5m (zoom 1:500 — 1:2000)
 *   LOD 2: Douglas-Peucker ε=2m (zoom 1:2000 — 1:10000)
 *   LOD 3: doar bbox fill (zoom < 1:10000)
 *
 * LABEL ATLAS:
 *   text_auto() calculat O DATĂ per entitate → cachat ca bitmap.
 *   Frame render = blit din cache, nu recalcul.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TC_TILE_SIZE 256
#define TC_MAX_TILES 256   /* 16×16 grid = 4096×4096 px viewport */
#define TC_MAX_LOD   4
#define TC_LABEL_MAX 4096

#pragma pack(push, 1)

typedef struct {
    int tx, ty;              /* tile grid position */
    int valid;               /* 0 = needs render, 1 = cached */
    int lod;                 /* LOD level when rendered */
    unsigned char *pixels;   /* TC_TILE_SIZE × TC_TILE_SIZE × 3 bytes RGB */
    double world_x0, world_y0;  /* world coords of tile top-left */
    double world_x1, world_y1;  /* world coords of tile bottom-right */
    int entity_count;        /* how many entities touch this tile */
    uint32_t content_hash;   /* hash of entities in tile — skip re-render if same */
} TC_Tile;

typedef struct {
    double x, y;         /* screen position */
    double font_size;
    char text[64];
    int valid;
    int visible;         /* in viewport? */
} TC_LabelEntry;

typedef struct {
    /* Viewport state */
    double view_x, view_y;       /* center of view in world coords */
    double zoom;                 /* pixels per world unit */
    int screen_w, screen_h;     /* screen size in pixels */

    /* Tile grid */
    TC_Tile tiles[TC_MAX_TILES];
    int n_tiles;
    int grid_cols, grid_rows;

    /* LOD */
    int current_lod;

    /* Label cache */
    TC_LabelEntry labels[TC_LABEL_MAX];
    int n_labels;
    int labels_dirty;

    /* Stats */
    int tiles_rendered;          /* tiles rendered this frame */
    int tiles_cached;            /* tiles served from cache */
    int vertices_processed;
    int entities_culled;
    double last_frame_ms;
} TC_Cache;

#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════ */
/*  CREATE / FREE                                              */
/* ═══════════════════════════════════════════════════════════ */

TC_Cache *ts_tile_cache_new(int screen_w, int screen_h) {
    TC_Cache *c = (TC_Cache*)calloc(1, sizeof(TC_Cache));
    c->screen_w = screen_w;
    c->screen_h = screen_h;
    c->zoom = 1.0;
    c->current_lod = 0;
    return c;
}

void ts_tile_cache_free(TC_Cache *c) {
    if (!c) return;
    for (int i = 0; i < c->n_tiles; i++)
        free(c->tiles[i].pixels);
    free(c);
}

/* ═══════════════════════════════════════════════════════════ */
/*  SET VIEWPORT — recalculate tile grid                       */
/* ═══════════════════════════════════════════════════════════ */

void ts_tile_cache_set_view(TC_Cache *c, double cx, double cy, double zoom) {
    if (!c) return;

    /* Determine LOD from zoom */
    int new_lod;
    if (zoom > 2.0) new_lod = 0;       /* 1:500 — full detail */
    else if (zoom > 0.5) new_lod = 1;  /* 1:2000 — simplified */
    else if (zoom > 0.1) new_lod = 2;  /* 1:10000 — more simplified */
    else new_lod = 3;                   /* 1:50000 — bbox only */

    int lod_changed = (new_lod != c->current_lod);
    int view_changed = (cx != c->view_x || cy != c->view_y || zoom != c->zoom);

    c->view_x = cx;
    c->view_y = cy;
    c->zoom = zoom;
    c->current_lod = new_lod;

    /* Calculate tile grid */
    c->grid_cols = (c->screen_w + TC_TILE_SIZE - 1) / TC_TILE_SIZE + 2; /* +2 for margin */
    c->grid_rows = (c->screen_h + TC_TILE_SIZE - 1) / TC_TILE_SIZE + 2;
    int needed = c->grid_cols * c->grid_rows;
    if (needed > TC_MAX_TILES) needed = TC_MAX_TILES;

    /* World coords of viewport top-left */
    double half_w = (c->screen_w / 2.0) / zoom;
    double half_h = (c->screen_h / 2.0) / zoom;
    double world_left = cx - half_w;
    double world_top = cy - half_h;
    double tile_world_size = TC_TILE_SIZE / zoom;

    /* Check which existing tiles can be reused */
    if (lod_changed) {
        /* LOD changed — invalidate ALL tiles */
        for (int i = 0; i < c->n_tiles; i++) {
            c->tiles[i].valid = 0;
        }
        c->labels_dirty = 1;
    }

    /* Set up tile grid positions */
    int tile_start_x = (int)floor(world_left / tile_world_size);
    int tile_start_y = (int)floor(world_top / tile_world_size);

    int idx = 0;
    for (int ty = 0; ty < c->grid_rows && idx < needed; ty++) {
        for (int tx = 0; tx < c->grid_cols && idx < needed; tx++) {
            int gtx = tile_start_x + tx;
            int gty = tile_start_y + ty;

            /* Check if this tile exists in cache */
            int found = -1;
            if (!lod_changed) {
                for (int i = 0; i < c->n_tiles; i++) {
                    if (c->tiles[i].tx == gtx && c->tiles[i].ty == gty && c->tiles[i].valid) {
                        found = i;
                        break;
                    }
                }
            }

            TC_Tile *t = &c->tiles[idx];
            if (found >= 0 && found != idx) {
                /* Reuse existing tile */
                TC_Tile tmp = c->tiles[found];
                c->tiles[found] = c->tiles[idx];
                c->tiles[idx] = tmp;
            } else if (found < 0) {
                /* New tile — needs rendering */
                t->tx = gtx;
                t->ty = gty;
                t->valid = 0;
                t->lod = new_lod;
                if (!t->pixels)
                    t->pixels = (unsigned char*)calloc(TC_TILE_SIZE * TC_TILE_SIZE * 3, 1);
            }
            t->world_x0 = gtx * tile_world_size;
            t->world_y0 = gty * tile_world_size;
            t->world_x1 = t->world_x0 + tile_world_size;
            t->world_y1 = t->world_y0 + tile_world_size;
            idx++;
        }
    }
    c->n_tiles = idx;
}

/* ═══════════════════════════════════════════════════════════ */
/*  QUERY: which tiles need rendering this frame?              */
/* ═══════════════════════════════════════════════════════════ */

int ts_tile_cache_dirty_count(const TC_Cache *c) {
    if (!c) return 0;
    int count = 0;
    for (int i = 0; i < c->n_tiles; i++)
        if (!c->tiles[i].valid) count++;
    return count;
}

int ts_tile_cache_get_dirty(const TC_Cache *c, int *tile_indices, int max) {
    if (!c) return 0;
    int count = 0;
    for (int i = 0; i < c->n_tiles && count < max; i++)
        if (!c->tiles[i].valid) tile_indices[count++] = i;
    return count;
}

/* Get tile world bounds for rendering */
void ts_tile_cache_tile_bounds(const TC_Cache *c, int tile_idx,
                                 double *x0, double *y0, double *x1, double *y1) {
    if (!c || tile_idx < 0 || tile_idx >= c->n_tiles) return;
    *x0 = c->tiles[tile_idx].world_x0;
    *y0 = c->tiles[tile_idx].world_y0;
    *x1 = c->tiles[tile_idx].world_x1;
    *y1 = c->tiles[tile_idx].world_y1;
}

/* Mark tile as rendered */
void ts_tile_cache_mark_valid(TC_Cache *c, int tile_idx) {
    if (!c || tile_idx < 0 || tile_idx >= c->n_tiles) return;
    c->tiles[tile_idx].valid = 1;
}

/* Get tile pixel buffer for rendering INTO */
unsigned char *ts_tile_cache_pixel_buf(TC_Cache *c, int tile_idx) {
    if (!c || tile_idx < 0 || tile_idx >= c->n_tiles) return NULL;
    return c->tiles[tile_idx].pixels;
}

/* ═══════════════════════════════════════════════════════════ */
/*  COMPOSE: blit all cached tiles to output buffer            */
/* ═══════════════════════════════════════════════════════════ */

int ts_tile_cache_compose(const TC_Cache *c, unsigned char *output, int out_w, int out_h) {
    if (!c || !output) return 0;

    memset(output, 240, out_w * out_h * 3); /* light grey background */

    int tiles_blitted = 0;
    double tile_world_size = TC_TILE_SIZE / c->zoom;
    double half_w = (c->screen_w / 2.0) / c->zoom;
    double half_h = (c->screen_h / 2.0) / c->zoom;
    double world_left = c->view_x - half_w;
    double world_top = c->view_y - half_h;

    for (int i = 0; i < c->n_tiles; i++) {
        if (!c->tiles[i].valid || !c->tiles[i].pixels) continue;

        /* Screen position of this tile */
        int sx = (int)((c->tiles[i].world_x0 - world_left) * c->zoom);
        int sy = (int)((c->tiles[i].world_y0 - world_top) * c->zoom);

        /* Blit tile to output */
        for (int ty = 0; ty < TC_TILE_SIZE; ty++) {
            int dy = sy + ty;
            if (dy < 0 || dy >= out_h) continue;
            for (int tx = 0; tx < TC_TILE_SIZE; tx++) {
                int dx = sx + tx;
                if (dx < 0 || dx >= out_w) continue;
                int src = (ty * TC_TILE_SIZE + tx) * 3;
                int dst = (dy * out_w + dx) * 3;
                output[dst] = c->tiles[i].pixels[src];
                output[dst+1] = c->tiles[i].pixels[src+1];
                output[dst+2] = c->tiles[i].pixels[src+2];
            }
        }
        tiles_blitted++;
    }
    return tiles_blitted;
}

/* ═══════════════════════════════════════════════════════════ */
/*  INVALIDATE: force re-render (after data change)            */
/* ═══════════════════════════════════════════════════════════ */

void ts_tile_cache_invalidate_all(TC_Cache *c) {
    if (!c) return;
    for (int i = 0; i < c->n_tiles; i++)
        c->tiles[i].valid = 0;
    c->labels_dirty = 1;
}

void ts_tile_cache_invalidate_region(TC_Cache *c, double x0, double y0, double x1, double y1) {
    if (!c) return;
    for (int i = 0; i < c->n_tiles; i++) {
        TC_Tile *t = &c->tiles[i];
        /* Check overlap */
        if (t->world_x1 > x0 && t->world_x0 < x1 &&
            t->world_y1 > y0 && t->world_y0 < y1)
            t->valid = 0;
    }
}

/* ═══════════════════════════════════════════════════════════ */
/*  LABEL CACHE                                                */
/* ═══════════════════════════════════════════════════════════ */

int ts_tile_cache_add_label(TC_Cache *c, double x, double y,
                              double font_size, const char *text) {
    if (!c || c->n_labels >= TC_LABEL_MAX) return -1;
    TC_LabelEntry *l = &c->labels[c->n_labels];
    l->x = x; l->y = y; l->font_size = font_size;
    strncpy(l->text, text, 63); l->text[63] = '\0';
    l->valid = 1; l->visible = 0;
    return c->n_labels++;
}

void ts_tile_cache_clear_labels(TC_Cache *c) {
    if (!c) return;
    c->n_labels = 0;
    c->labels_dirty = 1;
}

/* Update label visibility based on current viewport */
int ts_tile_cache_update_labels(TC_Cache *c) {
    if (!c) return 0;
    double half_w = (c->screen_w / 2.0) / c->zoom;
    double half_h = (c->screen_h / 2.0) / c->zoom;
    double vx0 = c->view_x - half_w, vy0 = c->view_y - half_h;
    double vx1 = c->view_x + half_w, vy1 = c->view_y + half_h;

    int visible = 0;
    for (int i = 0; i < c->n_labels; i++) {
        TC_LabelEntry *l = &c->labels[i];
        l->visible = (l->x >= vx0 && l->x <= vx1 && l->y >= vy0 && l->y <= vy1);
        if (l->visible) visible++;
    }
    c->labels_dirty = 0;
    return visible;
}

/* ═══════════════════════════════════════════════════════════ */
/*  STATS                                                      */
/* ═══════════════════════════════════════════════════════════ */

int ts_tile_cache_tile_count(const TC_Cache *c) { return c ? c->n_tiles : 0; }
int ts_tile_cache_lod(const TC_Cache *c) { return c ? c->current_lod : 0; }
int ts_tile_cache_label_count(const TC_Cache *c) { return c ? c->n_labels : 0; }
int ts_tile_cache_visible_labels(const TC_Cache *c) {
    if (!c) return 0;
    int n = 0;
    for (int i = 0; i < c->n_labels; i++)
        if (c->labels[i].visible) n++;
    return n;
}

#endif /* TS_TILE_CACHE_H */
