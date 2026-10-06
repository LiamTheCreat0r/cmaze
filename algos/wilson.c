/*
 * wilson.c - Wilson's algorithm (loop-erased random walks).
 *
 * Unbiased sampling: start from one carved cell, then repeatedly walk
 * at random from an unvisited cell until the walk reaches the maze,
 * erasing any loop the walk forms along the way.  The wandering walk is
 * the whole point - it looks great animated.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int *walk;          /* cells of the current walk, in order       */
    int  wlen, wcap;
    int *pos;           /* rows*cols: index inside walk, or -1       */
    int  left;          /* cells still unvisited                     */
    int  walking;
} WiState;

static void walk_clear(WiState *s)
{
    int i;
    for (i = 0; i < s->wlen; i++)
        s->pos[s->walk[i]] = -1;
    s->wlen = 0;
}

static void walk_push(WiState *s, int idx)
{
    if (s->wlen == s->wcap) {
        int  ncap = s->wcap ? s->wcap * 2 : 64;
        int *ns   = realloc(s->walk, sizeof(int) * (size_t)ncap);
        if (!ns)
            return;
        s->walk = ns;
        s->wcap = ncap;
    }
    s->pos[idx] = s->wlen;
    s->walk[s->wlen++] = idx;
}

static int pick_unvisited(const Maze *m, Rng *rng)
{
    int total = m->rows * m->cols;
    int i, t;

    for (t = 0; t < 64; t++) {
        i = (int)rng_below(rng, (unsigned)total);
        if (!m->vis[i])
            return i;
    }
    for (i = 0; i < total; i++)
        if (!m->vis[i])
            return i;
    return -1;
}

static void *wi_create(Maze *m, Rng *rng, const Options *o)
{
    WiState *s = calloc(1, sizeof *s);
    int i, start;

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    s->pos   = malloc(sizeof(int) * (size_t)m->rows * m->cols);
    s->wcap  = 64;
    s->walk  = malloc(sizeof(int) * (size_t)s->wcap);
    if (!s->pos || !s->walk) {
        free(s->pos);
        free(s->walk);
        free(s);
        return NULL;
    }
    for (i = 0; i < m->rows * m->cols; i++)
        s->pos[i] = -1;

    maze_reset(m, 1);

    start = (int)rng_below(rng, (unsigned)m->rows * (unsigned)m->cols);
    maze_mark(m, start / m->cols, start % m->cols);
    m->head_r = start / m->cols;
    m->head_c = start % m->cols;
    s->left = m->rows * m->cols - 1;
    return s;
}

static int wi_step(Maze *m, void *vp)
{
    WiState *s = vp;
    int cur, d, nr, nc, n;

    if (s->left <= 0) {
        m->head_r = m->head_c = -1;
        return 1;
    }

    if (!s->walking) {
        int idx = pick_unvisited(m, s->rng);
        if (idx < 0) {
            s->left = 0;
            return 1;
        }
        walk_clear(s);
        walk_push(s, idx);
        s->walking = 1;
        m->head_r = idx / m->cols;
        m->head_c = idx % m->cols;
        return 0;
    }

    cur = s->walk[s->wlen - 1];
    d   = rng_below(s->rng, 4u);
    if (!maze_nb(m, cur / m->cols, cur % m->cols, d, &nr, &nc))
        return 0;                       /* bumped into the border */
    n = maze_idx(m, nr, nc);

    if (s->pos[n] >= 0) {
        /* The walk revisited itself: erase the loop it just made. */
        int from = s->pos[n] + 1;
        int i;
        for (i = from; i < s->wlen; i++)
            s->pos[s->walk[i]] = -1;
        s->wlen = from;
        m->head_r = nr;
        m->head_c = nc;
        return 0;
    }

    if (m->vis[n]) {
        /* Reached the maze: commit the walk. */
        int i;
        walk_push(s, n);
        for (i = 0; i + 1 < s->wlen; i++) {
            int a = s->walk[i], b = s->walk[i + 1];
            int ar = a / m->cols, ac = a % m->cols;
            int dir;
            if (b == a + 1)             dir = DIR_E;
            else if (b == a - 1)        dir = DIR_W;
            else if (b == a + m->cols)  dir = DIR_S;
            else                        dir = DIR_N;
            maze_carve(m, ar, ac, dir);
        }
        for (i = 0; i < s->wlen; i++) {
            int idx = s->walk[i];
            if (!m->vis[idx]) {
                m->vis[idx] = 1;
                m->stamp[idx] = (int)m->tick;
                s->left--;
            }
        }
        s->walking = 0;
        walk_clear(s);                  /* forget the indices we just used */
        m->head_r = nr;
        m->head_c = nc;
        return 0;
    }

    walk_push(s, n);
    m->head_r = nr;
    m->head_c = nc;
    return 0;
}

static void wi_destroy(void *vp)
{
    WiState *s = vp;
    if (!s)
        return;
    free(s->walk);
    free(s->pos);
    free(s);
}

const Algo algo_wilson = {
    "wilson",
    "Wilson's: unbiased loop-erased random walks",
    wi_create, wi_step, wi_destroy
};
