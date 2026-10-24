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
#ifndef TS_SPATIAL_INDEX_H
#define TS_SPATIAL_INDEX_H
/*
 * R-Tree Spatial Index — O(log n) queries instead of O(n^2)
 * Used by: CGXML, CSV, Audit, GEO, Render
 * SoftCAD C+C TOPO (C) 2026
 */
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define TS_RTREE_M 8  /* max children per node */

typedef struct { double x_min, y_min, x_max, y_max; } TS_RBBox;
typedef struct _TS_RNode {
    TS_RBBox bbox;
    int is_leaf;
    int count;
    struct _TS_RNode *children[TS_RTREE_M];
    int data_ids[TS_RTREE_M];  /* leaf: IDs of stored items */
    TS_RBBox data_bboxes[TS_RTREE_M]; /* leaf: bboxes of items */
} TS_RNode;

typedef struct {
    TS_RNode *root;
    int count;
} TS_RTree;

static TS_RBBox _bbox_empty(void) {
    TS_RBBox b = {DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX}; return b;
}
static TS_RBBox _bbox_expand(TS_RBBox a, TS_RBBox b) {
    if (b.x_min < a.x_min) a.x_min = b.x_min;
    if (b.y_min < a.y_min) a.y_min = b.y_min;
    if (b.x_max > a.x_max) a.x_max = b.x_max;
    if (b.y_max > a.y_max) a.y_max = b.y_max;
    return a;
}
static double _bbox_area(TS_RBBox b) {
    return (b.x_max-b.x_min)*(b.y_max-b.y_min);
}
static int _bbox_overlaps(TS_RBBox a, TS_RBBox b) {
    return a.x_min<=b.x_max && a.x_max>=b.x_min && a.y_min<=b.y_max && a.y_max>=b.y_min;
}

static TS_RNode *_rnode_new(int is_leaf) {
    TS_RNode *n = (TS_RNode*)calloc(1, sizeof(TS_RNode));
    if (n) { n->is_leaf = is_leaf; n->bbox = _bbox_empty(); }
    return n;
}

TS_RTree *ts_rtree_new(void) {
    TS_RTree *t = (TS_RTree*)calloc(1, sizeof(TS_RTree));
    if (t) t->root = _rnode_new(1);
    return t;
}

static void _rnode_free(TS_RNode *n) {
    if (!n) return;
    if (!n->is_leaf) for (int i=0;i<n->count;i++) _rnode_free(n->children[i]);
    free(n);
}

void ts_rtree_free(TS_RTree *t) {
    if (t) { _rnode_free(t->root); free(t); }
}

/* Insert — simplified (no split, linear scan for best child) */
static void _rnode_insert(TS_RNode *n, TS_RBBox bb, int id) {
    n->bbox = _bbox_expand(n->bbox, bb);
    if (n->is_leaf) {
        if (n->count < TS_RTREE_M) {
            n->data_bboxes[n->count] = bb;
            n->data_ids[n->count] = id;
            n->count++;
        } else {
            /* Split: convert to internal, create 2 children */
            TS_RNode *c1 = _rnode_new(1), *c2 = _rnode_new(1);
            /* Simple split: first half to c1, second to c2 */
            for (int i=0; i<TS_RTREE_M/2; i++) {
                c1->data_bboxes[c1->count]=n->data_bboxes[i];
                c1->data_ids[c1->count]=n->data_ids[i]; c1->count++;
                c1->bbox=_bbox_expand(c1->bbox,n->data_bboxes[i]);
            }
            for (int i=TS_RTREE_M/2; i<TS_RTREE_M; i++) {
                c2->data_bboxes[c2->count]=n->data_bboxes[i-TS_RTREE_M/2+c2->count]=n->data_bboxes[i];
                c2->data_ids[c2->count]=n->data_ids[i]; c2->count++;
                c2->bbox=_bbox_expand(c2->bbox,n->data_bboxes[i]);
            }
            /* Add new item to best child */
            if (_bbox_area(_bbox_expand(c1->bbox,bb)) < _bbox_area(_bbox_expand(c2->bbox,bb)))
                _rnode_insert(c1, bb, id);
            else
                _rnode_insert(c2, bb, id);
            n->is_leaf = 0; n->count = 2;
            n->children[0] = c1; n->children[1] = c2;
        }
    } else {
        /* Find child with minimum area enlargement */
        int best=0; double min_enlarge=DBL_MAX;
        for (int i=0;i<n->count;i++) {
            double enlarge = _bbox_area(_bbox_expand(n->children[i]->bbox,bb)) - _bbox_area(n->children[i]->bbox);
            if (enlarge < min_enlarge) { min_enlarge=enlarge; best=i; }
        }
        _rnode_insert(n->children[best], bb, id);
        n->children[best]->bbox = _bbox_expand(n->children[best]->bbox, bb);
    }
}

void ts_rtree_insert(TS_RTree *t, double x_min, double y_min, double x_max, double y_max, int id) {
    if (!t || !t->root) return;
    TS_RBBox bb = {x_min, y_min, x_max, y_max};
    _rnode_insert(t->root, bb, id);
    t->count++;
}

/* Query: find all items overlapping bbox */
static int _rnode_query(TS_RNode *n, TS_RBBox q, int *results, int max, int count) {
    if (!n || count >= max) return count;
    if (!_bbox_overlaps(n->bbox, q)) return count;
    if (n->is_leaf) {
        for (int i=0;i<n->count && count<max;i++)
            if (_bbox_overlaps(n->data_bboxes[i], q))
                results[count++] = n->data_ids[i];
    } else {
        for (int i=0;i<n->count;i++)
            count = _rnode_query(n->children[i], q, results, max, count);
    }
    return count;
}

int ts_rtree_query(TS_RTree *t, double x_min, double y_min, double x_max, double y_max,
                    int *results, int max) {
    if (!t || !t->root) return 0;
    TS_RBBox q = {x_min, y_min, x_max, y_max};
    return _rnode_query(t->root, q, results, max, 0);
}

/* Nearest neighbor (approximate) */
int ts_rtree_nearest(TS_RTree *t, double x, double y, double *out_dist) {
    if (!t || !t->root) return -1;
    /* Simple: query expanding bbox until we find something */
    int result = -1; double best = DBL_MAX;
    double r = 10;
    while (result < 0 && r < 1e8) {
        int ids[16]; TS_RBBox q = {x-r, y-r, x+r, y+r};
        int n = _rnode_query(t->root, q, ids, 16, 0);
        for (int i=0;i<n;i++) result = ids[i]; /* take first */
        r *= 4;
    }
    if (out_dist) *out_dist = best;
    return result;
}

/* Point insert helper */
void ts_rtree_insert_point(TS_RTree *t, double x, double y, int id) {
    ts_rtree_insert(t, x, y, x, y, id);
}

/* Count items */
int ts_rtree_count(TS_RTree *t) { return t ? t->count : 0; }


/* Void* wrappers for cross-header access (no type dependency) */
void *_ts_rtree_create(void) { return (void*)ts_rtree_new(); }
void  _ts_rtree_destroy(void *rt) { ts_rtree_free((TS_RTree*)rt); }
void  _ts_rtree_add(void *rt, double x0, double y0, double x1, double y1, int id) {
    ts_rtree_insert((TS_RTree*)rt, x0, y0, x1, y1, id);
}
int   _ts_rtree_find(void *rt, double x0, double y0, double x1, double y1, int *out, int max) {
    return ts_rtree_query((TS_RTree*)rt, x0, y0, x1, y1, out, max);
}

#endif
