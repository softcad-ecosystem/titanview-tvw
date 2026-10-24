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
#ifndef TS_ALGORITHMS_H
#define TS_ALGORITHMS_H
/*
 * Advanced Algorithms — Quicksort, Douglas-Peucker, Hash Table, 
 * Streaming Stats, Voronoi helpers
 * SoftCAD C+C TOPO (C) 2026
 */
#include "ts_outbuf.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══ QUICKSORT ═══════════════════════════════════════════════ */

static void _qs_swap(double *a, int *idx, int i, int j) {
    double t=a[i]; a[i]=a[j]; a[j]=t;
    int ti=idx[i]; idx[i]=idx[j]; idx[j]=ti;
}

static void _qs_sort(double *a, int *idx, int lo, int hi) {
    if (lo >= hi) return;
    double pivot = a[(lo+hi)/2];
    int i=lo, j=hi;
    while (i<=j) {
        while (a[i]<pivot) i++;
        while (a[j]>pivot) j--;
        if (i<=j) { _qs_swap(a,idx,i,j); i++; j--; }
    }
    _qs_sort(a,idx,lo,j);
    _qs_sort(a,idx,i,hi);
}

/* Sort array of doubles, return sorted indices */
void ts_quicksort(double *values, int *indices, int n) {
    if (!values||!indices||n<2) return;
    for (int i=0;i<n;i++) indices[i]=i;
    _qs_sort(values, indices, 0, n-1);
}

/* Sort integer array */
void ts_quicksort_int(int *values, int n) {
    if (!values||n<2) return;
    int pivot=values[n/2]; int i=0,j=n-1;
    while(i<=j){
        while(values[i]<pivot)i++;
        while(values[j]>pivot)j--;
        if(i<=j){int t=values[i];values[i]=values[j];values[j]=t;i++;j--;}
    }
    if(j>0) ts_quicksort_int(values,j+1);
    if(i<n) ts_quicksort_int(values+i,n-i);
}

/* ═══ DOUGLAS-PEUCKER LINE SIMPLIFICATION ═════════════════════ */

static double _dp_dist(double px, double py, double ax, double ay, double bx, double by) {
    double dx=bx-ax, dy=by-ay;
    double len2=dx*dx+dy*dy;
    if (len2<1e-20) return sqrt((px-ax)*(px-ax)+(py-ay)*(py-ay));
    double t=((px-ax)*dx+(py-ay)*dy)/len2;
    if(t<0)t=0; if(t>1)t=1;
    double nx=ax+t*dx-px, ny=ay+t*dy-py;
    return sqrt(nx*nx+ny*ny);
}

static void _dp_simplify(const double *x, const double *y, int start, int end,
                          double epsilon, int *keep) {
    double max_d=0; int idx=start;
    for (int i=start+1;i<end;i++) {
        double d=_dp_dist(x[i],y[i],x[start],y[start],x[end],y[end]);
        if(d>max_d){max_d=d;idx=i;}
    }
    if (max_d>epsilon) {
        keep[idx]=1;
        _dp_simplify(x,y,start,idx,epsilon,keep);
        _dp_simplify(x,y,idx,end,epsilon,keep);
    }
}

/* Douglas-Peucker: simplify polyline, returns new point count */
int ts_simplify_line(const double *x_in, const double *y_in, int n,
                      double epsilon, double *x_out, double *y_out) {
    if (n<3||!x_in||!y_in||!x_out||!y_out) {
        if(x_out&&y_out&&x_in&&y_in) {memcpy(x_out,x_in,sizeof(double)*(size_t)n);memcpy(y_out,y_in,sizeof(double)*(size_t)n);}
        return n;
    }
    int *keep=(int*)calloc((size_t)n,sizeof(int));
    keep[0]=1; keep[n-1]=1;
    _dp_simplify(x_in,y_in,0,n-1,epsilon,keep);
    int out=0;
    for(int i=0;i<n;i++) if(keep[i]){x_out[out]=x_in[i];y_out[out]=y_in[i];out++;}
    free(keep);
    return out;
}

/* Adaptive simplification: epsilon varies with local curvature */
int ts_simplify_adaptive(const double *x_in, const double *y_in, int n,
                          double base_epsilon, double *x_out, double *y_out) {
    if (n<5||!x_in||!y_in||!x_out||!y_out) return ts_simplify_line(x_in,y_in,n,base_epsilon,x_out,y_out);
    /* Compute local curvature at each point */
    double *curvature=(double*)calloc((size_t)n,sizeof(double));
    for(int i=1;i<n-1;i++){
        double ax=x_in[i]-x_in[i-1], ay=y_in[i]-y_in[i-1];
        double bx=x_in[i+1]-x_in[i], by=y_in[i+1]-y_in[i];
        double cross=fabs(ax*by-ay*bx);
        double la=sqrt(ax*ax+ay*ay), lb=sqrt(bx*bx+by*by);
        curvature[i]=(la>0.01&&lb>0.01)?cross/(la*lb):0;
    }
    /* Variable epsilon: high curvature → small epsilon (keep detail) */
    int *keep=(int*)calloc((size_t)n,sizeof(int));
    keep[0]=1; keep[n-1]=1;
    for(int i=1;i<n-1;i++){
        double local_eps = base_epsilon / (1.0 + curvature[i]*10.0);
        /* Keep point if removing it would cause error > local_eps */
        double d=_dp_dist(x_in[i],y_in[i],x_in[i-1],y_in[i-1],x_in[i+1],y_in[i+1]);
        if(d>local_eps) keep[i]=1;
    }
    int out=0;
    for(int i=0;i<n;i++) if(keep[i]){x_out[out]=x_in[i];y_out[out]=y_in[i];out++;}
    free(keep); free(curvature);
    return out;
}

/* ═══ HASH TABLE (string → int) ═══════════════════════════════ */

#define TS_HT_SIZE 4096

typedef struct _TS_HTEntry {
    char *key; int value;
    struct _TS_HTEntry *next;
} TS_HTEntry;

typedef struct {
    TS_HTEntry *buckets[TS_HT_SIZE];
    int count;
} TS_HashTable;

static unsigned _ht_hash(const char *s) {
    unsigned h=5381;
    while(*s) h=((h<<5)+h)+(unsigned char)*s++;
    return h%TS_HT_SIZE;
}

TS_HashTable *ts_hashtable_new(void) {
    return (TS_HashTable*)calloc(1,sizeof(TS_HashTable));
}

void ts_hashtable_free(TS_HashTable *ht) {
    if(!ht) return;
    for(int i=0;i<TS_HT_SIZE;i++){
        TS_HTEntry *e=ht->buckets[i];
        while(e){TS_HTEntry *n=e->next;free(e->key);free(e);e=n;}
    }
    free(ht);
}

void ts_hashtable_set(TS_HashTable *ht, const char *key, int value) {
    if(!ht||!key) return;
    unsigned h=_ht_hash(key);
    TS_HTEntry *e=ht->buckets[h];
    while(e){if(strcmp(e->key,key)==0){e->value=value;return;}e=e->next;}
    e=(TS_HTEntry*)malloc(sizeof(TS_HTEntry));
    e->key=strdup(key); e->value=value; e->next=ht->buckets[h];
    ht->buckets[h]=e; ht->count++;
}

int ts_hashtable_get(TS_HashTable *ht, const char *key, int default_val) {
    if(!ht||!key) return default_val;
    unsigned h=_ht_hash(key);
    TS_HTEntry *e=ht->buckets[h];
    while(e){if(strcmp(e->key,key)==0) return e->value; e=e->next;}
    return default_val;
}

int ts_hashtable_has(TS_HashTable *ht, const char *key) {
    if(!ht||!key) return 0;
    unsigned h=_ht_hash(key);
    TS_HTEntry *e=ht->buckets[h];
    while(e){if(strcmp(e->key,key)==0) return 1; e=e->next;}
    return 0;
}

/* ═══ STREAMING STATISTICS ════════════════════════════════════ */
/* Single-pass: mean, variance, min, max, count — O(1) memory */

typedef struct {
    int count;
    double sum, sum_sq;
    double min_val, max_val;
    /* Welford's online algorithm for stable variance */
    double mean, m2;
} TS_StreamStats;

TS_StreamStats ts_stats_new(void) {
    TS_StreamStats s = {0, 0, 0, 1e30, -1e30, 0, 0};
    return s;
}

void ts_stats_add(TS_StreamStats *s, double val) {
    if (!s) return;
    s->count++;
    s->sum += val; s->sum_sq += val*val;
    if (val < s->min_val) s->min_val = val;
    if (val > s->max_val) s->max_val = val;
    /* Welford */
    double delta = val - s->mean;
    s->mean += delta / s->count;
    double delta2 = val - s->mean;
    s->m2 += delta * delta2;
}

double ts_stats_mean(const TS_StreamStats *s) { return s&&s->count>0 ? s->mean : 0; }
double ts_stats_variance(const TS_StreamStats *s) { return s&&s->count>1 ? s->m2/(s->count-1) : 0; }
double ts_stats_stddev(const TS_StreamStats *s) { return sqrt(ts_stats_variance(s)); }
double ts_stats_min(const TS_StreamStats *s) { return s ? s->min_val : 0; }
double ts_stats_max(const TS_StreamStats *s) { return s ? s->max_val : 0; }
int    ts_stats_count(const TS_StreamStats *s) { return s ? s->count : 0; }

/* Z-score: how many std devs from mean */
double ts_stats_zscore(const TS_StreamStats *s, double val) {
    double sd = ts_stats_stddev(s);
    return sd > 1e-12 ? (val - ts_stats_mean(s)) / sd : 0;
}

/* ═══ UUID v7 (timestamp-sortable) ════════════════════════════ */
int ts_random_bytes(void *buf, int len);   /* ts_titan_crypto.h */
int ts_uuid_v7(char *buf, int cap) {
    if (!buf || cap < 37) return 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    /* 48 bits timestamp + 4 bits version (7) + 12 bits random + 2 bits variant + 62 bits random */
    unsigned char bytes[16];
    /* Timestamp in first 6 bytes */
    bytes[0]=(ms>>40)&0xFF; bytes[1]=(ms>>32)&0xFF; bytes[2]=(ms>>24)&0xFF;
    bytes[3]=(ms>>16)&0xFF; bytes[4]=(ms>>8)&0xFF;  bytes[5]=ms&0xFF;
    /* Random for rest — 4.66: din sursa OS (ts_random_bytes). Înainte rand()
       nesemănat: aceeași secvență în fiecare proces → două procese în
       aceeași milisecundă dădeau UUID identice; plus strica starea rand()
       a aplicației gazdă. */
    if (!ts_random_bytes(bytes + 6, 10)) {
        uint64_t h = ms ^ ((uint64_t)ts.tv_nsec << 20) ^ (uint64_t)(uintptr_t)buf;
        for (int i = 6; i < 16; i++) {
            h += 0x9E3779B97F4A7C15ULL;
            uint64_t z = h;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            bytes[i] = (unsigned char)((z ^ (z >> 31)) >> 24);
        }
    }
    bytes[6]=(bytes[6]&0x0F)|0x70; /* version 7 */
    bytes[8]=(bytes[8]&0x3F)|0x80; /* variant 2 */
    snprintf(buf,cap,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
        bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return 36;
}



/* ═══ CSV HASH JOIN — O(n+m) instead of O(n*m) ═══════════════ */

typedef struct {
    int *row_indices;  /* row indices from left table that match */
    int count, cap;
} TS_JoinBucket;


/* ═══ DBSCAN CLUSTERING — for anomaly grouping ═══════════════ */

typedef struct {
    double x, y;
    int cluster;  /* -1 = noise, 0+ = cluster id */
    int visited;
} TS_DBScanPoint;

/* Find neighbors within eps distance */
static int _dbscan_region(TS_DBScanPoint *pts, int n, int idx,
                           double eps, int *neighbors, int max_n) {
    int count = 0;
    double eps2 = eps * eps;
    for (int i = 0; i < n && count < max_n; i++) {
        double dx = pts[i].x - pts[idx].x, dy = pts[i].y - pts[idx].y;
        if (dx*dx + dy*dy <= eps2) neighbors[count++] = i;
    }
    return count;
}

/* DBSCAN: cluster points by spatial density
   Returns number of clusters found
   pts[i].cluster = cluster ID (0+) or -1 (noise) */
int ts_dbscan(double *x, double *y, int n,
               double eps, int min_pts,
               int *cluster_ids) {
    if (!x || !y || !cluster_ids || n < 1) return 0;

    TS_DBScanPoint *pts = (TS_DBScanPoint*)calloc((size_t)n, sizeof(TS_DBScanPoint));
    int *neighbors = (int*)malloc(sizeof(int) * (size_t)n);
    int *seed_queue = (int*)malloc(sizeof(int) * (size_t)n);
    if (!pts || !neighbors || !seed_queue) {
        free(pts); free(neighbors); free(seed_queue); return 0;
    }

    for (int i = 0; i < n; i++) {
        pts[i].x = x[i]; pts[i].y = y[i];
        pts[i].cluster = -1; pts[i].visited = 0;
    }

    int cluster_id = 0;
    for (int i = 0; i < n; i++) {
        if (pts[i].visited) continue;
        pts[i].visited = 1;

        int nn = _dbscan_region(pts, n, i, eps, neighbors, n);
        if (nn < min_pts) { pts[i].cluster = -1; continue; }

        /* Expand cluster */
        pts[i].cluster = cluster_id;
        int sq_len = 0;
        for (int k = 0; k < nn; k++)
            if (neighbors[k] != i) seed_queue[sq_len++] = neighbors[k];

        int sq_pos = 0;
        while (sq_pos < sq_len) {
            int j = seed_queue[sq_pos++];
            if (!pts[j].visited) {
                pts[j].visited = 1;
                int *nb2 = (int*)malloc(sizeof(int) * (size_t)n);
                if (nb2) {
                    int nn2 = _dbscan_region(pts, n, j, eps, nb2, n);
                    if (nn2 >= min_pts) {
                        for (int k = 0; k < nn2 && sq_len < n; k++) {
                            int dup = 0;
                            for (int s = 0; s < sq_len && !dup; s++)
                                if (seed_queue[s] == nb2[k]) dup = 1;
                            if (!dup) seed_queue[sq_len++] = nb2[k];
                        }
                    }
                    free(nb2);
                }
            }
            if (pts[j].cluster < 0) pts[j].cluster = cluster_id;
        }
        cluster_id++;
    }

    for (int i = 0; i < n; i++) cluster_ids[i] = pts[i].cluster;
    free(pts); free(neighbors); free(seed_queue);
    return cluster_id;
}

/* ═══ VORONOI DIAGRAM (Fortune's algorithm simplified) ════════ */
/* For topology: compute which centroid is nearest to each pixel */

/* Brute-force Voronoi for small N (cadastral: <1000 parcels) */
int ts_voronoi_nearest(const double *cx, const double *cy, int n,
                        double px, double py) {
    if (!cx || !cy || n < 1) return -1;
    int best = 0;
    double best_d = (px-cx[0])*(px-cx[0]) + (py-cy[0])*(py-cy[0]);
    for (int i = 1; i < n; i++) {
        double d = (px-cx[i])*(px-cx[i]) + (py-cy[i])*(py-cy[i]);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

/* Compute Voronoi neighbors (shapes that share a Voronoi edge) */
int ts_voronoi_neighbors(const double *cx, const double *cy, int n,
                          int shape_id, int *neighbors, int max_n) {
    if (!cx || !cy || !neighbors || n < 2 || shape_id < 0 || shape_id >= n) return 0;
    int count = 0;
    /* Two shapes are Voronoi neighbors if there exists a point
       equidistant to both and closer to them than any other */
    /* Simplified: check midpoints between shape_id and all others */
    for (int i = 0; i < n && count < max_n; i++) {
        if (i == shape_id) continue;
        double mx = (cx[shape_id] + cx[i]) / 2;
        double my = (cy[shape_id] + cy[i]) / 2;
        /* Check if midpoint's nearest two are shape_id and i */
        double d_self = (mx-cx[shape_id])*(mx-cx[shape_id]) + (my-cy[shape_id])*(my-cy[shape_id]);
        int is_neighbor = 1;
        for (int j = 0; j < n; j++) {
            if (j == shape_id || j == i) continue;
            double d_j = (mx-cx[j])*(mx-cx[j]) + (my-cy[j])*(my-cy[j]);
            if (d_j < d_self - 0.01) { is_neighbor = 0; break; }
        }
        if (is_neighbor) neighbors[count++] = i;
    }
    return count;
}

/* ═══ XML STRUCTURAL DIFF ═════════════════════════════════════ */


/* ═══ SDF FONT ATLAS — resolution-independent text ═══════════ */
/* Pre-compute SDF for each ASCII glyph from bitmap font.
   Render text at ANY size with perfect AA. Valve technique applied to 2D. */

typedef struct {
    double *fields;   /* 95 glyphs * 16*16 = 24320 doubles */
    int glyph_w, glyph_h;  /* SDF grid size per glyph (16x16) */
} TS_SDFFont;

TS_SDFFont *ts_sdf_font_new(void) {
    TS_SDFFont *f = (TS_SDFFont*)calloc(1, sizeof(TS_SDFFont));
    if (!f) return NULL;
    f->glyph_w = 16; f->glyph_h = 16;
    f->fields = (double*)calloc(95 * 16 * 16, sizeof(double));
    if (!f->fields) { free(f); return NULL; }

    /* Generate SDF from bitmap font (5x7 → 16x16 SDF) */
    extern const unsigned char _font5x7[95][7]; /* from render engine */
    for (int g = 0; g < 95; g++) {
        double *sdf = f->fields + g * 256;
        for (int sy = 0; sy < 16; sy++) {
            for (int sx = 0; sx < 16; sx++) {
                /* Map SDF coords to bitmap coords */
                double bx = sx * 5.0 / 16.0;
                double by = sy * 7.0 / 16.0;
                /* Find min distance to any set pixel */
                double min_d = 100;
                int inside = 0;
                for (int row = 0; row < 7; row++) {
                    unsigned char bits = _font5x7[g][row];
                    for (int col = 0; col < 5; col++) {
                        if (bits & (0x10 >> col)) {
                            double dx = bx - col, dy = by - row;
                            double d = sqrt(dx*dx + dy*dy);
                            if (d < min_d) min_d = d;
                            if ((int)(bx+0.5)==col && (int)(by+0.5)==row) inside=1;
                        }
                    }
                }
                sdf[sy * 16 + sx] = inside ? -min_d : min_d;
            }
        }
    }
    return f;
}

void ts_sdf_font_free(TS_SDFFont *f) {
    if (f) { free(f->fields); free(f); }
}

/* Render SDF text at arbitrary scale */
void ts_sdf_font_render(TS_SDFFont *font, void *rb_ptr,
                         int x, int y, const char *text,
                         int r, int g, int b, double scale) {
    if (!font || !rb_ptr || !text || scale <= 0) return;
    TS_RenderBuf *rb = (TS_RenderBuf*)rb_ptr;
    int cx = x;
    int gw = (int)(font->glyph_w * scale);
    int gh = (int)(font->glyph_h * scale);
    double softness = 0.8 / scale;
    if (softness < 0.3) softness = 0.3;

    while (*text) {
        int gi = *text - 32;
        if (gi >= 0 && gi < 95) {
            double *sdf = font->fields + gi * 256;
            for (int py = 0; py < gh; py++) {
                for (int px = 0; px < gw; px++) {
                    /* Sample SDF with bilinear interpolation */
                    double u = px / scale, v = py / scale;
                    int iu = (int)u, iv = (int)v;
                    if (iu >= 15) iu = 14; if (iv >= 15) iv = 14;
                    double fu = u - iu, fv = v - iv;
                    double d00 = sdf[iv*16+iu], d10 = sdf[iv*16+iu+1];
                    double d01 = sdf[(iv+1)*16+iu], d11 = sdf[(iv+1)*16+iu+1];
                    double d = d00*(1-fu)*(1-fv) + d10*fu*(1-fv) + d01*(1-fu)*fv + d11*fu*fv;
                    /* Smoothstep for AA */
                    double t = (d - softness) / (-2*softness);
                    if (t < 0) t = 0; if (t > 1) t = 1;
                    double alpha = t * t * (3 - 2*t);
                    if (alpha > 0.01)
                        _rend_set_pixel(rb, cx+px, y+py, r, g, b, (int)(alpha*255));
                }
            }
        }
        cx += gw + (int)(scale * 1.5);
        text++;
    }
}

/* ═══ KNUTH-PLASS LINE BREAKING (simplified) ═════════════════ */
/* Optimal paragraph breaking — LaTeX quality text layout */

typedef struct {
    int *breaks;     /* indices in text where lines break */
    int break_count;
} TS_LineBreaks;

TS_LineBreaks ts_knuth_plass(const char *text, int line_width, int char_width) {
    TS_LineBreaks result = {NULL, 0};
    if (!text || line_width <= 0 || char_width <= 0) return result;

    int len = (int)strlen(text);
    int max_breaks = len / 2 + 10;
    result.breaks = (int*)malloc(sizeof(int) * (size_t)max_breaks);
    if (!result.breaks) return result;

    /* Find word boundaries */
    int *words = (int*)malloc(sizeof(int) * (size_t)(len+1));
    int *word_lens = (int*)malloc(sizeof(int) * (size_t)(len+1));
    int nw = 0;
    int i = 0;
    while (i < len) {
        while (i < len && text[i] == ' ') i++;
        if (i >= len) break;
        words[nw] = i;
        int wl = 0;
        while (i < len && text[i] != ' ') { i++; wl++; }
        word_lens[nw] = wl;
        nw++;
    }

    /* Dynamic programming: minimize badness */
    int chars_per_line = line_width / char_width;
    double *cost = (double*)malloc(sizeof(double) * (size_t)(nw + 1));
    int *from = (int*)malloc(sizeof(int) * (size_t)(nw + 1));
    for (int j = 0; j <= nw; j++) { cost[j] = 1e18; from[j] = -1; }
    cost[0] = 0;

    for (int j = 0; j < nw; j++) {
        int line_len = 0;
        for (int k = j; k < nw; k++) {
            line_len += word_lens[k] + (k > j ? 1 : 0); /* +1 for space */
            if (line_len > chars_per_line && k > j) break;
            double slack = chars_per_line - line_len;
            double badness = (k == nw - 1) ? 0 : slack * slack; /* last line = 0 badness */
            double c = cost[j] + badness;
            if (c < cost[k + 1]) { cost[k + 1] = c; from[k + 1] = j; }
        }
    }

    /* Trace back to get break points */
    int *stack = (int*)malloc(sizeof(int) * (size_t)(nw + 1));
    int sp = 0;
    int pos = nw;
    while (pos > 0) { stack[sp++] = pos; pos = from[pos]; }

    /* Convert word indices to character positions */
    for (int j = sp - 1; j >= 0; j--) {
        int wi = stack[j];
        if (wi < nw) result.breaks[result.break_count++] = words[wi];
    }

    free(words); free(word_lens); free(cost); free(from); free(stack);
    return result;
}

void ts_knuth_plass_free(TS_LineBreaks *lb) {
    if (lb) { free(lb->breaks); lb->breaks = NULL; lb->break_count = 0; }
}

/* ═══ ARC (Adaptive Replacement Cache) ═══════════════════════ */
/* Self-tuning cache that balances recency (LRU) and frequency (LFU) */

typedef struct _ARC_Entry {
    char key[128];
    int value;
    struct _ARC_Entry *prev, *next;
} ARC_Entry;

typedef struct {
    ARC_Entry *head, *tail;
    int count;
} ARC_List;

typedef struct {
    ARC_List t1, t2;   /* T1: recent, T2: frequent */
    ARC_List b1, b2;   /* B1: ghost recent, B2: ghost frequent */
    int capacity;
    int p;              /* target size for T1 (adaptive) */
} TS_ARC;

static void _arc_list_push(ARC_List *l, const char *key, int val) {
    ARC_Entry *e = (ARC_Entry*)calloc(1, sizeof(ARC_Entry));
    strncpy(e->key, key, 127); e->value = val;
    e->next = l->head; if (l->head) l->head->prev = e;
    l->head = e; if (!l->tail) l->tail = e;
    l->count++;
}

static int _arc_list_find(ARC_List *l, const char *key) {
    ARC_Entry *e = l->head;
    while (e) { if (strcmp(e->key, key) == 0) return 1; e = e->next; }
    return 0;
}

static int _arc_list_get(ARC_List *l, const char *key) {
    ARC_Entry *e = l->head;
    while (e) { if (strcmp(e->key, key) == 0) return e->value; e = e->next; }
    return -1;
}

static void _arc_list_remove(ARC_List *l, const char *key) {
    ARC_Entry *e = l->head;
    while (e) {
        if (strcmp(e->key, key) == 0) {
            if (e->prev) e->prev->next = e->next; else l->head = e->next;
            if (e->next) e->next->prev = e->prev; else l->tail = e->prev;
            free(e); l->count--; return;
        }
        e = e->next;
    }
}

static void _arc_list_evict_tail(ARC_List *l, char *evicted_key) {
    if (!l->tail) return;
    if (evicted_key) strncpy(evicted_key, l->tail->key, 127);
    ARC_Entry *t = l->tail;
    if (t->prev) t->prev->next = NULL; else l->head = NULL;
    l->tail = t->prev;
    free(t); l->count--;
}

TS_ARC *ts_arc_new(int capacity) {
    TS_ARC *a = (TS_ARC*)calloc(1, sizeof(TS_ARC));
    if (a) a->capacity = capacity > 4 ? capacity : 4;
    return a;
}

void ts_arc_free(TS_ARC *a) {
    if (!a) return;
    while (a->t1.head) _arc_list_evict_tail(&a->t1, NULL);
    while (a->t2.head) _arc_list_evict_tail(&a->t2, NULL);
    while (a->b1.head) _arc_list_evict_tail(&a->b1, NULL);
    while (a->b2.head) _arc_list_evict_tail(&a->b2, NULL);
    free(a);
}

int ts_arc_get(TS_ARC *a, const char *key) {
    if (!a || !key) return -1;
    /* Hit in T1 → move to T2 (promote to frequent) */
    if (_arc_list_find(&a->t1, key)) {
        int v = _arc_list_get(&a->t1, key);
        _arc_list_remove(&a->t1, key);
        _arc_list_push(&a->t2, key, v);
        return v;
    }
    /* Hit in T2 → move to front of T2 */
    if (_arc_list_find(&a->t2, key)) {
        int v = _arc_list_get(&a->t2, key);
        _arc_list_remove(&a->t2, key);
        _arc_list_push(&a->t2, key, v);
        return v;
    }
    return -1; /* miss */
}

void ts_arc_put(TS_ARC *a, const char *key, int val) {
    if (!a || !key) return;

    /* Already in cache? Update */
    if (_arc_list_find(&a->t1, key) || _arc_list_find(&a->t2, key)) {
        _arc_list_remove(&a->t1, key);
        _arc_list_remove(&a->t2, key);
        _arc_list_push(&a->t2, key, val);
        return;
    }

    /* In ghost B1? Adapt p upward (favor recency) */
    if (_arc_list_find(&a->b1, key)) {
        int d = a->b2.count > a->b1.count ? a->b2.count / a->b1.count : 1;
        a->p = a->p + d > a->capacity ? a->capacity : a->p + d;
        _arc_list_remove(&a->b1, key);
        /* Evict from T2 if needed */
        if (a->t1.count + a->t2.count >= a->capacity) {
            char ek[128]={0}; _arc_list_evict_tail(&a->t2, ek);
            if (ek[0]) _arc_list_push(&a->b2, ek, 0);
        }
        _arc_list_push(&a->t2, key, val);
        return;
    }

    /* In ghost B2? Adapt p downward (favor frequency) */
    if (_arc_list_find(&a->b2, key)) {
        int d = a->b1.count > a->b2.count ? a->b1.count / a->b2.count : 1;
        a->p = a->p - d < 0 ? 0 : a->p - d;
        _arc_list_remove(&a->b2, key);
        if (a->t1.count + a->t2.count >= a->capacity) {
            char ek[128]={0}; _arc_list_evict_tail(&a->t1, ek);
            if (ek[0]) _arc_list_push(&a->b1, ek, 0);
        }
        _arc_list_push(&a->t2, key, val);
        return;
    }

    /* New item → T1 */
    if (a->t1.count + a->t2.count >= a->capacity) {
        if (a->t1.count >= (a->p > 1 ? a->p : 1)) {
            char ek[128]={0}; _arc_list_evict_tail(&a->t1, ek);
            if (ek[0]) _arc_list_push(&a->b1, ek, 0);
        } else {
            char ek[128]={0}; _arc_list_evict_tail(&a->t2, ek);
            if (ek[0]) _arc_list_push(&a->b2, ek, 0);
        }
    }
    /* Trim ghost lists */
    while (a->b1.count > a->capacity) _arc_list_evict_tail(&a->b1, NULL);
    while (a->b2.count > a->capacity) _arc_list_evict_tail(&a->b2, NULL);

    _arc_list_push(&a->t1, key, val);
}

int ts_arc_size(TS_ARC *a) { return a ? a->t1.count + a->t2.count : 0; }


#endif
