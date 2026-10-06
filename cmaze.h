/*
 * cmaze - a terminal maze generator, in C + ncurses
 *
 * Common declarations shared by every translation unit.
 */

#ifndef CMAZE_H
#define CMAZE_H

/* Must precede <ncurses.h> so the wide character API (mvadd_wch, cchar_t)
 * is declared.  The Makefile passes -D_XOPEN_SOURCE_EXTENDED=1 as well. */
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#include <ncurses.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define CMAZE_VERSION "1.0.0"

/* ------------------------------------------------------------------ *
 * Directions and walls
 * ------------------------------------------------------------------ */

enum { DIR_N = 0, DIR_E = 1, DIR_S = 2, DIR_W = 3 };

#define WALL_BIT(d)        (1u << (d))
#define DIR_OPPOSITE(d)    (((d) + 2) & 3)

/* Length of the fading colour trail left behind the generator head. */
#define CMAZE_TRAIL_1      8
#define CMAZE_TRAIL_2      26

/* ------------------------------------------------------------------ *
 * Seeded RNG (splitmix64) - identical sequences on every platform
 * ------------------------------------------------------------------ */

typedef struct Rng {
    unsigned long long state;
} Rng;

void          rng_seed(Rng *r, unsigned long long seed);
unsigned long long rng_next(Rng *r);
unsigned      rng_below(Rng *r, unsigned n);   /* 0 .. n-1, n > 0 */
double        rng_double(Rng *r);              /* [0, 1) */

/* ------------------------------------------------------------------ *
 * Maze grid
 * ------------------------------------------------------------------ */

typedef struct Maze {
    int            rows, cols;
    unsigned char *walls;   /* WALL_BIT(dir) set => wall present        */
    unsigned char *vis;     /* 1 once the cell has been carved/visited  */
    int           *stamp;   /* generation tick of first visit, else 0   */
    int            head_r, head_c;  /* active cell, -1 when there is none */
    unsigned int   tick;
    int            ep1_r, ep1_c, ep2_r, ep2_c;  /* decorative endpoints */
} Maze;

Maze *maze_new(int rows, int cols);
void  maze_free(Maze *m);
/* closed=1 => every wall present (carving algorithms)
 * closed=0 => only the border walls (algorithms that build walls)      */
void  maze_reset(Maze *m, int closed);
void  maze_carve(Maze *m, int r, int c, int dir);
void  maze_wall(Maze *m, int r, int c, int dir);
void  maze_mark(Maze *m, int r, int c);
void  maze_braid(Maze *m, int pct, Rng *rng);
void  maze_endpoints(Maze *m, Rng *rng);

static inline int maze_in(const Maze *m, int r, int c)
{
    return r >= 0 && r < m->rows && c >= 0 && c < m->cols;
}

static inline int maze_idx(const Maze *m, int r, int c)
{
    return r * m->cols + c;
}

/* Neighbour of (r,c) in direction d.  Returns 1 when it is inside the grid. */
static inline int maze_nb(const Maze *m, int r, int c, int d, int *nr, int *nc)
{
    *nr = r + (d == DIR_N ? -1 : d == DIR_S ? 1 : 0);
    *nc = c + (d == DIR_E ? 1 : d == DIR_W ? -1 : 0);
    return maze_in(m, *nr, *nc);
}

/* ------------------------------------------------------------------ *
 * Command line options
 * ------------------------------------------------------------------ */

typedef struct Options {
    int    live, infinite, screensaver, print, endpoints;
    double time, wait;
    const char *type;
    unsigned long long seed;
    int    seed_set;
    const char *message;
    const char *colors;
    const char *style_arg;
    const char *theme;
    const char *strategy;
    int    wall_width, corridor, braid;
    int    size_w, size_h;         /* -1 => derive from the terminal */
    const char *save;
    const char *load;
    int    help, version;
    int    style;                  /* resolved STYLE_* */
    int    explicit_mask;          /* which flags the user really typed */
} Options;

/* Bits used to merge a --load file with the command line. */
enum {
    X_TIME     = 1 << 0,
    X_WAIT     = 1 << 1,
    X_TYPE     = 1 << 2,
    X_SEED     = 1 << 3,
    X_STYLE    = 1 << 4,
    X_THEME    = 1 << 5,
    X_WW       = 1 << 6,
    X_CORR     = 1 << 7,
    X_BRAID    = 1 << 8,
    X_STRAT    = 1 << 9,
    X_SIZE     = 1 << 10,
    X_COLORS   = 1 << 11,
    X_MSG      = 1 << 12,
    X_ENDPOINTS = 1 << 13
};

int  options_parse(Options *o, int argc, char **argv);
void options_defaults(Options *o);
void options_print_help(void);
void options_print_version(void);
int  options_load(Options *o, const char *path);
int  options_save(const Options *o, const char *path, unsigned long long seed);

/* ------------------------------------------------------------------ *
 * Algorithm registry
 * ------------------------------------------------------------------ */

typedef struct Algo {
    const char *name;
    const char *desc;
    void *(*create)(Maze *m, Rng *rng, const Options *o);
    int   (*step)(Maze *m, void *state);  /* one animation step, 1 = done */
    void  (*destroy)(void *state);
} Algo;

extern const Algo algo_backtracker, algo_prim, algo_kruskal, algo_wilson,
                  algo_aldous_broder, algo_eller, algo_growing_tree,
                  algo_division, algo_sidewinder, algo_binary_tree,
                  algo_hunt_kill;

const Algo *algo_at(int i);            /* NULL past the end of the list */
const Algo *algo_find(const char *name);
const Algo *algo_pick(Rng *rng);       /* never returns the "random" alias */
int         algo_count(void);

/* ------------------------------------------------------------------ *
 * Colours, themes, rendering
 * ------------------------------------------------------------------ */

enum {
    CR_WALL = 0,
    CR_PATH,
    CR_HEAD,
    CR_TRAIL1,
    CR_TRAIL2,
    CR_UNVIS,
    CR_ENDPOINT,
    CR_COUNT
};

/* Order accepted by --colors=LIST */
#define CMAZE_COLOR_ROLES \
    "wall,path,head,trail1,trail2,unvisited,endpoint"

enum { STYLE_AUTO = 0, STYLE_TEXTURE, STYLE_ASCII, STYLE_UNICODE, STYLE_BLOCK };

typedef struct Render {
    int   style;                  /* resolved STYLE_*                        */
    int   wall_width, corridor;
    int   use_color;
    unsigned int tex_seed;        /* per-maze seed for the wall texture      */
    short pair[CR_COUNT];         /* ncurses colour pair, 0 when disabled    */
    short col[CR_COUNT];          /* resolved colour numbers                 */
} Render;

int   color_parse(const char *s);       /* -> 0..255, or -1 on error        */
short color_nearest(int c);             /* -> best available colour         */
int   theme_lookup(const char *name, short out[CR_COUNT]);
int   style_parse(const char *name);    /* -> STYLE_*                       */
void  render_setup(Render *R, const Options *o);

/* ------------------------------------------------------------------ *
 * Layout
 * ------------------------------------------------------------------ */

typedef struct Layout {
    int rows, cols;     /* maze dimensions in cells                      */
    int cw, ch;         /* rendered size in characters                   */
    int oy, ox;         /* top-left corner of the maze on screen         */
    int tr, tc;         /* terminal size used for centring               */
    int msg_y, msg_x;   /* message box position, 0 size when there is none */
    int msg_w, msg_h;
} Layout;

/* Geometry of the -m message box on a tr x tc screen.  Returns 1 and fills
 * the outputs when a box will be drawn, 0 when there is no message.      */
int  message_box_geom(const Options *o, int tr, int tc,
                      int *y, int *x, int *w, int *h);

/* Derive cell dimensions from a terminal size (or honour --size). */
void layout_autosize(const Options *o, int tr, int tc, int *rows, int *cols);
/* Place a rows x cols maze on a tr x tc screen; the origin is clamped so
 * an oversized maze is simply clipped.  When a message box is configured
 * a band of that height is reserved at the bottom and the maze is centred
 * in what is left, so the two never overlap.                              */
void layout_place(int rows, int cols, const Options *o, int tr, int tc,
                  Layout *L);

/* ------------------------------------------------------------------ *
 * Rendering
 * ------------------------------------------------------------------ */

void render_screen(const Maze *m, const Options *o, const Render *R,
                   const Layout *L);
void render_message(const Options *o, const Render *R, const Layout *L);
void render_print(const Maze *m, const Options *o, FILE *fp);

/* Locale + style detection, called once at start-up.  Returns 1 when the
 * locale can carry UTF-8.                                               */
int  cmaze_locale(void);

#endif /* CMAZE_H */
