# cmaze

A terminal maze generator in C + ncurses, meant to be watched rather than played.
Carve a maze one cell at a time, in color, at whatever speed you like — or print
one to a file.

```
┌───┬─────┬─────┐
│   │     │     │
│ │ │ ┌─┐ │ ┌── │
│ │ │ │ │   │   │
│ │ │ │ └───┤ │ │
│ │ │ │     │ │ │
│ └─┘ └──── │ │ │
│           │ │ │
└───────────┴─┴─┘
```

## Features

- **11 maze algorithms**, plus `random` to pick one per maze
- **Live mode** — watch the maze carve itself, cell by cell
- **Infinite / screensaver mode** — endless mazes, the screensaver quits on any key
- **Themes and custom colors** — `default`, `forest`, `ocean`, `amber`, `mono`, or your own palette
- **Box-drawing or ASCII output** — Unicode connectivity glyphs, block walls, or plain `#`
- **Braiding** — remove dead ends for a loopy maze
- **Endpoints** — carve an entrance and exit
- **Messages** — a framed message below the maze, like cbonsai
- **Deterministic seeds** — the same seed always draws the same maze
- **Save / load** — persist a seed plus its settings to a file
- **Resize-safe** — handles `SIGWINCH`; the maze re-lays out to the new size
- **Wide characters** — real UTF-8 via wide ncurses (`mvadd_wch`)

## Building

Requires a C99 compiler and ncurses. No other dependencies.

```sh
make            # builds ./cmaze
make install    # installs binary + man page to $(PREFIX), default /usr/local
make uninstall
make clean
```

On macOS the Xcode command line tools provide ncurses. On Debian/Ubuntu:

```sh
sudo apt install libncurses-dev
```

## Usage

```
modes
  -l, --live              animate the maze being carved
  -i, --infinite          keep generating new mazes forever
  -S, --screensaver       live + infinite, quits on any keypress
  -p, --print             print the finished maze and exit (no curses)

timing
  -t, --time=TIME         seconds between animation steps (default 0.01)
  -w, --wait=TIME         seconds to pause between mazes (default 1.0)
  -s, --seed=INT          seed, so the same maze comes back

appearance
  -T, --type=TYPE         maze algorithm (default backtracker)
      --style=STYLE       ascii, unicode or block (default auto)
      --theme=THEME       default, forest, ocean, amber or mono
  -c, --colors=LIST       comma list: wall,path,head,trail1,trail2,unvisited,endpoint
  -W, --wall-width=INT    wall thickness in characters (default 1)
  -C, --corridor=INT      corridor width in characters (default 1)
      --size=WxH          fixed maze size in cells
  -m, --message=STR       show a message below the maze, like cbonsai
  -e, --endpoints         mark an entrance and exit

tweaks
  -b, --braid=INT         percentage of dead ends to remove (0-100)
      --strategy=STRAT    growing tree: newest, oldest, random or mix

files
  -f, --save=FILE         save the seed and settings to FILE
  -L, --load=FILE         load a saved maze configuration

  -v, --version           print the version and exit
  -h, --help              print this help and exit
```

### Keys

| Key | Action |
| --- | --- |
| `q`, `ESC`, `Ctrl-C` | quit (any key quits in screensaver mode) |

The terminal is restored cleanly on exit, whatever way you quit.

### Examples

```sh
cmaze -l                          # watch a maze carve itself
cmaze -l -T wilson --theme=ocean  # Wilson's algorithm, ocean colors
cmaze -l -S                       # screensaver
cmaze -l -i -w 2                  # new maze every 2 seconds, forever
cmaze -p --size=40x20 > maze.txt  # print a maze to a file
cmaze -l -s 42 -b 40 -e -m "hello" # seeded, braided, with endpoints + message
```

## Maze types

| Type | Notes |
| --- | --- |
| `backtracker` | depth-first; long windy corridors (default) |
| `prim` | randomized Prim; short dead ends, rustic look |
| `kruskal` | randomized Kruskal; shuffle-and-merge |
| `wilson` | Wilson's uniform algorithm; unbiased, pretty |
| `aldous-broder` | pure random walk; uniform but slow to converge |
| `eller` | row-by-row, low memory |
| `growing-tree` | blend of DFS and BFS via `--strategy` |
| `division` | recursive division; long straight walls |
| `sidewinder` | run-and-choose, biased corridors |
| `binary-tree` | fastest, strong northeast bias |
| `hunt-and-kill` | walk until stuck, then hunt for unvisited cells |
| `random` | pick a different algorithm each maze |

## Colors

A theme sets the whole palette; `-c` overrides individual roles:

```sh
cmaze -l -c "#ff0000,#00ff00,#ffffff"   # hex colors
cmaze -l -c "196,46,231"                # xterm-256 indices
```

Roles, in order: `wall,path,head,trail1,trail2,unvisited,endpoint`.
`head` is the carving front, `trail`/`trail2` are the receding glow behind it.
Colors are remapped to the nearest available on 8/16-color terminals.

## Save files

`-f` writes the current seed and settings as `key=value` lines; `-L` reads them
back:

```
seed=42
type=backtracker
size=40x20
theme=default
braid=40
endpoints=1
```

## Project layout

```
cmaze.h        shared types: Maze, Rng, Algo, Options, Render
main.c         CLI parsing glue, curses setup, main loop, signals
options.c      getopt_long, help/version, save/load file format
grid.c         grid + wall bookkeeping, braiding, endpoints
algo.c         algorithm registry
render.c       canvas build, box-drawing, message box, print mode
theme.c        color names, xterm-256 mapping, themes
rng.c          splitmix64/xoshiro-style PRNG
algos/         the 11 algorithms, one file each
```

## See also

- `cmaze(6)` — the man page (`make install` installs it)
- `cmaze-pitch.md` — the original design pitch
