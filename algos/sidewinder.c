/*
 * sidewinder.c - classic Sidewinder.
 *
 * Sweep the grid like a scanline, row 0 first, left to right, keeping a
 * run of column indices from the current row.  At each cell the run is
 * either closed - carving north out of a uniformly random member of the
 * run - or extended one cell east.  The east border always closes the
 * run, so the top row ends up fully open east-west.
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int *run;        /* column indices of the current run */
    int  runlen;
    int  row, col;   /* cursor: the cell this step processes */
} SwState;

static void *sw_create(Maze *m, Rng *rng, const Options *o)
{
    SwState *s = calloc(1, sizeof *s);

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;
    s->run = malloc(sizeof(int) * (size_t)m->cols);
    if (!s->run) {
        free(s);
        return NULL;
    }

    maze_reset(m, 1);

    s->row = 0;
    s->col = 0;
    s->runlen = 0;
    return s;
}

static int sw_step(Maze *m, void *vp)
{
    SwState *s = vp;
    int at_east, at_north, close;

    maze_mark(m, s->row, s->col);
    m->head_r = s->row;
    m->head_c = s->col;

    if (s->runlen < m->cols)                 /* never overrun one row */
        s->run[s->runlen++] = s->col;

    at_east  = (s->col == m->cols - 1);
    at_north = (s->row == 0);
    close    = at_east || (!at_north && rng_double(s->rng) < 0.5);

    if (close) {
        if (!at_north) {
            int pick     = (int)rng_below(s->rng, (unsigned)s->runlen);
            int thatcol  = s->run[pick];
            maze_carve(m, s->row, thatcol, DIR_N);
            m->head_r = s->row;
            m->head_c = thatcol;
        }
        s->runlen = 0;
    } else {
        maze_carve(m, s->row, s->col, DIR_E);
    }

    s->col++;
    if (s->col >= m->cols) {
        s->col = 0;
        s->row++;
    }

    if (s->row >= m->rows) {
        m->head_r = m->head_c = -1;
        return 1;
    }
    return 0;
}

static void sw_destroy(void *vp)
{
    SwState *s = vp;
    if (!s)
        return;
    free(s->run);
    free(s);
}

const Algo algo_sidewinder = {
    "sidewinder",
    "Sidewinder: horizontal runs with a strong eastward bias",
    sw_create, sw_step, sw_destroy
};
