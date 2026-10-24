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
#ifndef TS_RENDER_MANAGER_H
#define TS_RENDER_MANAGER_H

/*
 * ts_render_manager.h — Render Manager pentru 60K+ entități
 * SoftCAD C+C TOPO © 2026
 *
 * PROBLEMA la 60K:
 *   60000 poligoane × 15 vertecși × 30fps = 27M vertex/sec
 *   Tile cache rezolvă PAN dar NU initial render
 *   R-Tree ajută dar 60K insert + query = încă lent
 *
 * SOLUȚIA: 5 tehnici combinate
 *
 *   1. SUB-PIXEL CULLING
 *      La zoom-out, un poligon de 100m² ocupă < 1px pe ecran
 *      → SKIP complet (nu-l desenezi, nu-l procesezi)
 *      Rezultat: din 60K, doar ~500-3000 sunt VIZIBILE la orice zoom
 *
 *   2. RENDER BUDGET
 *      Maxim N entități per frame (ex: 2000)
 *      Restul = progressive refinement (frame următor)
 *      Frame 1: cele mai mari 2000, Frame 2: următoarele 2000, etc.
 *      Utilizatorul vede instant cele importante, restul "apar"
 *
 *   3. SPATIAL GRID (nu R-Tree, GRID fix!)
 *      UAT 10km × 10km → grid 100×100 = 10000 celule
 *      Fiecare celulă = 100m × 100m → max ~10-20 entități
 *      Viewport query = check doar celulele vizibile
 *      Insert O(1), query O(celule_vizibile) nu O(N)
 *
 *   4. LOD PYRAMID
 *      Pre-compute la încărcare:
 *        LOD 0: original (15 vertecși/polygon)
 *        LOD 1: simplificat (8 vertecși)
 *        LOD 2: simplificat (4 vertecși)
 *        LOD 3: doar centroid + area (1 punct)
 *      Selectat automat pe baza zoom
 *
 *   5. IMPORTANCE SORTING
 *      Entitățile au "importanță" = area × visibility
 *      Cele mai mari/importante se randează PRIMELE
 *      La render budget, cele mici se sacrifică primele
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RM_INITIAL_CAPACITY 8192
#define RM_GRID_SIZE       200    /* 200×200 grid cells */
#define RM_CELL_INITIAL     16    /* grows dynamically in dense cells */
#define RM_RENDER_BUDGET  3000    /* max entities per frame */

typedef struct {
    int id;
    double bbox[4];          /* x0, y0, x1, y1 */
    double area;             /* computed area */
    double cx, cy;           /* centroid */
    float importance;        /* area-based sorting key */
    uint8_t type;            /* 0=parcel, 1=building, 2=road */
    uint8_t lod_current;     /* current LOD level */
    uint8_t visible;         /* in viewport? */
    uint8_t rendered;        /* rendered this frame? */
    /* Geometry stored externally — manager only tracks metadata */
} RM_Entity;

typedef struct {
    int *entity_ids;
    int count;
    int capacity;
} RM_Cell;

typedef struct {
    /* Entity storage */
    RM_Entity *entities;
    int n_entities;
    int cap_entities;

    /* Spatial grid */
    RM_Cell *grid;
    double grid_x0, grid_y0;    /* grid origin (world coords) */
    double cell_w, cell_h;      /* cell size in world units */
    int grid_cols, grid_rows;

    /* Viewport */
    double vp_x0, vp_y0, vp_x1, vp_y1;  /* world coords */
    double zoom;                           /* pixels per world unit */

    /* Frame state */
    int *visible_ids;            /* ids visible this frame */
    int n_visible;
    int *render_ids;             /* ids to render this frame (budget-limited) */
    int n_render;
    int list_capacity;
    int grid_overflow_count;
    int frame_number;
    int render_budget;

    /* Stats */
    int stat_total;
    int stat_subpixel_culled;
    int stat_viewport_culled;
    int stat_budget_deferred;
    int stat_rendered;
    double stat_frame_ms;
} RM_Manager;


void ts_render_mgr_free(RM_Manager *m);

/* ═══════════════════════════════════════════════════ */
/*  CREATE / FREE                                      */
/* ═══════════════════════════════════════════════════ */

static int _rm_reserve_lists(RM_Manager *m, int capacity) {
    if (!m) return 0;
    if (capacity <= m->list_capacity) return 1;
    int next = m->list_capacity > 0 ? m->list_capacity : RM_INITIAL_CAPACITY;
    while (next < capacity) {
        if (next > 1073741823) return 0;
        next *= 2;
    }
    int *visible = (int*)realloc(m->visible_ids, (size_t)next * sizeof(int));
    if (!visible) return 0;
    m->visible_ids = visible;
    int *render = (int*)realloc(m->render_ids, (size_t)next * sizeof(int));
    if (!render) return 0;
    m->render_ids = render;
    m->list_capacity = next;
    return 1;
}

static void _rm_free_grid(RM_Manager *m) {
    if (!m || !m->grid) return;
    int n_cells = m->grid_cols * m->grid_rows;
    for (int i = 0; i < n_cells; ++i) free(m->grid[i].entity_ids);
    free(m->grid);
    m->grid = NULL;
}

static int _rm_cell_append(RM_Manager *m, RM_Cell *cell, int entity_index) {
    if (!cell) return 0;
    if (cell->count >= cell->capacity) {
        int next = cell->capacity > 0 ? cell->capacity * 2 : RM_CELL_INITIAL;
        int *ids = (int*)realloc(cell->entity_ids, (size_t)next * sizeof(int));
        if (!ids) {
            if (m) m->grid_overflow_count++;
            return 0;
        }
        cell->entity_ids = ids;
        cell->capacity = next;
    }
    cell->entity_ids[cell->count++] = entity_index;
    return 1;
}

RM_Manager *ts_render_mgr_new(int budget) {
    RM_Manager *m = (RM_Manager*)calloc(1, sizeof(RM_Manager));
    if (!m) return NULL;
    m->cap_entities = RM_INITIAL_CAPACITY;
    m->entities = (RM_Entity*)calloc((size_t)m->cap_entities, sizeof(RM_Entity));
    m->render_budget = budget > 0 ? budget : RM_RENDER_BUDGET;
    if (!m->entities || !_rm_reserve_lists(m, m->cap_entities)) {
        ts_render_mgr_free(m);
        return NULL;
    }
    return m;
}

void ts_render_mgr_free(RM_Manager *m) {
    if (!m) return;
    free(m->entities);
    _rm_free_grid(m);
    free(m->visible_ids); free(m->render_ids);
    free(m);
}

/* ═══════════════════════════════════════════════════ */
/*  ADD ENTITIES                                       */
/* ═══════════════════════════════════════════════════ */

int ts_render_mgr_add(RM_Manager *m, int id, 
                        double x0, double y0, double x1, double y1,
                        double area, int type) {
    if (!m) return -1;
    if (m->n_entities >= m->cap_entities) {
        int next = m->cap_entities > 0 ? m->cap_entities * 2 : RM_INITIAL_CAPACITY;
        if (!_rm_reserve_lists(m, next)) return -1;
        RM_Entity *entities = (RM_Entity*)realloc(
            m->entities, (size_t)next * sizeof(RM_Entity));
        if (!entities) return -1;
        m->entities = entities;
        m->cap_entities = next;
    }
    RM_Entity *e = &m->entities[m->n_entities];
    e->id = id;
    e->bbox[0] = x0; e->bbox[1] = y0; e->bbox[2] = x1; e->bbox[3] = y1;
    e->area = area;
    e->cx = (x0 + x1) / 2; e->cy = (y0 + y1) / 2;
    e->importance = (float)(area > 0 ? log(area + 1) : 0);
    e->type = (uint8_t)type;
    e->lod_current = 0;
    e->visible = 0; e->rendered = 0;
    return m->n_entities++;
}

/* ═══════════════════════════════════════════════════ */
/*  BUILD SPATIAL GRID                                 */
/* ═══════════════════════════════════════════════════ */

void ts_render_mgr_build_grid(RM_Manager *m) {
    if (!m || m->n_entities == 0) return;

    /* Find extent */
    double x0 = m->entities[0].bbox[0], y0 = m->entities[0].bbox[1];
    double x1 = m->entities[0].bbox[2], y1 = m->entities[0].bbox[3];
    for (int i = 1; i < m->n_entities; i++) {
        if (m->entities[i].bbox[0] < x0) x0 = m->entities[i].bbox[0];
        if (m->entities[i].bbox[1] < y0) y0 = m->entities[i].bbox[1];
        if (m->entities[i].bbox[2] > x1) x1 = m->entities[i].bbox[2];
        if (m->entities[i].bbox[3] > y1) y1 = m->entities[i].bbox[3];
    }

    m->grid_x0 = x0 - 1; m->grid_y0 = y0 - 1;
    double w = (x1 - x0) + 2, h = (y1 - y0) + 2;

    /* Grid dimensions — aim for ~10-20 entities per cell */
    m->grid_cols = (int)ceil(sqrt(m->n_entities / 15.0));
    m->grid_rows = m->grid_cols;
    if (m->grid_cols > RM_GRID_SIZE) m->grid_cols = RM_GRID_SIZE;
    if (m->grid_rows > RM_GRID_SIZE) m->grid_rows = RM_GRID_SIZE;
    if (m->grid_cols < 10) m->grid_cols = 10;
    if (m->grid_rows < 10) m->grid_rows = 10;

    m->cell_w = w / m->grid_cols;
    m->cell_h = h / m->grid_rows;

    /* Allocate grid */
    int n_cells = m->grid_cols * m->grid_rows;
    _rm_free_grid(m);
    m->grid_overflow_count = 0;
    m->grid = (RM_Cell*)calloc((size_t)n_cells, sizeof(RM_Cell));
    if (!m->grid) return;

    /* Insert entities into grid cells */
    for (int i = 0; i < m->n_entities; i++) {
        RM_Entity *e = &m->entities[i];
        int cx0 = (int)((e->bbox[0] - m->grid_x0) / m->cell_w);
        int cy0 = (int)((e->bbox[1] - m->grid_y0) / m->cell_h);
        int cx1 = (int)((e->bbox[2] - m->grid_x0) / m->cell_w);
        int cy1 = (int)((e->bbox[3] - m->grid_y0) / m->cell_h);
        if (cx0 < 0) cx0 = 0; if (cy0 < 0) cy0 = 0;
        if (cx1 >= m->grid_cols) cx1 = m->grid_cols - 1;
        if (cy1 >= m->grid_rows) cy1 = m->grid_rows - 1;

        for (int gy = cy0; gy <= cy1; gy++) {
            for (int gx = cx0; gx <= cx1; gx++) {
                RM_Cell *cell = &m->grid[gy * m->grid_cols + gx];
                (void)_rm_cell_append(m, cell, i);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════ */
/*  FRAME: determine visible + renderable entities     */
/* ═══════════════════════════════════════════════════ */

int ts_render_mgr_frame(RM_Manager *m, 
                          double vp_x0, double vp_y0, double vp_x1, double vp_y1,
                          double zoom) {
    if (!m || !m->grid) return 0;

    m->vp_x0 = vp_x0; m->vp_y0 = vp_y0;
    m->vp_x1 = vp_x1; m->vp_y1 = vp_y1;
    m->zoom = zoom;
    m->frame_number++;

    /* Reset stats */
    m->stat_subpixel_culled = 0;
    m->stat_viewport_culled = 0;
    m->stat_budget_deferred = 0;
    m->stat_rendered = 0;
    m->n_visible = 0;
    m->n_render = 0;

    /* Sub-pixel threshold: entity smaller than 2px on screen */
    double min_world_size = 2.0 / zoom;

    /* Find visible grid cells */
    int cx0 = (int)((vp_x0 - m->grid_x0) / m->cell_w) - 1;
    int cy0 = (int)((vp_y0 - m->grid_y0) / m->cell_h) - 1;
    int cx1 = (int)((vp_x1 - m->grid_x0) / m->cell_w) + 1;
    int cy1 = (int)((vp_y1 - m->grid_y0) / m->cell_h) + 1;
    if (cx0 < 0) cx0 = 0; if (cy0 < 0) cy0 = 0;
    if (cx1 >= m->grid_cols) cx1 = m->grid_cols - 1;
    if (cy1 >= m->grid_rows) cy1 = m->grid_rows - 1;

    /* Reset visibility */
    for (int i = 0; i < m->n_entities; i++) {
        m->entities[i].visible = 0;
        m->entities[i].rendered = 0;
    }

    /* Collect visible entities from grid cells */
    for (int gy = cy0; gy <= cy1; gy++) {
        for (int gx = cx0; gx <= cx1; gx++) {
            RM_Cell *cell = &m->grid[gy * m->grid_cols + gx];
            for (int ci = 0; ci < cell->count; ci++) {
                int ei = cell->entity_ids[ci];
                RM_Entity *e = &m->entities[ei];
                if (e->visible) continue; /* already counted */

                /* Viewport test */
                if (e->bbox[2] < vp_x0 || e->bbox[0] > vp_x1 ||
                    e->bbox[3] < vp_y0 || e->bbox[1] > vp_y1) {
                    m->stat_viewport_culled++;
                    continue;
                }

                /* Sub-pixel test */
                double ew = e->bbox[2] - e->bbox[0];
                double eh = e->bbox[3] - e->bbox[1];
                if (ew < min_world_size && eh < min_world_size) {
                    m->stat_subpixel_culled++;
                    continue;
                }

                e->visible = 1;
                m->visible_ids[m->n_visible++] = ei;
            }
        }
    }

    /* LOD assignment based on zoom */
    for (int i = 0; i < m->n_visible; i++) {
        RM_Entity *e = &m->entities[m->visible_ids[i]];
        double screen_size = fmax(e->bbox[2] - e->bbox[0], e->bbox[3] - e->bbox[1]) * zoom;
        if (screen_size > 100) e->lod_current = 0;       /* full detail */
        else if (screen_size > 30) e->lod_current = 1;   /* simplified */
        else if (screen_size > 10) e->lod_current = 2;   /* more simplified */
        else e->lod_current = 3;                           /* bbox/dot only */
    }

    /* Partial sort: only need top render_budget, not full sort */
    /* Use partial selection: find threshold, then collect */
    if (m->n_visible > m->render_budget * 2) {
        /* Quick partition: swap most important to front */
        float threshold = 0;
        /* Sample 100 random items to estimate threshold */
        float samples[100];
        int ns = m->n_visible < 100 ? m->n_visible : 100;
        for (int i = 0; i < ns; i++) {
            int idx = (i * m->n_visible) / ns;
            samples[i] = m->entities[m->visible_ids[idx]].importance;
        }
        /* Sort samples */
        for (int i = 0; i < ns-1; i++)
            for (int j = i+1; j < ns; j++)
                if (samples[j] > samples[i]) { float t=samples[i]; samples[i]=samples[j]; samples[j]=t; }
        int th_idx = (m->render_budget * ns) / m->n_visible;
        if (th_idx >= ns) th_idx = ns - 1;
        threshold = samples[th_idx];
        /* Partition: important ones to front */
        int front = 0;
        for (int i = 0; i < m->n_visible && front < m->render_budget; i++) {
            if (m->entities[m->visible_ids[i]].importance >= threshold) {
                int tmp = m->visible_ids[front];
                m->visible_ids[front] = m->visible_ids[i];
                m->visible_ids[i] = tmp;
                front++;
            }
        }
    }

    /* Apply render budget */
    m->n_render = m->n_visible;
    if (m->n_render > m->render_budget) {
        m->stat_budget_deferred = m->n_render - m->render_budget;
        m->n_render = m->render_budget;
    }

    /* Copy render list */
    for (int i = 0; i < m->n_render; i++) {
        m->render_ids[i] = m->visible_ids[i];
        m->entities[m->visible_ids[i]].rendered = 1;
    }
    m->stat_rendered = m->n_render;
    m->stat_total = m->n_entities;

    return m->n_render;
}

/* ═══════════════════════════════════════════════════ */
/*  QUERY: what to render                              */
/* ═══════════════════════════════════════════════════ */

int ts_render_mgr_render_count(const RM_Manager *m) { return m ? m->n_render : 0; }
int ts_render_mgr_visible_count(const RM_Manager *m) { return m ? m->n_visible : 0; }

int ts_render_mgr_render_id(const RM_Manager *m, int idx) {
    if (!m || idx < 0 || idx >= m->n_render) return -1;
    return m->entities[m->render_ids[idx]].id;
}

int ts_render_mgr_render_lod(const RM_Manager *m, int idx) {
    if (!m || idx < 0 || idx >= m->n_render) return 3;
    return m->entities[m->render_ids[idx]].lod_current;
}

/* Stats */
int ts_render_mgr_stat_total(const RM_Manager *m) { return m ? m->stat_total : 0; }
int ts_render_mgr_stat_culled(const RM_Manager *m) { return m ? m->stat_viewport_culled : 0; }
int ts_render_mgr_stat_subpixel(const RM_Manager *m) { return m ? m->stat_subpixel_culled : 0; }
int ts_render_mgr_stat_deferred(const RM_Manager *m) { return m ? m->stat_budget_deferred : 0; }
int ts_render_mgr_stat_rendered(const RM_Manager *m) { return m ? m->stat_rendered : 0; }

int ts_render_mgr_capacity(const RM_Manager *m) { return m ? m->cap_entities : 0; }
int ts_render_mgr_grid_overflow_count(const RM_Manager *m) {
    return m ? m->grid_overflow_count : 0;
}

/* Pick entity at world coords */
int ts_render_mgr_pick(const RM_Manager *m, double wx, double wy) {
    if (!m || !m->grid) return -1;
    int gx = (int)((wx - m->grid_x0) / m->cell_w);
    int gy = (int)((wy - m->grid_y0) / m->cell_h);
    if (gx < 0 || gx >= m->grid_cols || gy < 0 || gy >= m->grid_rows) return -1;

    RM_Cell *cell = &m->grid[gy * m->grid_cols + gx];
    int best = -1;
    double best_area = 1e30;
    for (int i = 0; i < cell->count; i++) {
        RM_Entity *e = &m->entities[cell->entity_ids[i]];
        if (wx >= e->bbox[0] && wx <= e->bbox[2] &&
            wy >= e->bbox[1] && wy <= e->bbox[3]) {
            if (e->area < best_area) { /* smallest = most precise pick */
                best_area = e->area;
                best = e->id;
            }
        }
    }
    return best;
}

#endif /* TS_RENDER_MANAGER_H */
