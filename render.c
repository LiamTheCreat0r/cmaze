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

static wchar_t wall_char(const Canvas *cv, const Render *R, int x, int y)
{
    int bits;

    if (R->style == STYLE_BLOCK)
        return L'\u2588';

    /* Walls thicker than one character read better as a solid mass;
     * strokes only make sense on a single line. */
    if (R->wall_width > 1)
        return (R->style == STYLE_ASCII) ? L'#' : L'\u2588';

    bits = (cv_wall(cv, x, y - 1) ? 1 : 0)
         | (cv_wall(cv, x + 1, y) ? 2 : 0)
         | (cv_wall(cv, x, y + 1) ? 4 : 0)
         | (cv_wall(cv, x - 1, y) ? 8 : 0);

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
    wchar_t fill = (R->style == STYLE_ASCII) ? L'#' : L'\u2588';
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

    for (y = 0; y < cv.h; y++) {
        int sy = L->oy + y;
        if (sy >= L->tr)
            break;
        if (sy < 0)
            continue;
        for (x = 0; x < cv.w; x++) {
            int sx = L->ox + x;
            size_t idx;

            if (sx >= L->tc)
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

void render_message(const Options *o, const Render *R, const Layout *L)
{
    wchar_t buf[512];
    wchar_t horiz, vert, tl, tr, bl, br, space = L' ';
    int starts[MAX_MSG_LINES], lens[MAX_MSG_LINES];
    int nlines, width, boxh, boxw, y, x, i, j;
    short border = R->use_color ? R->pair[CR_HEAD] : 0;
    short text   = R->use_color ? R->pair[CR_ENDPOINT] : 0;

    if (!o->message || !*o->message)
        return;

    if (mbstowcs(buf, o->message, sizeof buf / sizeof buf[0] - 1) == (size_t)-1) {
        size_t k;
        for (k = 0; k + 1 < sizeof buf / sizeof buf[0] && o->message[k]; k++)
            buf[k] = (unsigned char)o->message[k];
        buf[k] = L'\0';
    }
    if (buf[0] == L'\0')
        return;

    if (R->style == STYLE_ASCII) {
        horiz = L'-'; vert = L'|';
        tl = tr = bl = br = L'+';
    } else {
        horiz = L'\u2500'; vert = L'\u2502';
        tl = L'\u250C'; tr = L'\u2510'; bl = L'\u2514'; br = L'\u2518';
    }

    width = L->tc - 4;
    if (width > 40)
        width = 40;
    if (width < 1)
        return;

    nlines = wrap_text(buf, width, starts, lens);
    boxh = nlines + 2;
    boxw = width + 2;
    for (i = 0; i < nlines; i++)
        if (lens[i] + 2 > boxw)
            boxw = lens[i] + 2;
    if (boxw > L->tc)
        boxw = L->tc;
    if (boxw < 3)
        return;

    y = L->tr - boxh;
    if (y < 0)
        y = 0;
    x = 0;

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

    for (i = 0; i < nlines; i++)
        draw_str(y + 1 + i, x + 1, buf + starts[i], lens[i],
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
    canvas_free(&cv);
}

/* ------------------------------------------------------------------ *
 * Layout
 * ------------------------------------------------------------------ */

void layout_autosize(const Options *o, int tr, int tc, int *rows, int *cols)
{
    int ww = o->wall_width;
    int cc = o->corridor;
    int r, c;

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

    L->rows = rows;
    L->cols = cols;
    L->tr   = tr;
    L->tc   = tc;
    L->cw   = cols * cc + (cols + 1) * ww;
    L->ch   = rows * cc + (rows + 1) * ww;

    L->ox = (tc - L->cw) / 2;
    L->oy = (tr - L->ch) / 2;
    if (L->ox < 0) L->ox = 0;
    if (L->oy < 0) L->oy = 0;
}
