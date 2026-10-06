/*
 * render.c - turning a grid of walls into characters, colours and
 * screen output.  The same canvas builder drives the curses view and
 * the plain `cmaze -p` dump.
 */

#include "cmaze.h"

/* ------------------------------------------------------------------ *
 * Canvas
 * ------------------------------------------------------------------ */

typedef struct {
    wchar_t       *ch;      /* w*h characters                             */
    unsigned char *role;    /* w*h colour roles, CR_WALL marks wall cells */
    int            w, h;
} Canvas;

static const wchar_t BOX_LINE[16] = {
    L'\u2500',  /* 0000 isolated */
    L'\u2502',  /* 0001 N        */
    L'\u2500',  /* 0010 E        */
    L'\u2514',  /* 0011 NE       */
    L'\u2502',  /* 0100 S        */
    L'\u2502',  /* 0101 NS       */
    L'\u250C',  /* 0110 SE       */
    L'\u251C',  /* 0111 NSE      */
    L'\u2500',  /* 1000 W        */
    L'\u2518',  /* 1001 NW       */
    L'\u2500',  /* 1010 EW       */
    L'\u2534',  /* 1011 NEW      */
    L'\u2510',  /* 1100 SW       */
    L'\u2524',  /* 1101 NSW      */
    L'\u252C',  /* 1110 SEW      */
    L'\u253C'   /* 1111 NSEW     */
};

static int canvas_alloc(Canvas *cv, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    cv->w = w;
    cv->h = h;
    cv->ch   = malloc(sizeof(wchar_t) * (size_t)w * (size_t)h);
    cv->role = malloc((size_t)w * (size_t)h);
    if (!cv->ch || !cv->role) {
        free(cv->ch);
        free(cv->role);
        cv->ch = NULL;
        cv->role = NULL;
        return 0;
    }
    return 1;
}

static void canvas_free(Canvas *cv)
{
    free(cv->ch);
    free(cv->role);
    cv->ch = NULL;
    cv->role = NULL;
}

static void paint(Canvas *cv, int x, int y, int w, int h,
                  wchar_t ch, unsigned char role)
{
    int i, j;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > cv->w) w = cv->w - x;
    if (y + h > cv->h) h = cv->h - y;
    if (w <= 0 || h <= 0)
        return;

    for (j = 0; j < h; j++) {
        size_t base = (size_t)(y + j) * (size_t)cv->w + (size_t)x;
        for (i = 0; i < w; i++) {
            cv->ch[base + i] = ch;
            cv->role[base + i] = role;
        }
    }
}

static int cell_role(const Maze *m, int r, int c)
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

static wchar_t path_char(const Render *R, int role)
{
    if (R->use_color)
        return L' ';
    switch (role) {
    case CR_HEAD:                        return L'@';
    case CR_TRAIL1: case CR_TRAIL2:      return L':';
    case CR_UNVIS:                       return L'.';
    default:                             return L' ';
    }
}

static int cv_wall(const Canvas *cv, int x, int y)
{
    if (x < 0 || y < 0 || x >= cv->w || y >= cv->h)
        return 0;
    return cv->role[(size_t)y * (size_t)cv->w + (size_t)x] == CR_WALL;
}

/* ------------------------------------------------------------------ *
 * Wall texture: seeded ASCII glyphs, grouped by stroke direction so
 * the maze keeps its shape while the walls look hand-typed.
 * ------------------------------------------------------------------ */

static const char TEX_HORZ[]  = "-=~_:.";   /* E-W runs  */
static const char TEX_VERT[]  = "|!:'";     /* N-S runs  */
static const char TEX_JOINT[] = "+#*x@%";   /* corners, tees, caps */
static const char TEX_CROSS[] = "+#x*";     /* NSEW crossing */

static unsigned tex_hash(unsigned x, unsigned y, unsigned seed)
{
    unsigned h = seed ^ 0x9e3779b9u;

    h ^= x * 0x85ebca6bu;
    h = (h << 13) | (h >> 19);
    h ^= y * 0xc2b2ae35u;
    h = (h << 13) | (h >> 19);
    h ^= h >> 16;
    return h;
}

static wchar_t tex_char(const Render *R, int bits, int x, int y)
{
    int ew = (bits & 10) == 10;    /* walls to the east and west */
    int ns = (bits & 5) == 5;      /* walls to the north and south */
    const char *set;
    size_t n, i;

    if (ew && !ns)      set = TEX_HORZ;
    else if (ns && !ew) set = TEX_VERT;
    else if (ew && ns)  set = TEX_CROSS;
    else                set = TEX_JOINT;

    n = strlen(set);
    i = n ? tex_hash((unsigned)x, (unsigned)y, R->tex_seed) % n : 0;
    return (wchar_t)(unsigned char)set[i];
}

static wchar_t wall_char(const Canvas *cv, const Render *R, int x, int y)
{
    int bits;

    if (R->style == STYLE_BLOCK)
        return L'\u2588';

    /* Walls thicker than one character read better as a solid mass;
     * strokes only make sense on a single line. */
    if (R->wall_width > 1)
        return (R->style == STYLE_ASCII || R->style == STYLE_TEXTURE)
             ? L'#' : L'\u2588';

    bits = (cv_wall(cv, x, y - 1) ? 1 : 0)
         | (cv_wall(cv, x + 1, y) ? 2 : 0)
         | (cv_wall(cv, x, y + 1) ? 4 : 0)
         | (cv_wall(cv, x - 1, y) ? 8 : 0);

    if (R->style == STYLE_TEXTURE)
        return tex_char(R, bits, x, y);

    if (R->style == STYLE_ASCII) {
        switch (bits) {
        case 1: case 4: case 5:   return L'|';
        case 2: case 8: case 10:  return L'-';
        default:                  return L'+';
        }
    }
    return BOX_LINE[bits];
}

static void canvas_build(Canvas *cv, const Maze *m, const Render *R)
{
    int ww = R->wall_width;
    int cc = R->corridor;
    wchar_t fill = (R->style == STYLE_ASCII || R->style == STYLE_TEXTURE)
                 ? L'#' : L'\u2588';
    int r, c, i, n;

    n = cv->w * cv->h;
    for (i = 0; i < n; i++) {
        cv->ch[i] = fill;
        cv->role[i] = CR_WALL;
    }

    for (r = 0; r < m->rows; r++) {
        int y0 = ww + r * (cc + ww);
        if (y0 - ww >= cv->h)
            break;
        for (c = 0; c < m->cols; c++) {
            int x0 = ww + c * (cc + ww);
            unsigned char wl;
            int role;
            wchar_t pc;

            if (x0 - ww >= cv->w)
                break;

            wl   = m->walls[maze_idx(m, r, c)];
            role = cell_role(m, r, c);
            pc   = path_char(R, role);

            paint(cv, x0, y0, cc, cc, pc, (unsigned char)role);

            if (!(wl & WALL_BIT(DIR_N)))
                paint(cv, x0, y0 - ww, cc, ww, pc, (unsigned char)role);
            if (!(wl & WALL_BIT(DIR_S)))
                paint(cv, x0, y0 + cc, cc, ww, pc, (unsigned char)role);
            if (!(wl & WALL_BIT(DIR_W)))
                paint(cv, x0 - ww, y0, ww, cc, pc, (unsigned char)role);
            if (!(wl & WALL_BIT(DIR_E)))
                paint(cv, x0 + cc, y0, ww, cc, pc, (unsigned char)role);
        }
    }

    /* Strokes, so corners and junctions meet properly. */
    if (R->style != STYLE_BLOCK && R->wall_width == 1) {
        for (r = 0; r < cv->h; r++) {
            for (c = 0; c < cv->w; c++) {
                size_t idx = (size_t)r * (size_t)cv->w + (size_t)c;
                if (cv->role[idx] == CR_WALL)
                    cv->ch[idx] = wall_char(cv, R, c, r);
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 * Curses output
 * ------------------------------------------------------------------ */

static void put_cell(int y, int x, wchar_t ch, short pair, int use_color)
{
    wchar_t tmp[2];
    cchar_t cc;

    if (y < 0 || y >= LINES || x < 0 || x >= COLS)
        return;
    tmp[0] = ch;
    tmp[1] = L'\0';
    if (setcchar(&cc, tmp, A_NORMAL, use_color ? pair : 0, NULL) == OK)
        mvadd_wch(y, x, &cc);
}

void render_screen(const Maze *m, const Options *o, const Render *R,
                   const Layout *L)
{
    Canvas cv;
    int y, x;

    (void)o;
    if (!canvas_alloc(&cv, L->cw, L->ch))
        return;
    canvas_build(&cv, m, R);

    /* The bottom band belongs to the message box; never draw into it.
     * Clip against both the layout and the *actual* screen, so a stale
     * layout after a resize can never write past the edge and make the
     * terminal wrap lines. */
    int scr_h, scr_w;
    getmaxyx(stdscr, scr_h, scr_w);
    int lim = L->tr - L->msg_h;
    if (lim > scr_h)
        lim = scr_h;

    for (y = 0; y < cv.h; y++) {
        int sy = L->oy + y;
        if (sy >= lim)
            break;
        if (sy < 0)
            continue;
        for (x = 0; x < cv.w; x++) {
            int sx = L->ox + x;
            size_t idx;

            if (sx >= L->tc || sx >= scr_w)
                break;
            if (sx < 0)
                continue;

            idx = (size_t)y * (size_t)cv.w + (size_t)x;
            put_cell(sy, sx, cv.ch[idx], R->pair[cv.role[idx]], R->use_color);
        }
    }
    canvas_free(&cv);
}

/* ------------------------------------------------------------------ *
 * Message box
 * ------------------------------------------------------------------ */

#define MAX_MSG_LINES 12
#define MSG_MAX_W     40     /* longest wrapped line inside the box      */

typedef struct {
    wchar_t buf[512];
    int     starts[MAX_MSG_LINES], lens[MAX_MSG_LINES];
    int     nlines, w, h;
} Msg;

static int wrap_text(const wchar_t *s, int width, int *starts, int *lens)
{
    int len = (int)wcslen(s);
    int i = 0, n = 0;

    if (width < 1)
        width = 1;
    while (i < len && n < MAX_MSG_LINES) {
        int cut, k;

        if (len - i <= width) {
            starts[n] = i;
            lens[n] = len - i;
            n++;
            break;
        }
        cut = i + width;
        for (k = cut; k > i; k--)
            if (s[k] == L' ')
                break;
        if (k == i) {                       /* no space: hard break */
            starts[n] = i;
            lens[n] = width;
            i += width;
        } else {                            /* break at the space */
            starts[n] = i;
            lens[n] = k - i;
            i = k + 1;
            while (i < len && s[i] == L' ')
                i++;
        }
        n++;
    }
    if (n == 0) {
        starts[0] = 0;
        lens[0] = 0;
        n = 1;
    }
    return n;
}

/* Convert and wrap the message so it fits `avail` columns.  Returns 0
 * when there is nothing to draw (no message, or no room at all).       */
static int msg_prepare(const Options *o, int avail, Msg *mb)
{
    int width, i;

    memset(mb, 0, sizeof *mb);
    if (!o->message || !*o->message)
        return 0;

    if (mbstowcs(mb->buf, o->message,
                 sizeof mb->buf / sizeof mb->buf[0] - 1) == (size_t)-1) {
        size_t k;
        for (k = 0; k + 1 < sizeof mb->buf / sizeof mb->buf[0]
                  && o->message[k]; k++)
            mb->buf[k] = (unsigned char)o->message[k];
        mb->buf[k] = L'\0';
    }
    if (mb->buf[0] == L'\0')
        return 0;

    width = avail - 4;                      /* one-column margin each side */
    if (width > MSG_MAX_W)
        width = MSG_MAX_W;
    if (width < 1)
        return 0;

    mb->nlines = wrap_text(mb->buf, width, mb->starts, mb->lens);
    mb->h = mb->nlines + 2;
    mb->w = width + 2;
    for (i = 0; i < mb->nlines; i++)
        if (mb->lens[i] + 2 > mb->w)
            mb->w = mb->lens[i] + 2;
    if (mb->w > avail)
        mb->w = avail;
    if (mb->w < 3)
        return 0;
    return 1;
}

int message_box_geom(const Options *o, int tr, int tc,
                     int *y, int *x, int *w, int *h)
{
    Msg mb;

    if (!msg_prepare(o, tc, &mb)) {
        *y = *x = *w = *h = 0;
        return 0;
    }
    *w = mb.w;
    *h = mb.h;
    *y = tr - mb.h;                         /* own band at the bottom */
    if (*y < 0)
        *y = 0;
    *x = (tc - mb.w) / 2;
    if (*x < 0)
        *x = 0;
    return 1;
}

static void msg_chars(int style, wchar_t *horiz, wchar_t *vert,
                      wchar_t *tl, wchar_t *tr, wchar_t *bl, wchar_t *br)
{
    if (style == STYLE_ASCII || style == STYLE_TEXTURE) {
        *horiz = L'-'; *vert = L'|';
        *tl = *tr = *bl = *br = L'+';
    } else {
        *horiz = L'\u2500'; *vert = L'\u2502';
        *tl = L'\u250C'; *tr = L'\u2510';
        *bl = L'\u2514'; *br = L'\u2518';
    }
}

static void draw_str(int y, int x, const wchar_t *s, int n, int maxw,
                     short pair, int use_color)
{
    int i;

    if (y < 0 || y >= LINES)
        return;
    for (i = 0; i < n && s[i] != L'\0'; i++) {
        int sx = x + i;
        if (sx < 0 || sx >= COLS)
            continue;
        if (i >= maxw)
            break;
        put_cell(y, sx, s[i], pair, use_color);
    }
}

void render_message(const Options *o, const Render *R, const Layout *L)
{
    Msg mb;
    wchar_t horiz, vert, tl, tr, bl, br, space = L' ';
    int i, j;
    short border = R->use_color ? R->pair[CR_HEAD] : 0;
    short text   = R->use_color ? R->pair[CR_ENDPOINT] : 0;

    if (L->msg_h <= 0)
        return;
    if (!msg_prepare(o, L->tc, &mb))
        return;

    msg_chars(R->style, &horiz, &vert, &tl, &tr, &bl, &br);

    int y = L->msg_y, x = L->msg_x;
    int boxw = L->msg_w, boxh = L->msg_h;

    put_cell(y, x, tl, border, R->use_color);
    for (i = 1; i < boxw - 1; i++)
        put_cell(y, x + i, horiz, border, R->use_color);
    put_cell(y, x + boxw - 1, tr, border, R->use_color);

    for (i = 1; i < boxh - 1; i++) {
        put_cell(y + i, x, vert, border, R->use_color);
        for (j = 1; j < boxw - 1; j++)
            put_cell(y + i, x + j, space, text, R->use_color);
        put_cell(y + i, x + boxw - 1, vert, border, R->use_color);
    }

    put_cell(y + boxh - 1, x, bl, border, R->use_color);
    for (i = 1; i < boxw - 1; i++)
        put_cell(y + boxh - 1, x + i, horiz, border, R->use_color);
    put_cell(y + boxh - 1, x + boxw - 1, br, border, R->use_color);

    for (i = 0; i < mb.nlines && 1 + i < boxh - 1; i++)
        draw_str(y + 1 + i, x + 1, mb.buf + mb.starts[i], mb.lens[i],
                 boxw - 2, text, R->use_color);
}

/* ------------------------------------------------------------------ *
 * Plain stdout output
 * ------------------------------------------------------------------ */

static void fput_wc(wchar_t wc, FILE *fp)
{
    unsigned int c = (unsigned int)wc;

    if (c < 0x80) {
        fputc((int)c, fp);
    } else if (c < 0x800) {
        fputc((int)(0xC0 | (c >> 6)), fp);
        fputc((int)(0x80 | (c & 0x3F)), fp);
    } else if (c < 0x10000) {
        fputc((int)(0xE0 | (c >> 12)), fp);
        fputc((int)(0x80 | ((c >> 6) & 0x3F)), fp);
        fputc((int)(0x80 | (c & 0x3F)), fp);
    } else {
        fputc((int)(0xF0 | (c >> 18)), fp);
        fputc((int)(0x80 | ((c >> 12) & 0x3F)), fp);
        fputc((int)(0x80 | ((c >> 6) & 0x3F)), fp);
        fputc((int)(0x80 | (c & 0x3F)), fp);
    }
}

void render_print(const Maze *m, const Options *o, FILE *fp)
{
    Canvas cv;
    Render R;
    int y, x;

    memset(&R, 0, sizeof R);
    R.style      = o->style;
    R.wall_width = o->wall_width;
    R.corridor   = o->corridor;
    R.use_color  = 0;
    R.tex_seed   = (unsigned int)o->seed;

    if (!canvas_alloc(&cv,
                      m->cols * o->corridor + (m->cols + 1) * o->wall_width,
                      m->rows * o->corridor + (m->rows + 1) * o->wall_width))
        return;
    canvas_build(&cv, m, &R);

    for (y = 0; y < cv.h; y++) {
        for (x = 0; x < cv.w; x++)
            fput_wc(cv.ch[(size_t)y * cv.w + x], fp);
        fputc('\n', fp);
    }

    /* Same message box the curses view shows, below the maze. */
    {
        Msg mb;
        wchar_t horiz, vert, tl, ctr, bl, br;
        int i, j;

        if (msg_prepare(o, cv.w, &mb)) {
            msg_chars(o->style, &horiz, &vert, &tl, &ctr, &bl, &br);
            fputc('\n', fp);

            for (j = 0; j < mb.h; j++) {
                for (i = 0; i < mb.w; i++) {
                    wchar_t ch = L' ';
                    if (j == 0)
                        ch = (i == 0) ? tl
                           : (i == mb.w - 1) ? ctr : horiz;
                    else if (j == mb.h - 1)
                        ch = (i == 0) ? bl
                           : (i == mb.w - 1) ? br : horiz;
                    else if (i == 0 || i == mb.w - 1)
                        ch = vert;
                    else if (i - 1 < mb.lens[j - 1])
                        ch = mb.buf[mb.starts[j - 1] + (i - 1)];
                    fput_wc(ch, fp);
                }
                fputc('\n', fp);
            }
        }
    }

    canvas_free(&cv);
}

/* ------------------------------------------------------------------ *
 * Layout
 * ------------------------------------------------------------------ */

void layout_autosize(const Options *o, int tr, int tc, int *rows, int *cols)
{
    int ww = o->wall_width;
    int cc = o->corridor;
    int y, x, w, h, r, c;

    /* Leave room for the message box so the whole output fits. */
    if (message_box_geom(o, tr, tc, &y, &x, &w, &h)) {
        tr -= h;
        if (tr < 1)
            tr = 1;
    }

    c = (o->size_w > 0) ? o->size_w : (tc - ww) / (cc + ww);
    r = (o->size_h > 0) ? o->size_h : (tr - ww) / (cc + ww);

    if (c < 1) c = 1;
    if (r < 1) r = 1;

    *rows = r;
    *cols = c;
}

void layout_place(int rows, int cols, const Options *o, int tr, int tc,
                  Layout *L)
{
    int ww = o->wall_width;
    int cc = o->corridor;
    int band = 0;

    L->rows = rows;
    L->cols = cols;
    L->tr   = tr;
    L->tc   = tc;
    L->cw   = cols * cc + (cols + 1) * ww;
    L->ch   = rows * cc + (rows + 1) * ww;

    /* Reserve a band at the bottom for the message, so the maze and the
     * message never cover each other. */
    if (message_box_geom(o, tr, tc, &L->msg_y, &L->msg_x,
                         &L->msg_w, &L->msg_h)) {
        band = L->msg_h;
        if (band > tr) {                /* box taller than the screen */
            band = (tr > 0) ? tr : 0;
            L->msg_y = 0;
            L->msg_h = band;
        }
    } else {
        L->msg_y = L->msg_x = L->msg_w = L->msg_h = 0;
    }

    L->ox = (tc - L->cw) / 2;
    L->oy = ((tr - band) - L->ch) / 2;
    if (L->ox < 0) L->ox = 0;
    if (L->oy < 0) L->oy = 0;
}
