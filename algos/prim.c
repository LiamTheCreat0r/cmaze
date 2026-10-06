/*
 * prim.c - randomized Prim's algorithm on cells.
 *
 * A frontier of cells touching the already-carved region is kept; each
 * step one frontier cell is joined up.  The result is bushy, full of
 * short dead ends, and grows outward like a stain.
 */

#include "algos/algos.h"

typedef struct {
    Rng           *rng;
    int           *frontier;
    int            len, cap;
    unsigned char *in;      /* rows*cols: 1 while queued */
} PrState;

static void frontier_add(PrState *s, int idx)
{
    if (s->in[idx])
        return;
    s->in[idx] = 1;
    if (s->len == s->cap) {
        int  ncap = s->cap ? s->cap * 2 : 64;
        int *ns   = realloc(s->frontier, sizeof(int) * (size_t)ncap);
        if (!ns)
            return;
        s->frontier = ns;
        s->cap = ncap;
    }
    s->frontier[s->len++] = idx;
}

static void *pr_create(Maze *m, Rng *rng, const Options *o)
{
    PrState *s = calloc(1, sizeof *s);
    int r, c, d;

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    s->in = calloc((size_t)m->rows * m->cols, 1);
    if (!s->in) {
        free(s);
        return NULL;
    }

    maze_reset(m, 1);

    r = (int)rng_below(rng, (unsigned)m->rows);
    c = (int)rng_below(rng, (unsigned)m->cols);
    maze_mark(m, r, c);
    m->head_r = r;
    m->head_c = c;

    for (d = 0; d < 4; d++) {
        int nr, nc;
        if (maze_nb(m, r, c, d, &nr, &nc))
            frontier_add(s, maze_idx(m, nr, nc));
    }
    return s;
}

static int pr_step(Maze *m, void *vp)
{
    PrState *s = vp;
    int pick, idx, r, c, d, dirs[4], n = 0;

    if (s->len == 0)
        return 1;

    pick = (int)rng_below(s->rng, (unsigned)s->len);
    idx  = s->frontier[pick];
    r    = idx / m->cols;
    c    = idx % m->cols;

    /* At least one neighbour is carved already by construction. */
    for (d = 0; d < 4; d++) {
        int nr, nc;
        if (!maze_nb(m, r, c, d, &nr, &nc))
            continue;
        if (m->vis[maze_idx(m, nr, nc)])
            dirs[n++] = d;
    }

    if (n > 0) {
        int sd = dirs[rng_below(s->rng, (unsigned)n)];
        maze_carve(m, r, c, sd);
        maze_mark(m, r, c);
        m->head_r = r;
        m->head_c = c;

        /* Everything around the newly carved cell is now frontier. */
        for (d = 0; d < 4; d++) {
            int nr, nc;
            if (!maze_nb(m, r, c, d, &nr, &nc))
                continue;
            if (!m->vis[maze_idx(m, nr, nc)])
                frontier_add(s, maze_idx(m, nr, nc));
        }
    }

    /* Consumed: swap-remove. */
    s->frontier[pick] = s->frontier[s->len - 1];
    s->len--;
    s->in[idx] = 0;
    return 0;
}

static void pr_destroy(void *vp)
{
    PrState *s = vp;
    if (!s)
        return;
    free(s->frontier);
    free(s->in);
    free(s);
}

const Algo algo_prim = {
    "prim",
    "randomized Prim's: bushy, many short dead ends",
    pr_create, pr_step, pr_destroy
};
