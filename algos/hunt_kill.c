/*
 * hunt_kill.c - hunt-and-kill maze generation.
 *
 * Like the backtracker the walker carves at random until boxed in, but
 * instead of backtracking it then scans the grid for any unvisited cell
 * touching the maze and resumes carving from there.  The scan is animated
 * one cell per step, which is the whole visual point of this algorithm.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int  r, c;       /* current cell (kill phase)                         */
    int  hunting;    /* 0 = carving, 1 = scanning for a new start        */
    int  hr, hc;     /* persistent scan cursor, wraps row-major          */
    int  examined;   /* cells scanned in the current hunt                */
} HkState;

static void *hk_create(Maze *m, Rng *rng, const Options *o)
{
    HkState *s = calloc(1, sizeof *s);

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

    s->hunting  = 0;
    s->hr       = 0;
    s->hc       = 0;
    s->examined = 0;
    return s;
}

static int hk_step(Maze *m, void *vp)
{
    HkState *s = vp;
    int d, dirs[4], n = 0;

    if (!s->hunting) {                  /* kill phase: carve at random */
        for (d = 0; d < 4; d++) {
            int nr, nc;
            if (!maze_nb(m, s->r, s->c, d, &nr, &nc))
                continue;
            if (!m->vis[maze_idx(m, nr, nc)])
                dirs[n++] = d;
        }
        if (n > 0) {
            int nr, nc;
            d = dirs[rng_below(s->rng, (unsigned)n)];
            maze_nb(m, s->r, s->c, d, &nr, &nc);
            maze_carve(m, s->r, s->c, d);
            maze_mark(m, nr, nc);
            s->r = nr;
            s->c = nc;
            m->head_r = nr;
            m->head_c = nc;
            return 0;
        }
        s->hunting = 1;                 /* boxed in: fall through to hunt */
    }

    /* Hunt phase: examine exactly one cell this step. */
    {
        int r = s->hr, c = s->hc;
        int idx = maze_idx(m, r, c);

        m->head_r = r;
        m->head_c = c;
        s->examined++;

        /* advance the cursor row-major, wrapping around the grid */
        if (++s->hc >= m->cols) {
            s->hc = 0;
            if (++s->hr >= m->rows)
                s->hr = 0;
        }

        if (!m->vis[idx]) {
            int vn[4], nv = 0;
            for (d = 0; d < 4; d++) {
                int nr, nc;
                if (!maze_nb(m, r, c, d, &nr, &nc))
                    continue;
                if (m->vis[maze_idx(m, nr, nc)])
                    vn[nv++] = d;
            }
            if (nv > 0) {
                int nr, nc;
                d = vn[rng_below(s->rng, (unsigned)nv)];
                maze_nb(m, r, c, d, &nr, &nc);
                maze_carve(m, r, c, d);
                maze_mark(m, r, c);
                s->r = r;
                s->c = c;
                m->head_r = r;
                m->head_c = c;
                s->hunting  = 0;
                s->examined = 0;
                return 0;
            }
        }

        /* A full pass without a candidate: every cell is accounted for. */
        if (s->examined >= m->rows * m->cols) {
            m->head_r = m->head_c = -1;
            return 1;
        }
        return 0;
    }
}

static void hk_destroy(void *vp)
{
    free(vp);
}

const Algo algo_hunt_kill = {
    "hunt-and-kill",
    "Hunt-and-Kill: like the backtracker with a visible scanning phase",
    hk_create, hk_step, hk_destroy
};
