/*
 * render_iso.c - the isometric (2:1 dimetric) and oblique views.
 *
 * Both views expand the maze into a tile grid (project.c), rotate it
 * (--rotate) and project it with a parallel projection, drawing tiles
 * back to front (painter's algorithm) into the off-screen canvas, so
 * walls in front correctly hide what is behind them.
 *
 * 2:1 isometric projection, for a tile at rotated coordinates (rx, ry)
 * and height z (in rows):
 *
 *     screen_col = (rx - ry) * 2
 *     screen_row = (rx + ry) - z
 *
 * A tile footprint is a diamond 4 columns wide and 2 rows tall; a wall
 * block is that diamond lifted `wall_height` rows, plus the left and
 * right side faces filling the gap down to the floor.
 */

#include "cmaze.h"

/* Inverse of grid_rotate(): rotated coords back to original tiles. */
static void grid_unrotate(int rot, int tw, int th, int rx, int ry,
                          int *x, int *y)
{
    switch (rot & 3) {
    case 1:  *x = tw - 1 - ry;  *y = rx;           break;
    case 2:  *x = tw - 1 - rx;  *y = th - 1 - ry;  break;
    case 3:  *x = ry;           *y = th - 1 - rx;  break;
    default: *x = rx;           *y = ry;           break;
    }
}

/* ------------------------------------------------------------------ *
 * Glyph sets.  With colour on, faces are filled with spaces drawn in
 * a pair whose foreground and background are the same shade, so the
 * faces read as solid slabs of colour in any glyph style.
 * ------------------------------------------------------------------ */

typedef struct IsoGlyphs {
    wchar_t top_l;       /* top face diamond:  row a  cols c-1,c       */
    wchar_t top_m;
    wchar_t top_ll;      /*                    row a+1 col  c-2       */
    wchar_t top_mm;      /*                          cols c-1,c        */
    wchar_t top_r;       /*                          col  c+1          */
    wchar_t side_l_edge; /* left face:  col c-2                         */
    wchar_t side_l_fill; /*             col c-1                         */
    wchar_t side_r_fill; /* right face: col c                           */
    wchar_t side_r_edge; /*             col c+1                         */
} IsoGlyphs;

static const IsoGlyphs iso_glyphs_ascii = {
    L'_', L'_', L'/', L'_', L'\\', L'|', L':', L'.', L'|'
};

static const IsoGlyphs iso_glyphs_box = {
    L'\u2500', L'\u2500', L'\u2571', L'\u2500', L'\u2572',
    L'\u2502', L' ',      L' ',      L'\u2502'
};

static const IsoGlyphs iso_glyphs_block = {
    L'\u2588', L'\u2588', L'\u2588', L'\u2588', L'\u2588',
    L'\u2593', L'\u2593', L'\u2592', L'\u2592'
};

/* Solid colour slabs (spaces in pairs whose fg == bg) only read well
 * when the terminal can show the three shades apart; on 8/16 colour
 * terminals fall back to the per-face glyphs, which carry the shading
 * through density instead. */
static int solid_ok(const Render *R)
{
    return R->use_color && COLORS >= 16;
}

static const IsoGlyphs *iso_glyphs_for(int style, int use_color)
{
    if (use_color) {
        /* Solid colour slabs; the shape comes from the colours. */
        static const IsoGlyphs flat = {
            L' ', L' ', L' ', L' ', L' ', L' ', L' ', L' ', L' '
        };
        return &flat;
    }
    switch (style) {
    case ISO_BOX:    return &iso_glyphs_box;
    case ISO_BLOCK:  return &iso_glyphs_block;
    default:         return &iso_glyphs_ascii;
    }
}

/* Glyph for a floor tile when colour cannot carry the role. */
static wchar_t iso_floor_char(int role)
{
    switch (role) {
    case CR_HEAD:                       return L'@';
    case CR_TRAIL1: case CR_TRAIL2:     return L':';
    case CR_UNVIS:                      return L'.';
    case CR_ENDPOINT:                   return L'*';
    default:                            return L' ';
    }
}

/* ------------------------------------------------------------------ *
 * Isometric drawing
 * ------------------------------------------------------------------ */

/* Draw one tile: floor diamond at z=0, or the wall block standing on
 * it.  Roles are stamped so the flush can pick the right colour pair. */
static void iso_tile(Canvas *cv, const Options *o, const Render *R,
                     const IsoGlyphs *g, int c, int a,
                     int is_wall, int role)
{
    int k, hh = (o->wall_height < 1) ? 1 : o->wall_height;
    unsigned char r_top = (unsigned char)
        (o->shade ? CR_WTOP : CR_WALL);
    unsigned char r_left = (unsigned char)
        (o->shade ? CR_WLEFT : CR_WALL);
    unsigned char r_right = (unsigned char)
        (o->shade ? CR_WRIGHT : CR_WALL);
    unsigned char r_floor = (unsigned char)role;
    wchar_t fch;

    if (!is_wall && o->floor) {
        if (solid_ok(R)) {
            fch = L' ';
            canvas_put(cv, c - 1, a, fch, r_floor);
            canvas_put(cv, c,     a, fch, r_floor);
            canvas_put(cv, c - 2, a + 1, fch, r_floor);
            canvas_put(cv, c - 1, a + 1, fch, r_floor);
            canvas_put(cv, c,     a + 1, fch, r_floor);
            canvas_put(cv, c + 1, a + 1, fch, r_floor);
        } else if (o->iso_style == ISO_BLOCK) {
            /* Light shade floor so corridors read between the blocks. */
            fch = (role == CR_PATH || role == CR_ENDPOINT)
                ? L'\u2591' : iso_floor_char(role);
            canvas_put(cv, c - 1, a, fch, r_floor);
            canvas_put(cv, c,     a, fch, r_floor);
            canvas_put(cv, c - 2, a + 1, fch, r_floor);
            canvas_put(cv, c - 1, a + 1, fch, r_floor);
            canvas_put(cv, c,     a + 1, fch, r_floor);
            canvas_put(cv, c + 1, a + 1, fch, r_floor);
        } else {
            /* A faint diamond outline, so corridors read as holes
             * between the solid wall tops, with the role glyph
             * inside it. */
            fch = iso_floor_char(role);
            canvas_put(cv, c - 1, a, L' ', r_floor);
            canvas_put(cv, c,     a, L' ', r_floor);
            canvas_put(cv, c - 2, a + 1, g->top_ll, r_floor);
            canvas_put(cv, c - 1, a + 1, fch, r_floor);
            canvas_put(cv, c,     a + 1, fch, r_floor);
            canvas_put(cv, c + 1, a + 1, g->top_r, r_floor);
        }
        return;
    }
    if (!is_wall)
        return;

    /* Side faces, bottom row first so the top face overlaps cleanly. */
    for (k = 0; k < hh; k++) {
        int row = a + 1 - k;
        canvas_put(cv, c - 2, row, g->side_l_edge, r_left);
        canvas_put(cv, c - 1, row, g->side_l_fill, r_left);
        canvas_put(cv, c,     row, g->side_r_fill, r_right);
        canvas_put(cv, c + 1, row, g->side_r_edge, r_right);
    }

    /* Top face, raised by hh rows. */
    canvas_put(cv, c - 1, a - hh,     g->top_l,  r_top);
    canvas_put(cv, c,     a - hh,     g->top_m,  r_top);
    canvas_put(cv, c - 2, a - hh + 1, g->top_ll, r_top);
    canvas_put(cv, c - 1, a - hh + 1, g->top_mm, r_top);
    canvas_put(cv, c,     a - hh + 1, g->top_mm, r_top);
    canvas_put(cv, c + 1, a - hh + 1, g->top_r,  r_top);
}

void iso_size(const Options *o, int rows, int cols, int *w, int *h)
{
    int tw = 2 * cols + 1, th = 2 * rows + 1;
    int hh = o->wall_height;

    if (hh < 1)
        hh = 1;
    *w = 2 * (tw + th);                 /* diamond footprint, 2:1 slope  */
    *h = tw + th + hh;                  /* floor rows + raised tops      */
}

void iso_build(Canvas *cv, const Maze *m, const Options *o, const Render *R)
{
    Tiles t;
    const IsoGlyphs *g;
    int tw, th, hh, RW, RH, s, rx, ry, x, y;
    int col_off, row_off;

    if (tiles_build(m, &t) != 0)
        return;
    tw = t.w;
    th = t.h;
    hh = (o->wall_height < 1) ? 1 : o->wall_height;

    /* -col_min / -row_min of the projection, so the drawing starts
     * inside the canvas. */
    RW = (o->rotate & 1) ? th : tw;
    RH = (o->rotate & 1) ? tw : th;
    col_off = 2 * RH;
    row_off = hh;

    g = iso_glyphs_for(o->iso_style, solid_ok(R));

    /* Painter's algorithm: back to front, by increasing rx + ry. */
    for (s = 0; s <= RW + RH - 2; s++) {
        for (rx = 0; rx < RW; rx++) {
            size_t i;
            int c, a;

            ry = s - rx;
            if (ry < 0 || ry >= RH)
                continue;
            grid_unrotate(o->rotate, tw, th, rx, ry, &x, &y);
            i = (size_t)y * (size_t)tw + (size_t)x;

            c = (rx - ry) * 2 + col_off;
            a = rx + ry + row_off;
            iso_tile(cv, o, R, g, c, a, t.wall[i], t.role[i]);
        }
    }

    tiles_free(&t);
}

/* ------------------------------------------------------------------ *
 * Oblique drawing: front-facing tiles, depth shown by a diagonal
 * offset.  Floor stays rectangular; a wall shows a front face, a top
 * face and one side face.
 * ------------------------------------------------------------------ */

typedef struct OblGlyphs {
    wchar_t top_l, top_r;      /* top face, row r-hh        */
    wchar_t side_top;          /* the diagonal joining edge */
    wchar_t front_l, front_r;  /* front face edges          */
    wchar_t front_fill;
    wchar_t side;              /* right face                */
} OblGlyphs;

static const OblGlyphs obl_glyphs_ascii = {
    L'_', L'_', L'/', L'|', L'|', L':', L'.'
};

static const OblGlyphs obl_glyphs_box = {
    L'\u2500', L'\u2500', L'\u2571', L'\u2502', L'\u2502', L' ', L' '
};

static const OblGlyphs obl_glyphs_block = {
    L'\u2588', L'\u2588', L'\u2588', L'\u2593', L'\u2593', L'\u2593', L'\u2592'
};

static const OblGlyphs *obl_glyphs_for(int style, int use_color)
{
    if (use_color) {
        static const OblGlyphs flat = {
            L' ', L' ', L' ', L' ', L' ', L' ', L' '
        };
        return &flat;
    }
    switch (style) {
    case ISO_BOX:    return &obl_glyphs_box;
    case ISO_BLOCK:  return &obl_glyphs_block;
    default:         return &obl_glyphs_ascii;
    }
}

static void oblique_tile(Canvas *cv, const Options *o, const Render *R,
                         const OblGlyphs *g, int b, int r,
                         int is_wall, int role)
{
    int k, hh = (o->wall_height < 1) ? 1 : o->wall_height;
    unsigned char r_top = (unsigned char)(o->shade ? CR_WTOP : CR_WALL);
    unsigned char r_front = (unsigned char)(o->shade ? CR_WLEFT : CR_WALL);
    unsigned char r_side = (unsigned char)(o->shade ? CR_WRIGHT : CR_WALL);
    wchar_t fch;

    if (!is_wall && o->floor) {
        if (solid_ok(R)) {
            fch = L' ';
        } else {
            /* Visible corridor glyphs; the sheared grid has no room
             * for an outline the way the iso diamonds do. */
            switch (role) {
            case CR_HEAD:     fch = L'@'; break;
            case CR_TRAIL1: case CR_TRAIL2: fch = L':'; break;
            case CR_UNVIS:    fch = L','; break;
            case CR_ENDPOINT: fch = L'*'; break;
            default:          fch = (o->iso_style == ISO_BLOCK)
                                    ? L'\u2591' : L'.'; break;
            }
        }
        canvas_put(cv, b,     r, fch, (unsigned char)role);
        canvas_put(cv, b + 1, r, fch, (unsigned char)role);
        return;
    }
    if (!is_wall)
        return;

    for (k = 0; k < hh; k++) {
        int row = r - k;
        canvas_put(cv, b,     row, g->front_l,    r_front);
        canvas_put(cv, b + 1, row, g->front_fill, r_front);
        canvas_put(cv, b + 2, row, g->side,       r_side);
    }
    /* Top face: raised by hh, offset one column right for depth, and
     * joined to the front face with a diagonal. */
    canvas_put(cv, b,     r - hh, g->side_top, r_top);
    canvas_put(cv, b + 1, r - hh, g->top_l,    r_top);
    canvas_put(cv, b + 2, r - hh, g->top_r,    r_top);
}

void oblique_size(const Options *o, int rows, int cols, int *w, int *h)
{
    int tw = 2 * cols + 1, th = 2 * rows + 1;
    int RW = (o->rotate & 1) ? th : tw;
    int RH = (o->rotate & 1) ? tw : th;
    int hh = o->wall_height;

    if (hh < 1)
        hh = 1;
    *w = 2 * RW + RH + 2;               /* 2 cols per tile + depth skew  */
    *h = RH + hh;
}

void oblique_build(Canvas *cv, const Maze *m, const Options *o,
                   const Render *R)
{
    Tiles t;
    const OblGlyphs *g;
    int tw, th, RW, RH, ry, rx, x, y;
    int col_off = 1, row_off;

    if (tiles_build(m, &t) != 0)
        return;
    tw = t.w;
    th = t.h;
    row_off = (o->wall_height < 1) ? 1 : o->wall_height;

    RW = (o->rotate & 1) ? th : tw;
    RH = (o->rotate & 1) ? tw : th;

    g = obl_glyphs_for(o->iso_style, solid_ok(R));

    /* Back to front: rows first (depth), then columns. */
    for (ry = 0; ry < RH; ry++) {
        for (rx = 0; rx < RW; rx++) {
            size_t i;
            int b, r;

            grid_unrotate(o->rotate, tw, th, rx, ry, &x, &y);
            i = (size_t)y * (size_t)tw + (size_t)x;

            b = 2 * rx + ry + col_off;
            r = ry + row_off;
            oblique_tile(cv, o, R, g, b, r, t.wall[i], t.role[i]);
        }
    }

    tiles_free(&t);
}
