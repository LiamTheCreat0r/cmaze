/*
 * project.c - the maze expanded into tiles, and the grid rotation the
 * isometric and oblique views share.  Kept free of curses so the math
 * is easy to reason about (and test).
 *
 * A maze of rows x cols cells becomes a (2*cols+1) x (2*rows+1) tile
 * grid: odd/odd tiles are cell interiors, even tiles are the wall
 * segments (and corner pillars) between them.
 */

#include "cmaze.h"

static int seg_hwall(const Maze *m, int r, int c)   /* between (r,c-1),(r,c) */
{
    if (c > 0 && c < m->cols) {
        return (m->walls[maze_idx(m, r, c - 1)] & WALL_BIT(DIR_E)) ||
               (m->walls[maze_idx(m, r, c)]     & WALL_BIT(DIR_W));
    }
    if (c <= 0)
        return m->walls[maze_idx(m, r, 0)] & WALL_BIT(DIR_W);
    return m->walls[maze_idx(m, r, m->cols - 1)] & WALL_BIT(DIR_E);
}

static int seg_vwall(const Maze *m, int r, int c)   /* between (r-1,c),(r,c) */
{
    if (r > 0 && r < m->rows) {
        return (m->walls[maze_idx(m, r - 1, c)] & WALL_BIT(DIR_S)) ||
               (m->walls[maze_idx(m, r, c)]     & WALL_BIT(DIR_N));
    }
    if (r <= 0)
        return m->walls[maze_idx(m, 0, c)] & WALL_BIT(DIR_N);
    return m->walls[maze_idx(m, m->rows - 1, c)] & WALL_BIT(DIR_S);
}

int tiles_build(const Maze *m, Tiles *t)
{
    int tw = 2 * m->cols + 1, th = 2 * m->rows + 1;
    int x, y;

    t->w = tw;
    t->h = th;
    t->wall = calloc((size_t)tw * (size_t)th, 1);
    t->role = calloc((size_t)tw * (size_t)th, 1);
    if (!t->wall || !t->role) {
        tiles_free(t);
        return -1;
    }

    for (y = 0; y < th; y++) {
        for (x = 0; x < tw; x++) {
            size_t i = (size_t)y * (size_t)tw + (size_t)x;
            int oddx = x & 1, oddy = y & 1;

            if (oddx && oddy) {
                /* Cell interior.  Not yet carved => still solid rock,
                 * which makes live carving look like digging. */
                int r = y / 2, c = x / 2;
                if (m->vis[maze_idx(m, r, c)]) {
                    t->role[i] = (unsigned char)maze_cell_role(m, r, c);
                } else {
                    t->wall[i] = 1;
                    t->role[i] = CR_WALL;
                }
            } else if (!oddx && oddy) {
                t->wall[i] = (unsigned char)seg_hwall(m, y / 2, x / 2);
                t->role[i] = CR_WALL;
            } else if (oddx && !oddy) {
                t->wall[i] = (unsigned char)seg_vwall(m, y / 2, x / 2);
                t->role[i] = CR_WALL;
            } else {
                /* Corner pillar at (x, y): solid when any of the four
                 * segments touching it is a wall. */
                int wy = (y - 1 >= 0) ? (y - 1) / 2 : -1;   /* tile (x,y-1) */
                t->wall[i] = (unsigned char)
                    ((wy >= 0 && seg_hwall(m, wy, x / 2)) ||
                     seg_hwall(m, y / 2, x / 2) ||          /* tile (x,y+1) */
                     ((x - 1 >= 0) && seg_vwall(m, y / 2, (x - 1) / 2)) ||
                     seg_vwall(m, y / 2, x / 2));
                t->role[i] = CR_WALL;
            }
        }
    }
    return 0;
}

void tiles_free(Tiles *t)
{
    free(t->wall);
    free(t->role);
    t->wall = NULL;
    t->role = NULL;
}

void grid_rotate(int rot, int tw, int th, int x, int y, int *rx, int *ry)
{
    switch (rot & 3) {
    case 1:  *rx = y;             *ry = tw - 1 - x; break;
    case 2:  *rx = tw - 1 - x;    *ry = th - 1 - y; break;
    case 3:  *rx = th - 1 - y;    *ry = x;           break;
    default: *rx = x;             *ry = y;           break;
    }
}
