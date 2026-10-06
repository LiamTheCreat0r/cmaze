/*
 * kruskal.c - randomized Kruskal's algorithm.
 *
 * Every wall between two cells is shuffled; edges that would close a
 * cycle are dropped.  The animation has a uniform texture of many small
 * regions merging into each other.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int *edges;         /* pairs of cell indices, two entries per edge */
    int  n, pos;        /* n = number of edges, pos = next to consider */
    int *parent, *rank;
} KrState;

static int uf_find(KrState *s, int x)
{
    while (s->parent[x] != x) {
        s->parent[x] = s->parent[s->parent[x]];
        x = s->parent[x];
    }
    return x;
}

static void uf_union(KrState *s, int a, int b)
{
    int ra = uf_find(s, a), rb = uf_find(s, b);

    if (ra == rb)
        return;
    if (s->rank[ra] < s->rank[rb]) {
        s->parent[ra] = rb;
    } else if (s->rank[ra] > s->rank[rb]) {
        s->parent[rb] = ra;
    } else {
        s->parent[rb] = ra;
        s->rank[ra]++;
    }
}

static void *kr_create(Maze *m, Rng *rng, const Options *o)
{
    KrState *s = calloc(1, sizeof *s);
    int cells = m->rows * m->cols;
    int maxe  = m->rows * (m->cols - 1) + (m->rows - 1) * m->cols;
    int i, r, c;

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    if (maxe < 1)
        maxe = 1;
    s->edges  = malloc(sizeof(int) * (size_t)maxe * 2);
    s->parent = malloc(sizeof(int) * (size_t)cells);
    s->rank   = calloc((size_t)cells, sizeof(int));
    if (!s->edges || !s->parent || !s->rank) {
        free(s->edges);
        free(s->parent);
        free(s->rank);
        free(s);
        return NULL;
    }

    maze_reset(m, 1);

    s->n = 0;
    for (r = 0; r < m->rows; r++) {
        for (c = 0; c < m->cols; c++) {
            int a = maze_idx(m, r, c);
            if (c + 1 < m->cols) {
                s->edges[2 * s->n]     = a;
                s->edges[2 * s->n + 1] = a + 1;
                s->n++;
            }
            if (r + 1 < m->rows) {
                s->edges[2 * s->n]     = a;
                s->edges[2 * s->n + 1] = a + m->cols;
                s->n++;
            }
        }
    }

    /* Fisher-Yates over the edge pairs. */
    for (i = s->n - 1; i > 0; i--) {
        int j = (int)rng_below(rng, (unsigned)(i + 1));
        int t0 = s->edges[2 * i];
        int t1 = s->edges[2 * i + 1];
        s->edges[2 * i]     = s->edges[2 * j];
        s->edges[2 * i + 1] = s->edges[2 * j + 1];
        s->edges[2 * j]     = t0;
        s->edges[2 * j + 1] = t1;
    }

    for (i = 0; i < cells; i++) {
        s->parent[i] = i;
        s->rank[i] = 0;
    }
    s->pos = 0;
    return s;
}

static int kr_step(Maze *m, void *vp)
{
    KrState *s = vp;

    /* Scan until an edge is actually taken, so a step always shows a
     * region growing and the animation length matches the other types. */
    while (s->pos < s->n) {
        int a = s->edges[2 * s->pos];
        int b = s->edges[2 * s->pos + 1];
        s->pos++;

        if (uf_find(s, a) != uf_find(s, b)) {
            int ar = a / m->cols, ac = a % m->cols;
            int br = b / m->cols, bc = b % m->cols;
            int dir = (br == ar) ? DIR_E : DIR_S;

            uf_union(s, a, b);
            maze_carve(m, ar, ac, dir);
            maze_mark(m, ar, ac);
            maze_mark(m, br, bc);
            m->head_r = br;
            m->head_c = bc;
            return 0;
        }
    }

    m->head_r = m->head_c = -1;
    return 1;
}

static void kr_destroy(void *vp)
{
    KrState *s = vp;
    if (!s)
        return;
    free(s->edges);
    free(s->parent);
    free(s->rank);
    free(s);
}

const Algo algo_kruskal = {
    "kruskal",
    "randomized Kruskal's: uniform texture, regions merging",
    kr_create, kr_step, kr_destroy
};
