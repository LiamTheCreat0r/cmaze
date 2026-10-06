/*
 * theme.c - colour names, palettes and colour pair setup.
 *
 * Colours are addressed as xterm-256 indices.  Anything the terminal
 * cannot display is remapped to the nearest colour it does have, so the
 * same theme works on an 8-colour $TERM as well as a 256-colour one.
 */

#include "cmaze.h"

#include <ctype.h>
#include <langinfo.h>
#include <locale.h>

/* ------------------------------------------------------------------ */

static void xterm_rgb(int c, int *R, int *G, int *B)
{
    static const int base[16][3] = {
        {   0,   0,   0 }, { 128,   0,   0 }, {   0, 128,   0 }, { 128, 128,   0 },
        {   0,   0, 128 }, { 128,   0, 128 }, {   0, 128, 128 }, { 192, 192, 192 },
        { 128, 128, 128 }, { 255,   0,   0 }, {   0, 255,   0 }, { 255, 255,   0 },
        {   0,   0, 255 }, { 255,   0, 255 }, {   0, 255, 255 }, { 255, 255, 255 }
    };
    static const int ramp[6] = { 0, 95, 135, 175, 215, 255 };

    if (c < 0)
        c = 0;
    if (c > 255)
        c = 255;

    if (c < 16) {
        *R = base[c][0];
        *G = base[c][1];
        *B = base[c][2];
    } else if (c < 232) {
        int i = c - 16;
        *R = ramp[i / 36];
        *G = ramp[(i / 6) % 6];
        *B = ramp[i % 6];
    } else {
        int v = 8 + (c - 232) * 10;
        *R = *G = *B = v;
    }
}

static int rgb_dist2(int c, int r, int g, int b)
{
    int cr, cg, cb, dr, dg, db;

    xterm_rgb(c, &cr, &cg, &cb);
    dr = cr - r;
    dg = cg - g;
    db = cb - b;
    return dr * dr + dg * dg + db * db;
}

static int nearest_rgb(int r, int g, int b)
{
    int i, best = 0, bestd = 0x7FFFFFFF;

    for (i = 0; i < 256; i++) {
        int d = rgb_dist2(i, r, g, b);
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

short color_nearest(int c)
{
    int i, best = 0, bestd = 0x7FFFFFFF;
    int r, g, b, limit;

    if (c < 0)
        return -1;
    if (COLORS >= 256)
        return (short)(c > 255 ? 255 : c);

    limit = COLORS;
    if (limit < 2)
        return 0;
    if (limit > 256)
        limit = 256;

    xterm_rgb(c > 255 ? 255 : c, &r, &g, &b);
    for (i = 0; i < limit; i++) {
        int d = rgb_dist2(i, r, g, b);
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return (short)best;
}

/* ------------------------------------------------------------------ */

struct nameent { const char *name; int idx; };

static const struct nameent names[] = {
    { "black",          0 }, { "maroon",       1 }, { "green",        2 },
    { "olive",         10 }, { "blue",         4 }, { "purple",     129 },
    { "cyan",           6 }, { "white",        7 }, { "silver",     251 },
    { "gray",         245 }, { "grey",       245 }, { "darkgray",   243 },
    { "lightgray",    252 }, { "red",          1 }, { "brightred",    9 },
    { "lightred",       9 }, { "crimson",    196 }, { "brightgreen", 10 },
    { "lightgreen",    10 }, { "lime",        46 }, { "seagreen",    29 },
    { "forest",        28 }, { "brightyellow",11 }, { "lightyellow", 11 },
    { "gold",         220 }, { "khaki",      222 }, { "brightblue",  12 },
    { "lightblue",     12 }, { "navy",         4 }, { "slate",       67 },
    { "sky",          117 }, { "azure",       45 }, { "brightmagenta", 13 },
    { "lightmagenta",  13 }, { "magenta",      5 }, { "pink",       218 },
    { "violet",       177 }, { "indigo",      54 }, { "orchid",     175 },
    { "brightcyan",    14 }, { "lightcyan",   14 }, { "teal",        30 },
    { "turquoise",     44 }, { "aquamarine", 122 }, { "brightwhite", 15 },
    { "lightwhite",    15 }, { "orange",     214 }, { "coral",      203 },
    { "salmon",       209 }, { "brown",       94 }, { "tan",        180 },
    { "wheat",        223 }, { "ivory",      231 }, { "darkgreen",   22 },
    { "darkblue",      18 }, { "darkred",     88 }, { "darkmagenta", 90 },
    { "darkcyan",      23 }, { "darkyellow",  58 }
};

static void normalize(const char *in, char *out, size_t n)
{
    size_t k = 0;

    for (; *in && k + 1 < n; in++) {
        char ch = *in;
        if (ch == '-' || ch == '_' || ch == ' ')
            continue;
        if (ch >= 'A' && ch <= 'Z')
            ch = (char)(ch - 'A' + 'a');
        out[k++] = ch;
    }
    out[k] = '\0';
}

static int hex2(char a, char b)
{
    char pair[3];

    pair[0] = a;
    pair[1] = b;
    pair[2] = '\0';
    return (int)strtol(pair, NULL, 16);
}

int color_parse(const char *s)
{
    char buf[64], norm[64];
    const char *p;
    size_t i, len;
    long v;
    char *end;

    if (!s)
        return -1;

    while (*s == ' ' || *s == '\t')
        s++;
    snprintf(buf, sizeof buf, "%s", s);
    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t'))
        buf[--len] = '\0';
    if (len == 0)
        return -1;

    /* Hex: #rrggbb, 0xrrggbb or rrggbb */
    p = buf;
    if (*p == '#')
        p++;
    else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;

    if (strlen(p) == 6) {
        int ok = 1, k;
        for (k = 0; k < 6; k++)
            if (!isxdigit((unsigned char)p[k]))
                ok = 0;
        if (ok)
            return nearest_rgb(hex2(p[0], p[1]),
                               hex2(p[2], p[3]),
                               hex2(p[4], p[5]));
    }

    /* Plain number 0..255 */
    v = strtol(buf, &end, 10);
    if (*end == '\0' && end != buf && v >= 0 && v <= 255)
        return (int)v;

    /* Colour name */
    normalize(buf, norm, sizeof norm);
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
        if (strcmp(names[i].name, norm) == 0)
            return names[i].idx;

    return -1;
}

/* ------------------------------------------------------------------ */

struct themeent { const char *name; short c[CR_COUNT]; };

/* wall, path, head, trail1, trail2, unvisited, endpoint */
static const struct themeent themes[] = {
    { "default", {  63, 251,  51,  50,  44, 238, 226 } },
    { "forest",  {  29, 108, 118,  78,  43,  22, 226 } },
    { "ocean",   {  33, 109, 123,  45,  38,  17, 122 } },
    { "amber",   { 166, 143, 226, 214, 172,  58, 231 } },
    { "mono",    { 245, 242, 255, 251, 248, 236, 231 } }
};

int theme_lookup(const char *name, short out[CR_COUNT])
{
    size_t i;
    char norm[64];

    if (name)
        normalize(name, norm, sizeof norm);
    else
        norm[0] = '\0';

    for (i = 0; i < sizeof themes / sizeof themes[0]; i++) {
        char tnorm[64];
        normalize(themes[i].name, tnorm, sizeof tnorm);
        if (strcmp(tnorm, norm) == 0) {
            memcpy(out, themes[i].c, sizeof(short) * CR_COUNT);
            return 0;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */

int style_parse(const char *name)
{
    char norm[64];

    if (!name || !*name)
        return STYLE_AUTO;
    normalize(name, norm, sizeof norm);

    if (strcmp(norm, "auto") == 0)    return STYLE_AUTO;
    if (strcmp(norm, "texture") == 0) return STYLE_TEXTURE;
    if (strcmp(norm, "ascii") == 0)   return STYLE_ASCII;
    if (strcmp(norm, "unicode") == 0) return STYLE_UNICODE;
    if (strcmp(norm, "utf8") == 0)    return STYLE_UNICODE;
    if (strcmp(norm, "block") == 0)   return STYLE_BLOCK;
    if (strcmp(norm, "solid") == 0)   return STYLE_BLOCK;
    return -1;
}

/* ------------------------------------------------------------------ */

int cmaze_locale(void)
{
    const char *l = setlocale(LC_ALL, "");

    if (!l || !*l)
        setlocale(LC_ALL, "C.UTF-8");
    else if (strcmp(l, "C") == 0 || strcmp(l, "POSIX") == 0)
        setlocale(LC_ALL, "en_US.UTF-8");

    return strcmp(nl_langinfo(CODESET), "UTF-8") == 0;
}

/* ------------------------------------------------------------------ */

void render_setup(Render *R, const Options *o)
{
    short cols[CR_COUNT];
    int i, has, bg = 0;

    memset(R, 0, sizeof *R);
    R->wall_width = o->wall_width;
    R->corridor   = o->corridor;
    R->style      = o->style;

    has = has_colors();
    if (has)
        has = (start_color() != ERR);
    if (has) {
#ifdef NCURSES_VERSION
        if (use_default_colors() != ERR)
            bg = -1;
#endif
    }
    R->use_color = has;

    if (theme_lookup(o->theme, cols) != 0 &&
        theme_lookup("default", cols) != 0) {
        for (i = 0; i < CR_COUNT; i++)
            cols[i] = 245;
    }

    /* --colors=wall,path,head,trail1,trail2,unvisited,endpoint */
    if (o->colors && *o->colors) {
        char *dup = strdup(o->colors);
        char *tok = dup;
        int   idx = 0;

        while (tok && idx < CR_COUNT) {
            char *comma = strchr(tok, ',');
            int   v;
            if (comma)
                *comma = '\0';
            v = color_parse(tok);
            if (v >= 0)
                cols[idx] = (short)v;
            idx++;
            tok = comma ? comma + 1 : NULL;
        }
        free(dup);
    }

    if (!R->use_color)
        return;

    for (i = 0; i < CR_COUNT; i++) {
        R->col[i]  = color_nearest(cols[i]);
        R->pair[i] = (short)(i + 1);
        if (init_pair(R->pair[i], R->col[i], (short)bg) == ERR)
            R->pair[i] = 0;
    }
}
