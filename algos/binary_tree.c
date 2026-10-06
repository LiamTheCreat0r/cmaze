/*
 * binary_tree.c - binary tree maze.
 *
 * Sweep every cell in order (row 0 first, left to right); at each cell
 * carve either north or east, chosen uniformly from whichever of the two
 * is still inside the grid.  The result has a strong diagonal bias with
 * two clear open edges (north and east).
 */

#include "algos/algos.h"

typedef struct {
    Rng *rng;
    int  row, col;   /* cursor: the cell this step processes */
} BintState;

static void *bint_create(Maze *m, Rng *rng, const Options *o)
{
    BintState *s = calloc(1, sizeof *s);

    (void)o;

    if (!s)
        return NULL;

    s->rng = rng;

    maze_reset(m, 1);

    s->row = 0;
    s->col = 0;
    return s;
}

static int bint_step(Maze *m, void *vp)
{
    BintState *s = vp;
    int dirs[2], n = 0;

    maze_mark(m, s->row, s->col);
    m->head_r = s->row;
    m->head_c = s->col;

    if (s->row > 0)
        dirs[n++] = DIR_N;
    if (s->col < m->cols - 1)
        dirs[n++] = DIR_E;

    if (n > 0) {                       /* only the top-right cell has none */
        int d = algo_pick_dir(s->rng, dirs, n);
        maze_carve(m, s->row, s->col, d);
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

static void bint_destroy(void *vp)
{
    free(vp);
}

const Algo algo_binary_tree = {
    "binary-tree",
    "Binary Tree: diagonal bias with two clear open edges",
    bint_create, bint_step, bint_destroy
};
