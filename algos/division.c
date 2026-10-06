/*
 * division.c - recursive division.
 *
 * The only algorithm that starts with an open field and *adds* walls.
 * Each step drops one straight wall with a gap in it, so the animation
 * shows long straight walls appearing and the space getting chopped
 * into rooms.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int *stk;          /* r0, r1, c0, c1 per queued rectangle */
    int  len, cap;
} DvState;

static void push_rect(DvState *s, int r0, int r1, int c0, int c1)
{
    if (s->len + 4 > s->cap) {
        int  ncap = s->cap ? s->cap * 2 : 64;
        int *ns   = realloc(s->stk, sizeof(int) * (size_t)ncap);
        if (!ns)
            return;
        s->stk = ns;
        s->cap = ncap;
    }
    s->stk[s->len++] = r0;
    s->stk[s->len++] = r1;
    s->stk[s->len++] = c0;
    s->stk[s->len++] = c1;
}

static void *dv_create(Maze *m, Rng *rng, const Options *o)
{
    DvState *s = calloc(1, sizeof *s);
    int i, n;

    (void)o;
    if (!s)
        return NULL;

    s->rng = rng;
    maze_reset(m, 0);                   /* open field, border walls only */

    /* Everything is already "carved"; light the whole grid up at once and
     * park the stamps far in the past so no colour trail is painted.     */
    n = m->rows * m->cols;
    for (i = 0; i < n; i++) {
        m->vis[i] = 1;
        m->stamp[i] = -CMAZE_TRAIL_2 - 1;
    }

    s->cap = 64;
    s->stk = malloc(sizeof(int) * (size_t)s->cap);
    if (!s->stk) {
        free(s);
        return NULL;
    }
    push_rect(s, 0, m->rows - 1, 0, m->cols - 1);
    return s;
}

static int dv_step(Maze *m, void *vp)
{
    DvState *s = vp;

    while (s->len >= 4) {
        int c1 = s->stk[--s->len];
        int c0 = s->stk[--s->len];
        int r1 = s->stk[--s->len];
        int r0 = s->stk[--s->len];
        int h  = r1 - r0 + 1;
        int w  = c1 - c0 + 1;
        int horiz, i;

        if (h < 1 || w < 1)
            continue;
        if (h < 2 && w < 2)
            continue;                   /* single cell: nothing to divide */

        /* Cut along the longer axis; coin-flip when square. */
        if (h > w)
            horiz = 1;
        else if (w > h)
            horiz = 0;
        else
            horiz = (int)rng_below(s->rng, 2u);

        if (horiz) {
            int k    = r0 + (int)rng_below(s->rng, (unsigned)(h - 1));
            int gap  = c0 + (int)rng_below(s->rng, (unsigned)w);

            for (i = c0; i <= c1; i++)
                if (i != gap)
                    maze_wall(m, k, i, DIR_S);

            push_rect(s, k + 1, r1, c0, c1);
            push_rect(s, r0, k, c0, c1);

            m->head_r = k;
            m->head_c = gap;
            m->stamp[maze_idx(m, k, gap)] = (int)m->tick;
            return 0;
        } else {
            int k    = c0 + (int)rng_below(s->rng, (unsigned)(w - 1));
            int gap  = r0 + (int)rng_below(s->rng, (unsigned)h);

            for (i = r0; i <= r1; i++)
                if (i != gap)
                    maze_wall(m, i, k, DIR_E);

            push_rect(s, r0, r1, k + 1, c1);
            push_rect(s, r0, r1, c0, k);

            m->head_r = gap;
            m->head_c = k;
            m->stamp[maze_idx(m, gap, k)] = (int)m->tick;
            return 0;
        }
    }

    m->head_r = m->head_c = -1;
    return 1;
}

static void dv_destroy(void *vp)
{
    DvState *s = vp;
    if (!s)
        return;
    free(s->stk);
    free(s);
}

const Algo algo_division = {
    "division",
    "recursive division: walls appear, long straight lines",
    dv_create, dv_step, dv_destroy
};
