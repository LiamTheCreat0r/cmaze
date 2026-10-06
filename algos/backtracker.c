/*
 * backtracker.c - recursive backtracker (depth first search).
 *
 * Long, winding corridors with few branches: walk as far as you can,
 * then backtrack to the most recent cell that still has an exit.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int *stack;
    int  len, cap;
} BtState;

static void *bt_create(Maze *m, Rng *rng, const Options *o)
{
    BtState *s = calloc(1, sizeof *s);

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    s->cap = 64;
    s->stack = malloc(sizeof(int) * (size_t)s->cap);
    if (!s->stack) {
        free(s);
        return NULL;
    }

    maze_reset(m, 1);

    {
        int r = (int)rng_below(rng, (unsigned)m->rows);
        int c = (int)rng_below(rng, (unsigned)m->cols);
        maze_mark(m, r, c);
        s->stack[s->len++] = maze_idx(m, r, c);
        m->head_r = r;
        m->head_c = c;
    }
    return s;
}

static int bt_step(Maze *m, void *vp)
{
    BtState *s = vp;
    int cur, r, c, d, dirs[4], n = 0;

    if (s->len == 0)
        return 1;

    cur = s->stack[s->len - 1];
    r = cur / m->cols;
    c = cur % m->cols;

    for (d = 0; d < 4; d++) {
        int nr, nc;
        if (!maze_nb(m, r, c, d, &nr, &nc))
            continue;
        if (!m->vis[maze_idx(m, nr, nc)])
            dirs[n++] = d;
    }

    if (n == 0) {                       /* dead end: backtrack */
        s->len--;
        if (s->len > 0) {
            int p = s->stack[s->len - 1];
            m->head_r = p / m->cols;
            m->head_c = p % m->cols;
        } else {
            m->head_r = m->head_c = -1;
        }
        return s->len == 0;
    }

    d = dirs[rng_below(s->rng, (unsigned)n)];
    maze_carve(m, r, c, d);

    {
        int nr, nc;
        maze_nb(m, r, c, d, &nr, &nc);
        maze_mark(m, nr, nc);
        m->head_r = nr;
        m->head_c = nc;

        if (s->len == s->cap) {
            int ncap = s->cap * 2;
            int *ns = realloc(s->stack, sizeof(int) * (size_t)ncap);
            if (ns) {
                s->stack = ns;
                s->cap = ncap;
            }
        }
        if (s->len < s->cap)
            s->stack[s->len++] = maze_idx(m, nr, nc);
        else
            s->len = 0;                 /* give up rather than overrun */
    }
    return 0;
}

static void bt_destroy(void *vp)
{
    BtState *s = vp;
    if (!s)
        return;
    free(s->stack);
    free(s);
}

const Algo algo_backtracker = {
    "backtracker",
    "recursive backtracker (DFS): long winding corridors",
    bt_create, bt_step, bt_destroy
};
