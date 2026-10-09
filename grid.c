/*
 * grid.c - the cell grid, wall bookkeeping and the post-generation
 * decoration passes (braid, endpoints).
 */

#include "cmaze.h"

Maze *maze_new(int rows, int cols)
{
    Maze *m;

    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;

    m = calloc(1, sizeof *m);
    if (!m)
        return NULL;

    m->rows = rows;
    m->cols = cols;
    m->walls = calloc((size_t)rows * cols, 1);
    m->vis   = calloc((size_t)rows * cols, 1);
    m->stamp = calloc((size_t)rows * cols, sizeof(int));
    if (!m->walls || !m->vis || !m->stamp) {
        maze_free(m);
        return NULL;
    }

    m->head_r = m->head_c = -1;
    m->ep1_r = m->ep1_c = m->ep2_r = m->ep2_c = -1;
    maze_reset(m, 1);
    return m;
}

void maze_free(Maze *m)
{
    if (!m)
        return;
    free(m->walls);
    free(m->vis);
    free(m->stamp);
    free(m);
}

void maze_reset(Maze *m, int closed)
{
    size_t n = (size_t)m->rows * m->cols;
    size_t i;
    int r, c;

    if (closed) {
        memset(m->walls, 0x0F, n);          /* N|E|S|W for every cell */
    } else {
        for (r = 0; r < m->rows; r++) {
            for (c = 0; c < m->cols; c++) {
                unsigned char w = 0;
                if (r == 0)             w |= WALL_BIT(DIR_N);
                if (r == m->rows - 1)   w |= WALL_BIT(DIR_S);
                if (c == 0)             w |= WALL_BIT(DIR_W);
                if (c == m->cols - 1)   w |= WALL_BIT(DIR_E);
                m->walls[maze_idx(m, r, c)] = w;
            }
        }
    }

    memset(m->vis, 0, n);
    for (i = 0; i < n; i++)
        m->stamp[i] = 0;

    m->head_r = m->head_c = -1;
    m->ep1_r = m->ep1_c = m->ep2_r = m->ep2_c = -1;
    m->tick = 0;
}

/* Open the passage between (r,c) and its neighbour in `dir`.  Both sides
 * of the wall are always updated so the grid never gets out of sync.   */
void maze_carve(Maze *m, int r, int c, int dir)
{
    int nr, nc;

    if (!maze_nb(m, r, c, dir, &nr, &nc))
        return;
    m->walls[maze_idx(m, r, c)]     &= (unsigned char)~WALL_BIT(dir);
    m->walls[maze_idx(m, nr, nc)]   &= (unsigned char)~WALL_BIT(DIR_OPPOSITE(dir));
}

/* Put a wall back (used by recursive division). */
void maze_wall(Maze *m, int r, int c, int dir)
{
    int nr, nc;

    if (!maze_nb(m, r, c, dir, &nr, &nc))
        return;
    m->walls[maze_idx(m, r, c)]   |= (unsigned char)WALL_BIT(dir);
    m->walls[maze_idx(m, nr, nc)] |= (unsigned char)WALL_BIT(DIR_OPPOSITE(dir));
}

void maze_mark(Maze *m, int r, int c)
{
    int i;

    if (!maze_in(m, r, c))
        return;
    i = maze_idx(m, r, c);
    if (!m->vis[i]) {
        m->vis[i] = 1;
        m->stamp[i] = (int)m->tick;
    }
}

int maze_cell_role(const Maze *m, int r, int c)
{
    int i = maze_idx(m, r, c);
    int role, age;

    if (!m->vis[i]) {
        role = CR_UNVIS;
    } else if (r == m->head_r && c == m->head_c) {
        role = CR_HEAD;
    } else {
        age = (int)m->tick - m->stamp[i];
        if (age >= 0 && age < CMAZE_TRAIL_1)
            role = CR_TRAIL1;
        else if (age >= 0 && age < CMAZE_TRAIL_2)
            role = CR_TRAIL2;
        else
            role = CR_PATH;
    }

    if ((r == m->ep1_r && c == m->ep1_c) || (r == m->ep2_r && c == m->ep2_c))
        role = CR_ENDPOINT;
    return role;
}

static int open_count(const Maze *m, int r, int c)
{
    int d, n = 0;
    unsigned char w = m->walls[maze_idx(m, r, c)];

    for (d = 0; d < 4; d++) {
        int nr, nc;
        if (w & WALL_BIT(d))
            continue;
        if (maze_nb(m, r, c, d, &nr, &nc))
            n++;
    }
    return n;
}

/*
 * Remove `pct` percent of the dead ends so the maze grows loops.
 * A dead end with no exits at all (an unreachable pocket) is stitched
 * into the maze instead of being skipped.
 */
void maze_braid(Maze *m, int pct, Rng *rng)
{
    int r, c, i, n, target, done = 0;
    int *cand;

    if (pct <= 0)
        return;
    if (pct > 100)
        pct = 100;

    cand = malloc(sizeof(int) * (size_t)m->rows * m->cols);
    if (!cand)
        return;

    n = 0;
    for (r = 0; r < m->rows; r++)
        for (c = 0; c < m->cols; c++)
            if (open_count(m, r, c) <= 1)
                cand[n++] = maze_idx(m, r, c);

    /* Partial Fisher-Yates so we only touch the prefix we need. */
    target = (int)(((long long)n * pct) / 100);
    for (i = 0; i < n && done < target; i++) {
        int j = i + (int)rng_below(rng, (unsigned)(n - i));
        int t = cand[i];
        int idx, d, order[4], k;

        cand[i] = cand[j];
        cand[j] = t;

        idx = cand[i];
        r = idx / m->cols;
        c = idx % m->cols;

        /* Pick a random closed wall that actually leads somewhere. */
        for (d = 0; d < 4; d++)
            order[d] = d;
        for (d = 3; d > 0; d--) {
            int k2 = (int)rng_below(rng, (unsigned)(d + 1));
            int t2 = order[d];
            order[d] = order[k2];
            order[k2] = t2;
        }

        for (k = 0; k < 4; k++) {
            int nr, nc;
            d = order[k];
            if (!(m->walls[idx] & WALL_BIT(d)))
                continue;
            if (!maze_nb(m, r, c, d, &nr, &nc))
                continue;
            maze_carve(m, r, c, d);
            done++;
            break;
        }
    }

    free(cand);
}

/* Open one gap in the top border and one in the bottom border. */
void maze_endpoints(Maze *m, Rng *rng)
{
    int last = m->rows - 1;
    int r, best;

    if (m->rows < 1 || m->cols < 1)
        return;

    /* Prefer a genuine dead end on the border. */
    best = -1;
    for (r = 0; r < m->cols; r++)
        if (open_count(m, 0, r) == 1) { best = r; break; }
    if (best < 0)
        best = (int)rng_below(rng, (unsigned)m->cols);
    /* Border gaps have no neighbour on the other side, so carve the bit
     * directly instead of going through maze_carve(). */
    m->walls[maze_idx(m, 0, best)] &= (unsigned char)~WALL_BIT(DIR_N);
    m->ep1_r = 0;
    m->ep1_c = best;

    best = -1;
    for (r = 0; r < m->cols; r++)
        if (open_count(m, last, r) == 1) { best = r; break; }
    if (best < 0)
        best = (int)rng_below(rng, (unsigned)m->cols);
    m->walls[maze_idx(m, last, best)] &= (unsigned char)~WALL_BIT(DIR_S);
    m->ep2_r = last;
    m->ep2_c = best;
}
