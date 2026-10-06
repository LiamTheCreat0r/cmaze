/*
 * main.c - setup, the main loop, and the input/resize plumbing.
 */

#include "cmaze.h"

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

/* Guard against an algorithm that never reports "done". */
#define STEP_CAP 50000000L

static volatile sig_atomic_t g_quit;
static volatile sig_atomic_t g_winch;

static void on_quit(int s) { (void)s; g_quit = 1; }
static void on_winch(int s) { (void)s; g_winch = 1; }

static double now_sec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static unsigned long long default_seed(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000000ULL
         + (unsigned long long)tv.tv_usec
         + (unsigned long long)getpid() * 1000003ULL;
}

static void term_size(int *rows, int *cols)
{
    struct winsize ws;

    *rows = 24;
    *cols = 80;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_row > 1 && ws.ws_col > 1) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0 &&
               ws.ws_row > 1 && ws.ws_col > 1) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}

/* ------------------------------------------------------------------ *
 * Generation
 * ------------------------------------------------------------------ */

typedef struct {
    Maze       *m;
    const Algo *algo;
    void       *st;
} Gen;

static void gen_free(Gen *g)
{
    if (g->st && g->algo)
        g->algo->destroy(g->st);
    if (g->m)
        maze_free(g->m);
    g->st = NULL;
    g->m = NULL;
    g->algo = NULL;
}

static int gen_start(Gen *g, const Options *o, const Layout *L, Rng *rng)
{
    memset(g, 0, sizeof *g);

    g->m = maze_new(L->rows, L->cols);
    if (!g->m)
        return -1;

    if (!o->type || strcmp(o->type, "random") == 0)
        g->algo = algo_pick(rng);
    else
        g->algo = algo_find(o->type);

    if (!g->algo) {
        maze_free(g->m);
        g->m = NULL;
        return -1;
    }

    g->st = g->algo->create(g->m, rng, o);
    if (!g->st) {
        maze_free(g->m);
        g->m = NULL;
        g->algo = NULL;
        return -1;
    }
    return 0;
}

static void gen_finish(Gen *g, const Options *o, Rng *rng)
{
    if (o->braid > 0)
        maze_braid(g->m, o->braid, rng);
    if (o->endpoints)
        maze_endpoints(g->m, rng);

    /* Settle into a clean, uniform maze for the final screenshot. */
    g->m->head_r = g->m->head_c = -1;
    g->m->tick += CMAZE_TRAIL_2 + 1;
}

/* ------------------------------------------------------------------ *
 * Non-curses mode: cmaze -p
 * ------------------------------------------------------------------ */

static int run_print(Options *o)
{
    Layout L;
    Rng rng;
    Gen g;
    unsigned long long seed;
    int tr, tc, rows, cols;
    long steps = 0;

    term_size(&tr, &tc);
    layout_autosize(o, tr, tc, &rows, &cols);
    layout_place(rows, cols, o, tr, tc, &L);

    seed = o->seed_set ? o->seed : default_seed();
    o->seed = seed;                 /* the texture is hashed from this */
    rng_seed(&rng, seed);

    if (gen_start(&g, o, &L, &rng) != 0) {
        fprintf(stderr, "cmaze: out of memory\n");
        return 1;
    }

    for (;;) {
        if (g.algo->step(g.m, g.st))
            break;
        g.m->tick++;
        if (++steps > STEP_CAP) {
            fprintf(stderr, "cmaze: %s did not finish, giving up\n",
                    g.algo->name);
            break;
        }
    }
    g.algo->destroy(g.st);
    g.st = NULL;

    gen_finish(&g, o, &rng);
    render_print(g.m, o, stdout);

    if (o->save)
        options_save(o, o->save, seed);

    maze_free(g.m);
    return 0;
}

/* ------------------------------------------------------------------ *
 * Curses mode
 * ------------------------------------------------------------------ */

typedef struct Ui {
    Options      *o;
    Render       *R;
    Layout       *L;
    const Maze   *m;         /* maze currently on screen, or NULL */
    int           restart;   /* terminal changed shape: rebuild the maze */
    int           changed;   /* layout moved: redraw                     */
} Ui;

static void ui_draw(Ui *u, int do_clear)
{
    if (do_clear)
        clear();
    if (u->m)
        render_screen(u->m, u->o, u->R, u->L);
    if (u->o->message)
        render_message(u->o, u->R, u->L);
    refresh();
}

static void ui_winch(Ui *u)
{
    struct winsize ws;
    int tr = 0, tc = 0, rows, cols;
    Layout nl;

    g_winch = 0;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_row > 0 && ws.ws_col > 0) {
        tr = ws.ws_row;
        tc = ws.ws_col;
    } else {
        getmaxyx(stdscr, tr, tc);   /* never resizeterm(0, 0) */
    }
    resizeterm(tr, tc);
    getmaxyx(stdscr, tr, tc);

    layout_autosize(u->o, tr, tc, &rows, &cols);
    layout_place(rows, cols, u->o, tr, tc, &nl);

    if (rows == u->L->rows && cols == u->L->cols) {
        *u->L = nl;
        u->changed = 1;
    } else if (nl.cw <= tc && nl.ch <= tr) {
        u->restart = 1;             /* rebuild at the new size */
    } else {
        *u->L = nl;                 /* oversized on purpose: draw clipped */
        u->changed = 1;
    }
}

/* Returns 1 when the program should exit. */
static int ui_key(Ui *u)
{
    int ch;

    if (g_quit)
        return 1;
    if (g_winch) {
        ui_winch(u);
        return 0;
    }

    ch = getch();
    if (ch == ERR)
        return 0;
    if (ch == KEY_RESIZE) {
        ui_winch(u);
        return 0;
    }
    if (u->o->screensaver)
        return 1;
    if (ch == 'q' || ch == 'Q' || ch == 27)
        return 1;
    return 0;
}

/* Wait `seconds` (negative = forever) while staying responsive.
 * Returns 1 when the user asked to quit. */
static int ui_pause(Ui *u, double seconds)
{
    double deadline = now_sec() + seconds;

    timeout(50);
    for (;;) {
        if (ui_key(u))
            return 1;
        if (u->restart)
            return 0;
        if (u->changed) {
            u->changed = 0;
            ui_draw(u, 1);
        }
        if (seconds >= 0 && now_sec() >= deadline)
            return 0;
    }
}

static int run_curses(Options *o)
{
    Render R;
    Ui u;
    unsigned long long base, maze_no = 0;
    int saved = 0, status = 0;
    struct sigaction sa;

    if (!isatty(STDOUT_FILENO)) {
        fprintf(stderr, "cmaze: stdout is not a terminal\n");
        fprintf(stderr, "cmaze: use 'cmaze -p' to print a maze instead\n");
        return 1;
    }

    if (!initscr()) {
        fprintf(stderr, "cmaze: cannot start curses\n");
        return 1;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    leaveok(stdscr, TRUE);
    scrollok(stdscr, FALSE);        /* drawing must never scroll the screen */

    /* If ncurses sized the screen from terminfo and it disagrees with
     * the real terminal, lines get wrapped by the physical terminal and
     * everything looks shifted.  Trust the ioctl. */
    {
        int ar, ac, ir, ic;

        getmaxyx(stdscr, ar, ac);
        term_size(&ir, &ic);
        if (ar != ir || ac != ic)
            resizeterm(ir, ic);
    }

    render_setup(&R, o);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_quit;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa, NULL);

    base = o->seed_set ? o->seed : default_seed();

    u.o = o;
    u.R = &R;
    u.m = NULL;

    for (;;) {
        Layout L;
        Gen g;
        Rng rng;
        unsigned long long seed;
        int rows, cols, tr, tc, stop = 0;
        long steps = 0;

        getmaxyx(stdscr, tr, tc);
        layout_autosize(o, tr, tc, &rows, &cols);
        layout_place(rows, cols, o, tr, tc, &L);

        seed = base + maze_no;
        rng_seed(&rng, seed);
        R.tex_seed = (unsigned int)seed;    /* wall texture varies per maze */

        u.L = &L;
        u.m = NULL;
        u.restart = 0;
        u.changed = 0;

        if (gen_start(&g, o, &L, &rng) != 0) {
            status = 1;
            break;
        }
        u.m = g.m;

        ui_draw(&u, 1);

        /* ---- carve ---- */
        for (;;) {
            int done = g.algo->step(g.m, g.st);
            g.m->tick++;

            if (o->live) {
                timeout((int)(o->time * 1000.0));
                ui_draw(&u, 0);
                if (ui_key(&u)) {
                    stop = 1;
                    break;
                }
            } else if (g_quit) {
                stop = 1;
                break;
            } else if (g_winch) {
                ui_winch(&u);
            }

            if (u.restart)
                break;
            if (++steps > STEP_CAP) {
                endwin();
                fprintf(stderr, "cmaze: %s did not finish, giving up\n",
                        g.algo->name);
                stop = 1;
                break;
            }
            if (done)
                break;
        }

        if (stop) {
            u.m = NULL;
            gen_free(&g);
            break;
        }
        if (u.restart) {
            u.m = NULL;
            gen_free(&g);
            maze_no++;
            continue;
        }

        /* ---- settle and show ---- */
        gen_finish(&g, o, &rng);
        u.changed = 0;
        ui_draw(&u, 1);

        if (o->save && !saved) {
            if (options_save(o, o->save, seed) != 0)
                fprintf(stderr, "cmaze: cannot write %s\n", o->save);
            saved = 1;
        }

        if (!o->infinite) {
            if (ui_pause(&u, -1.0)) {      /* the user asked to quit */
                u.m = NULL;
                gen_free(&g);
                break;
            }
            /* Terminal changed shape: rebuild at the new size instead
             * of falling through and exiting.  Keep the seed so the
             * maze only changes shape, not identity. */
            u.m = NULL;
            gen_free(&g);
            continue;
        }

        if (ui_pause(&u, o->wait)) {
            u.m = NULL;
            gen_free(&g);
            break;
        }

        u.m = NULL;
        gen_free(&g);
        maze_no++;
    }

    endwin();
    return status;
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    Options o;
    int utf8;

    options_defaults(&o);
    if (options_parse(&o, argc, argv) != 0)
        return 2;

    if (o.help) {
        options_print_help();
        return 0;
    }
    if (o.version) {
        options_print_version();
        return 0;
    }

    if (o.style_arg)
        o.style = style_parse(o.style_arg);

    utf8 = cmaze_locale();
    if (o.style == STYLE_AUTO)
        o.style = STYLE_TEXTURE;
    if (!utf8 && (o.style == STYLE_UNICODE || o.style == STYLE_BLOCK)) {
        fprintf(stderr, "cmaze: no UTF-8 locale, falling back to texture walls\n");
        o.style = STYLE_TEXTURE;
    }

    if (o.print)
        return run_print(&o);
    return run_curses(&o);
}
