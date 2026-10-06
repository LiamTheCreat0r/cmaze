/*
 * growing_tree.c - growing tree algorithm, strategy selectable.
 *
 * Maintain a list of "active" cells; each step pick one, carve into a
 * random unvisited neighbour and add that neighbour to the list.  How the
 * active cell is chosen (newest / oldest / random / mix) gives very
 * different mazes from the same machinery.
 */

#include "algos/algos.h"

enum { GT_NEWEST, GT_OLDEST, GT_RANDOM, GT_MIX };

typedef struct {
    Rng *rng;
    int *list;
    int  len, cap;
    int  toggle;     /* flips each step under the "mix" strategy         */
    int  strategy;   /* one of GT_*                                     */
} GtState;

static void *gt_create(Maze *m, Rng *rng, const Options *o)
{
    GtState *s = calloc(1, sizeof *s);
    int r, c;

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    s->strategy = GT_MIX;
    if (o && o->strategy) {
        if (!strcmp(o->strategy, "newest"))       s->strategy = GT_NEWEST;
        else if (!strcmp(o->strategy, "oldest"))  s->strategy = GT_OLDEST;
        else if (!strcmp(o->strategy, "random"))  s->strategy = GT_RANDOM;
    }

    s->cap = 64;
    s->list = malloc(sizeof(int) * (size_t)s->cap);
    if (!s->list) {
        free(s);
        return NULL;
    }

    maze_reset(m, 1);

    r = (int)rng_below(rng, (unsigned)m->rows);
    c = (int)rng_below(rng, (unsigned)m->cols);
    maze_mark(m, r, c);
    m->head_r = r;
    m->head_c = c;
    s->list[s->len++] = maze_idx(m, r, c);
    return s;
}

static int gt_step(Maze *m, void *vp)
{
    GtState *s = vp;
    int pick, r, c, d, dirs[4], n = 0;

    if (s->len == 0) {
        m->head_r = m->head_c = -1;
        return 1;
    }

    switch (s->strategy) {
    case GT_OLDEST: pick = 0; break;
    case GT_RANDOM: pick = (int)rng_below(s->rng, (unsigned)s->len); break;
    case GT_MIX:
        if (s->toggle & 1)
            pick = (int)rng_below(s->rng, (unsigned)s->len);
        else
            pick = s->len - 1;
        s->toggle++;
        break;
    default:        pick = s->len - 1; break;   /* GT_NEWEST */
    }

    r = s->list[pick] / m->cols;
    c = s->list[pick] % m->cols;

    for (d = 0; d < 4; d++) {
        int nr, nc;
        if (!maze_nb(m, r, c, d, &nr, &nc))
            continue;
        if (!m->vis[maze_idx(m, nr, nc)])
            dirs[n++] = d;
    }

    if (n == 0) {                       /* dead end: drop from the list */
        if (pick == s->len - 1)
            s->len--;
        else if (pick == 0) {
            memmove(s->list, s->list + 1, sizeof(int) * (size_t)(s->len - 1));
            s->len--;
        } else {
            s->list[pick] = s->list[s->len - 1];
            s->len--;
        }
        if (s->len == 0) {
            m->head_r = m->head_c = -1;
            return 1;
        }
        m->head_r = s->list[s->len - 1] / m->cols;
        m->head_c = s->list[s->len - 1] % m->cols;
        return 0;
    }

    d = dirs[rng_below(s->rng, (unsigned)n)];
    {
        int nr, nc;
        maze_nb(m, r, c, d, &nr, &nc);
        maze_carve(m, r, c, d);
        maze_mark(m, nr, nc);
        m->head_r = nr;
        m->head_c = nc;

        if (s->len == s->cap) {
            int ncap = s->cap * 2;
            int *ns = realloc(s->list, sizeof(int) * (size_t)ncap);
            if (ns) {
                s->list = ns;
                s->cap = ncap;
            }
        }
        if (s->len < s->cap)
            s->list[s->len++] = maze_idx(m, nr, nc);
        else
            s->len = 0;                 /* give up rather than overrun */
    }
    return 0;
}

static void gt_destroy(void *vp)
{
    GtState *s = vp;
    if (!s)
        return;
    free(s->list);
    free(s);
}

const Algo algo_growing_tree = {
    "growing-tree",
    "Growing Tree: selectable strategy mixes depth and breadth",
    gt_create, gt_step, gt_destroy
};
