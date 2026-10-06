/*
 * aldous_broder.c - Aldous-Broder random walk.
 *
 * A single walker wanders the grid in uniformly random directions; every
 * unvisited cell it steps into gets carved open.  Unbiased but slow - the
 * walk can take a long time to find the last few cells, which is exactly
 * what makes it hypnotic to watch.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int  r, c;      /* walker position                                   */
    int  left;      /* cells still to be visited                         */
} AbState;

static void *ab_create(Maze *m, Rng *rng, const Options *o)
{
    AbState *s = calloc(1, sizeof *s);

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    maze_reset(m, 1);

    s->r = (int)rng_below(rng, (unsigned)m->rows);
    s->c = (int)rng_below(rng, (unsigned)m->cols);
    maze_mark(m, s->r, s->c);
    m->head_r = s->r;
    m->head_c = s->c;
    s->left = m->rows * m->cols - 1;
    return s;
}

static int ab_step(Maze *m, void *vp)
{
    AbState *s = vp;
    int or, oc, d, nr, nc;

    if (s->left <= 0) {
        m->head_r = m->head_c = -1;
        return 1;
    }

    or = s->r;
    oc = s->c;
    d  = (int)rng_below(s->rng, 4u);
    if (!maze_nb(m, s->r, s->c, d, &nr, &nc)) {
        m->head_r = s->r;               /* bumped into the border */
        m->head_c = s->c;
        return 0;
    }

    s->r = nr;                          /* follow the walker */
    s->c = nc;
    m->head_r = nr;
    m->head_c = nc;

    if (!m->vis[maze_idx(m, nr, nc)]) {
        maze_carve(m, or, oc, d);
        maze_mark(m, nr, nc);
        s->left--;
    }
    return 0;
}

static void ab_destroy(void *vp)
{
    free(vp);
}

const Algo algo_aldous_broder = {
    "aldous-broder",
    "Aldous-Broder: unbiased random walk, hypnotic and slow",
    ab_create, ab_step, ab_destroy
};
