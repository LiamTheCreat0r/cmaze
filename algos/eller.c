/*
 * eller.c - Eller's algorithm.
 *
 * Builds the maze one row at a time: cells in a row are merged into
 * sets, every set is guaranteed a connection downwards, and the next
 * row inherits the ids it is attached to.  The animation is a scanline
 * sweeping down the grid.
 */

#include "algos/algos.h"

enum { PH_MERGE = 0, PH_DOWN, PH_ADVANCE, PH_DONE };

typedef struct {
    Rng *rng;
    int  row;
    int  phase;
    int  col;           /* cursor inside PH_MERGE                    */
    int  seti;          /* cursor inside PH_DOWN                     */
    int  next_id;       /* set id allocator                          */
    int *sid;           /* [cols] set id of each cell in the current row */
    int *next_sid;      /* [cols] id inherited by the next row       */
    int *sets;          /* [cols] distinct ids present in this row   */
    int  nsets;
} ElState;

static void ensure_id(ElState *s, int c)
{
    if (s->sid[c] == 0)
        s->sid[c] = s->next_id++;
}

static void relabel(ElState *s, int cols, int from, int to)
{
    int i;

    if (from == to)
        return;
    for (i = 0; i < cols; i++)
        if (s->sid[i] == from)
            s->sid[i] = to;
}

static void build_sets(ElState *s, int cols)
{
    int i, j;

    s->nsets = 0;
    for (i = 0; i < cols; i++) {
        for (j = 0; j < s->nsets; j++)
            if (s->sets[j] == s->sid[i])
                break;
        if (j == s->nsets)
            s->sets[s->nsets++] = s->sid[i];
    }
}

static void mark_row(Maze *m, int r)
{
    int c;
    for (c = 0; c < m->cols; c++)
        maze_mark(m, r, c);
    m->head_r = r;
    m->head_c = 0;
}

static void *el_create(Maze *m, Rng *rng, const Options *o)
{
    ElState *s = calloc(1, sizeof *s);

    (void)o;
    if (!s)
        return NULL;

    s->rng = rng;
    s->next_id  = 1;
    s->sid      = calloc((size_t)m->cols, sizeof(int));
    s->next_sid = calloc((size_t)m->cols, sizeof(int));
    s->sets     = calloc((size_t)m->cols, sizeof(int));
    if (!s->sid || !s->next_sid || !s->sets) {
        free(s->sid);
        free(s->next_sid);
        free(s->sets);
        free(s);
        return NULL;
    }

    maze_reset(m, 1);                   /* Eller's carves like the rest */

    s->row   = 0;
    s->phase = PH_MERGE;
    s->col   = 0;
    ensure_id(s, 0);
    mark_row(m, 0);
    return s;
}

static int el_step(Maze *m, void *vp)
{
    ElState *s = vp;
    int cols = m->cols;

    switch (s->phase) {

    case PH_MERGE: {
        int last_row = (s->row == m->rows - 1);

        if (s->col >= cols - 1) {
            if (last_row) {
                s->phase = PH_DONE;
            } else {
                build_sets(s, cols);
                s->seti = 0;
                s->phase = PH_DOWN;
            }
            return 0;
        }

        ensure_id(s, s->col);
        ensure_id(s, s->col + 1);

        if (s->sid[s->col] != s->sid[s->col + 1]) {
            int join = last_row || rng_double(s->rng) < 0.5;
            if (join) {
                maze_carve(m, s->row, s->col, DIR_E);
                relabel(s, cols, s->sid[s->col + 1], s->sid[s->col]);
            }
        }

        s->col++;
        m->head_r = s->row;
        m->head_c = s->col;
        return 0;
    }

    case PH_DOWN: {
        int set, i, chosen = -1, members = 0, dropped = 0;

        if (s->seti >= s->nsets) {
            s->phase = PH_ADVANCE;
            return 0;
        }

        set = s->sets[s->seti];
        for (i = 0; i < cols; i++) {
            if (s->sid[i] != set)
                continue;
            members++;
            if (rng_double(s->rng) < 0.5) {
                maze_carve(m, s->row, i, DIR_S);
                s->next_sid[i] = set;
                dropped++;
                chosen = i;
            }
        }

        /* Every set must reach the row below, or the maze falls apart. */
        if (dropped == 0 && members > 0) {
            int pick = (int)rng_below(s->rng, (unsigned)members);
            for (i = 0; i < cols; i++) {
                if (s->sid[i] != set)
                    continue;
                if (pick-- == 0) {
                    maze_carve(m, s->row, i, DIR_S);
                    s->next_sid[i] = set;
                    chosen = i;
                    break;
                }
            }
        }

        if (chosen >= 0) {
            m->head_r = s->row;
            m->head_c = chosen;
        }

        s->seti++;
        return 0;
    }

    case PH_ADVANCE: {
        int i;

        s->row++;
        if (s->row >= m->rows) {
            s->phase = PH_DONE;
            return 0;
        }
        for (i = 0; i < cols; i++) {
            s->sid[i] = s->next_sid[i];
            s->next_sid[i] = 0;
        }
        s->phase = PH_MERGE;
        s->col = 0;
        ensure_id(s, 0);
        mark_row(m, s->row);
        return 0;
    }

    default:
        m->head_r = m->head_c = -1;
        return 1;
    }
}

static void el_destroy(void *vp)
{
    ElState *s = vp;
    if (!s)
        return;
    free(s->sid);
    free(s->next_sid);
    free(s->sets);
    free(s);
}

const Algo algo_eller = {
    "eller",
    "Eller's: builds row by row, a scanline effect",
    el_create, el_step, el_destroy
};
