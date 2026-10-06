/*
 * options.c - command line parsing, help text and save/load files.
 */

#include "cmaze.h"

#include <getopt.h>

enum {
    OPT_STYLE = 1000,
    OPT_THEME,
    OPT_SIZE,
    OPT_STRATEGY
};

static const struct option longopts[] = {
    { "live",        no_argument,       0, 'l' },
    { "infinite",    no_argument,       0, 'i' },
    { "time",        required_argument, 0, 't' },
    { "wait",        required_argument, 0, 'w' },
    { "screensaver", no_argument,       0, 'S' },
    { "type",        required_argument, 0, 'T' },
    { "seed",        required_argument, 0, 's' },
    { "message",     required_argument, 0, 'm' },
    { "colors",      required_argument, 0, 'c' },
    { "wall-width",  required_argument, 0, 'W' },
    { "corridor",    required_argument, 0, 'C' },
    { "braid",       required_argument, 0, 'b' },
    { "endpoints",   no_argument,       0, 'e' },
    { "print",       no_argument,       0, 'p' },
    { "save",        required_argument, 0, 'f' },
    { "load",        required_argument, 0, 'L' },
    { "version",     no_argument,       0, 'v' },
    { "help",        no_argument,       0, 'h' },
    { "style",       required_argument, 0, OPT_STYLE },
    { "theme",       required_argument, 0, OPT_THEME },
    { "size",        required_argument, 0, OPT_SIZE },
    { "strategy",    required_argument, 0, OPT_STRATEGY },
    { 0, 0, 0, 0 }
};

void options_defaults(Options *o)
{
    memset(o, 0, sizeof *o);
    o->time         = 0.01;
    o->wait         = 1.0;
    o->type         = "backtracker";
    o->strategy     = "mix";
    o->theme        = "default";
    o->wall_width   = 1;
    o->corridor     = 1;
    o->size_w       = -1;
    o->size_h       = -1;
    o->style        = STYLE_AUTO;
}

void options_print_version(void)
{
    printf("cmaze %s\n", CMAZE_VERSION);
}

void options_print_help(void)
{
    printf(
"cmaze %s - a terminal maze generator\n"
"\n"
"usage: cmaze [options]\n"
"\n"
"modes\n"
"  -l, --live              animate the maze being carved\n"
"  -i, --infinite          keep generating new mazes forever\n"
"  -S, --screensaver       live + infinite, quits on any keypress\n"
"  -p, --print             print the finished maze and exit (no curses)\n"
"\n"
"timing\n"
"  -t, --time=TIME         seconds between animation steps (default 0.01)\n"
"  -w, --wait=TIME         seconds to pause between mazes (default 1.0)\n"
"  -s, --seed=INT          seed, so the same maze comes back\n"
"\n"
"appearance\n"
"  -T, --type=TYPE         maze algorithm (default backtracker)\n"
"      --style=STYLE       texture, ascii, unicode or block (default texture)\n"
"      --theme=THEME       default, forest, ocean, amber or mono\n"
"  -c, --colors=LIST       comma list: " CMAZE_COLOR_ROLES "\n"
"  -W, --wall-width=INT    wall thickness in characters (default 1)\n"
"  -C, --corridor=INT      corridor width in characters (default 1)\n"
"      --size=WxH          fixed maze size in cells\n"
"  -m, --message=STR       show a message below the maze, like cbonsai\n"
"  -e, --endpoints         mark an entrance and exit\n"
"\n"
"tweaks\n"
"  -b, --braid=INT         percentage of dead ends to remove (0-100)\n"
"      --strategy=STRAT    growing tree: newest, oldest, random or mix\n"
"\n"
"files\n"
"  -f, --save=FILE         save the seed and settings to FILE\n"
"  -L, --load=FILE         load a saved maze configuration\n"
"\n"
"  -v, --version           print the version and exit\n"
"  -h, --help              print this help and exit\n"
"\n"
"maze types\n"
"  backtracker prim kruskal wilson aldous-broder eller growing-tree\n"
"  division sidewinder binary-tree hunt-and-kill random\n"
"\n"
"keys\n"
"  q, ESC, Ctrl-C          quit (any key quits in screensaver mode)\n"
"\n"
"examples\n"
"  cmaze -l                        watch a maze carve itself\n"
"  cmaze -l -T wilson --theme=ocean\n"
"  cmaze -l -S                     screensaver\n"
"  cmaze -p --size=40x20 > maze.txt\n"
"  cmaze -l -s 42 -b 40 -e -m \"hello\"\n",
        CMAZE_VERSION);
}

/* ------------------------------------------------------------------ */

static int bad(const char *fmt, const char *arg)
{
    fprintf(stderr, "cmaze: ");
    fprintf(stderr, fmt, arg ? arg : "");
    fprintf(stderr, "\n");
    return -1;
}

static int parse_int(const char *what, const char *arg, long *out)
{
    char *end;
    long v = strtol(arg, &end, 0);

    if (end == arg || *end != '\0')
        return bad("%s: not a number", what);
    *out = v;
    return 0;
}

int options_parse(Options *o, int argc, char **argv)
{
    int ch;

    opterr = 0;
    while ((ch = getopt_long(argc, argv, "lit:w:ST:s:m:c:W:C:b:epf:L:vh",
                             longopts, NULL)) != -1) {
        switch (ch) {
        case 'l': o->live = 1; break;
        case 'i': o->infinite = 1; break;
        case 'S': o->screensaver = 1; o->live = 1; o->infinite = 1; break;
        case 'p': o->print = 1; break;
        case 'e': o->endpoints = 1; o->explicit_mask |= X_ENDPOINTS; break;
        case 'v': o->version = 1; break;
        case 'h': o->help = 1; break;

        case 't': {
            double v = atof(optarg);
            if (v < 0) return bad("time must not be negative: %s", optarg);
            o->time = v;
            o->explicit_mask |= X_TIME;
            break;
        }
        case 'w': {
            double v = atof(optarg);
            if (v < 0) return bad("wait must not be negative: %s", optarg);
            o->wait = v;
            o->explicit_mask |= X_WAIT;
            break;
        }
        case 'T':
            o->type = optarg;
            o->explicit_mask |= X_TYPE;
            break;
        case 's': {
            char *end;
            unsigned long long v = strtoull(optarg, &end, 0);
            if (end == optarg || *end != '\0')
                return bad("bad seed: %s", optarg);
            o->seed = v;
            o->seed_set = 1;
            o->explicit_mask |= X_SEED;
            break;
        }
        case 'm':
            o->message = optarg;
            o->explicit_mask |= X_MSG;
            break;
        case 'c':
            o->colors = optarg;
            o->explicit_mask |= X_COLORS;
            break;
        case 'W': {
            long v;
            if (parse_int("wall width", optarg, &v)) return -1;
            if (v < 1) return bad("wall width must be >= 1: %s", optarg);
            o->wall_width = (int)v;
            o->explicit_mask |= X_WW;
            break;
        }
        case 'C': {
            long v;
            if (parse_int("corridor width", optarg, &v)) return -1;
            if (v < 1) return bad("corridor width must be >= 1: %s", optarg);
            o->corridor = (int)v;
            o->explicit_mask |= X_CORR;
            break;
        }
        case 'b': {
            long v;
            if (parse_int("braid", optarg, &v)) return -1;
            if (v < 0 || v > 100) return bad("braid must be 0-100: %s", optarg);
            o->braid = (int)v;
            o->explicit_mask |= X_BRAID;
            break;
        }
        case 'f': o->save = optarg; break;
        case 'L': o->load = optarg; break;

        case OPT_STYLE:
            if (style_parse(optarg) < 0)
                return bad("unknown style: %s (try texture, ascii, unicode, block)",
                           optarg);
            o->style_arg = optarg;
            o->explicit_mask |= X_STYLE;
            break;
        case OPT_THEME: {
            short tmp[CR_COUNT];
            if (theme_lookup(optarg, tmp) != 0)
                return bad("unknown theme: %s (try default, forest, ocean,"
                           " amber, mono)", optarg);
            o->theme = optarg;
            o->explicit_mask |= X_THEME;
            break;
        }
        case OPT_SIZE: {
            int w = 0, h = 0;
            int n = sscanf(optarg, "%dx%d", &w, &h);
            if (n < 1 || w < 1 || h < 0)
                return bad("size must look like 40x20: %s", optarg);
            o->size_w = w;
            o->size_h = (h > 0) ? h : -1;
            o->explicit_mask |= X_SIZE;
            break;
        }
        case OPT_STRATEGY:
            if (strcmp(optarg, "newest") && strcmp(optarg, "oldest") &&
                strcmp(optarg, "random") && strcmp(optarg, "mix"))
                return bad("unknown strategy: %s (newest, oldest, random, mix)",
                           optarg);
            o->strategy = optarg;
            o->explicit_mask |= X_STRAT;
            break;

        case '?':
        default:
            if (optopt)
                fprintf(stderr, "cmaze: invalid option -- '%c'\n", optopt);
            else if (optind > 0 && argv[optind - 1])
                fprintf(stderr, "cmaze: invalid option -- '%s'\n",
                        argv[optind - 1]);
            else
                fprintf(stderr, "cmaze: invalid option\n");
            fprintf(stderr, "try 'cmaze --help'\n");
            return -1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "cmaze: unexpected argument -- '%s'\n", argv[optind]);
        fprintf(stderr, "try 'cmaze --help'\n");
        return -1;
    }

    if (o->load) {
        if (options_load(o, o->load) != 0) {
            fprintf(stderr, "cmaze: cannot read %s\n", o->load);
            return -1;
        }
    }

    if (!algo_find(o->type) && strcmp(o->type, "random") != 0) {
        fprintf(stderr, "cmaze: unknown type '%s'\n", o->type);
        fprintf(stderr, "try one of: backtracker prim kruskal wilson "
                        "aldous-broder eller growing-tree division "
                        "sidewinder binary-tree hunt-and-kill random\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */

/* The file buffer is reused, so any value we keep must be copied. */
static const char *keep(const char *s)
{
    char *d = strdup(s);
    return d ? d : s;
}

static void load_kv(Options *o, const char *key, const char *val)
{
    int ex = o->explicit_mask;

    if (!strcmp(key, "seed")) {
        if (!(ex & X_SEED)) {
            o->seed = strtoull(val, NULL, 0);
            o->seed_set = 1;
        }
    } else if (!strcmp(key, "type")) {
        if (!(ex & X_TYPE) && (algo_find(val) || !strcmp(val, "random")))
            o->type = keep(val);
    } else if (!strcmp(key, "time")) {
        if (!(ex & X_TIME)) o->time = atof(val);
    } else if (!strcmp(key, "wait")) {
        if (!(ex & X_WAIT)) o->wait = atof(val);
    } else if (!strcmp(key, "style")) {
        if (!(ex & X_STYLE) && style_parse(val) >= 0)
            o->style_arg = keep(val);
    } else if (!strcmp(key, "theme")) {
        if (!(ex & X_THEME)) {
            short tmp[CR_COUNT];
            if (theme_lookup(val, tmp) == 0)
                o->theme = keep(val);
        }
    } else if (!strcmp(key, "wall-width")) {
        if (!(ex & X_WW)) {
            long v = strtol(val, NULL, 0);
            if (v >= 1) o->wall_width = (int)v;
        }
    } else if (!strcmp(key, "corridor")) {
        if (!(ex & X_CORR)) {
            long v = strtol(val, NULL, 0);
            if (v >= 1) o->corridor = (int)v;
        }
    } else if (!strcmp(key, "braid")) {
        if (!(ex & X_BRAID)) {
            long v = strtol(val, NULL, 0);
            if (v >= 0 && v <= 100) o->braid = (int)v;
        }
    } else if (!strcmp(key, "strategy")) {
        if (!(ex & X_STRAT) &&
            (!strcmp(val, "newest") || !strcmp(val, "oldest") ||
             !strcmp(val, "random") || !strcmp(val, "mix")))
            o->strategy = keep(val);
    } else if (!strcmp(key, "size")) {
        if (!(ex & X_SIZE)) {
            int w = 0, h = 0;
            if (sscanf(val, "%dx%d", &w, &h) >= 1 && w >= 1) {
                o->size_w = w;
                o->size_h = (h > 0) ? h : -1;
            }
        }
    } else if (!strcmp(key, "colors")) {
        if (!(ex & X_COLORS) && *val) o->colors = keep(val);
    } else if (!strcmp(key, "message")) {
        if (!(ex & X_MSG) && *val) o->message = keep(val);
    } else if (!strcmp(key, "endpoints")) {
        if (!(ex & X_ENDPOINTS)) o->endpoints = atoi(val);
    }
}

int options_load(Options *o, const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[1024];

    if (!fp)
        return -1;

    while (fgets(line, sizeof line, fp)) {
        char *nl = strchr(line, '\n');
        char *eq;
        if (nl)
            *nl = '\0';
        eq = strchr(line, '=');
        if (!eq)
            continue;                   /* header / comment line */
        *eq = '\0';
        load_kv(o, line, eq + 1);
    }
    fclose(fp);
    return 0;
}

int options_save(const Options *o, const char *path, unsigned long long seed)
{
    FILE *fp = fopen(path, "w");

    if (!fp)
        return -1;

    fprintf(fp, "cmaze %s seed and settings\n", CMAZE_VERSION);
    fprintf(fp, "seed=%llu\n", seed);
    fprintf(fp, "type=%s\n", o->type ? o->type : "backtracker");
    fprintf(fp, "time=%.4f\n", o->time);
    fprintf(fp, "wait=%.4f\n", o->wait);
    fprintf(fp, "style=%s\n", o->style_arg ? o->style_arg : "auto");
    fprintf(fp, "theme=%s\n", o->theme ? o->theme : "default");
    fprintf(fp, "wall-width=%d\n", o->wall_width);
    fprintf(fp, "corridor=%d\n", o->corridor);
    fprintf(fp, "braid=%d\n", o->braid);
    fprintf(fp, "strategy=%s\n", o->strategy ? o->strategy : "mix");
    fprintf(fp, "size=%dx%d\n", o->size_w > 0 ? o->size_w : 0,
            o->size_h > 0 ? o->size_h : 0);
    fprintf(fp, "colors=%s\n", o->colors ? o->colors : "");
    fprintf(fp, "message=%s\n", o->message ? o->message : "");
    fprintf(fp, "endpoints=%d\n", o->endpoints);

    fclose(fp);
    return 0;
}
