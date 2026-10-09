/*
 * canvas.c - the off-screen buffer every view draws into.  A frame is
 * built here and flushed to ncurses (or stdout) in one pass, which
 * keeps the isometric painter's algorithm simple and flicker free.
 */

#include "cmaze.h"

int canvas_alloc(Canvas *cv, int w, int h)
{
    size_t n;
    size_t i;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    cv->w = w;
    cv->h = h;
    n = (size_t)w * (size_t)h;
    cv->ch   = malloc(sizeof(wchar_t) * n);
    cv->role = malloc(n);
    if (!cv->ch || !cv->role) {
        free(cv->ch);
        free(cv->role);
        cv->ch = NULL;
        cv->role = NULL;
        return 0;
    }
    /* Blank background: the isometric views do not cover every cell
     * of their bounding box. */
    for (i = 0; i < n; i++) {
        cv->ch[i] = L' ';
        cv->role[i] = CR_BG;
    }
    return 1;
}

void canvas_free(Canvas *cv)
{
    free(cv->ch);
    free(cv->role);
    cv->ch = NULL;
    cv->role = NULL;
}

void canvas_put(Canvas *cv, int x, int y, wchar_t ch, unsigned char role)
{
    if (x < 0 || y < 0 || x >= cv->w || y >= cv->h)
        return;
    cv->ch[(size_t)y * (size_t)cv->w + (size_t)x]   = ch;
    cv->role[(size_t)y * (size_t)cv->w + (size_t)x] = role;
}

void canvas_paint(Canvas *cv, int x, int y, int w, int h,
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

/* ------------------------------------------------------------------ *
 * Curses output
 * ------------------------------------------------------------------ */

void canvas_put_cell(int y, int x, wchar_t ch, short pair, int use_color)
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

/* ------------------------------------------------------------------ *
 * Plain stdout output
 * ------------------------------------------------------------------ */

void canvas_fput_wc(wchar_t wc, FILE *fp)
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
