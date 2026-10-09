# cmaze

A terminal maze generator in C + ncurses, meant to be watched rather than played.
Carve a maze one cell at a time, in color, at whatever speed you like — or print
one to a file. Walls default to a seeded ASCII texture, and the same maze can be
drawn top-down, isometric, or oblique:

```
                     __
                   ______
                 ____\/____
               ____\.||:/____
             ____\.|.__:|:/____
           ____\.|.______:|:/____
         ____\.|.____\/____:|:/____
       ____\.|.____\.||:/____:|:/____
     ____\.|.____\.|.__:|:/____:|:/____
   ______|. /____|.____\ :|____\ :|:/____
 ____\/____ |:/______\.| ____\.| __:|:/____
/____||:/__\ :|____\.|.______|. /____:|____\
|:/____:|:.| ____\.|.____\/____ |:/______\.|
|:|:/____:. /__\.|.____\.||____\ :|____\.|.|
  |:|:/____ |:.|.____\.|.____\.| ____\.|.|
    |:|:/____:.____\.|.____\.|.____\.|.|
      |:|:/______\.|.____\.|.____\.|.|
        |:|:/____|. /__\.|.____\.|.|
          |:|:/____ |:.|.____\.|.|
            |:|:/____:.____\.|.|
              |:|:/______\.|.|
                |:|:/__\.|.|
                  |:|:.|.|
                    |:.|
```

## Features

- **11 maze algorithms**, plus `random` to pick one per maze
- **Three views** — top-down (default), isometric (`-V iso`) and oblique
  (`-V oblique`); the same seed draws the same maze in every view
- **Textured walls (default)** — walls composed of seeded ASCII symbols,
  grouped by stroke direction, so the maze reads clearly but looks hand-typed
- **Three more wall styles** — plain ASCII, Unicode box-drawing, or solid blocks
  (`--style=ascii|unicode|block`)
- **Live mode** — watch the maze carve itself, cell by cell
- **Infinite / screensaver mode** — endless mazes, the screensaver quits on any key
- **Themes and custom colors** — `default`, `forest`, `ocean`, `amber`, `mono`, or your own palette
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
      --style=STYLE       texture, ascii, unicode or block (default texture)
      --theme=THEME       default, forest, ocean, amber or mono
  -c, --colors=LIST       comma list: wall,path,head,trail1,trail2,unvisited,endpoint
  -W, --wall-width=INT    wall thickness in characters (default 1)
  -C, --corridor=INT      corridor width in characters (default 2)
      --fill=PCT          share of the terminal the maze may use (default 67)
      --size=WxH          fixed maze size in cells
  -m, --message=STR       show a message below the maze, like cbonsai
  -e, --endpoints         mark an entrance and exit

tweaks
  -b, --braid=INT         percentage of dead ends to remove (0-100)
      --strategy=STRAT    growing tree: newest, oldest, random or mix

views
  -V, --view=VIEW         top (default), iso or oblique
  -H, --wall-height=INT   wall height in rows for iso/oblique (default 2)
  -R, --rotate=INT        which corner faces you: 0, 1, 2 or 3 (default 0)
      --shade=BOOL        shade wall faces light/medium/dark (default on)
      --no-floor          draw only walls, no floor tiles
      --iso-style=STYLE   iso glyphs: ascii, box or block (default auto)

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
cmaze -V iso -l -H 3              # isometric view with taller walls
cmaze -p -V iso -R 1 -s 42        # same maze as above, rotated, printed
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

## Wall styles

| Style | Look |
| --- | --- |
| `texture` (default) | random ASCII symbols per wall cell, chosen from a hash of position + seed; horizontal runs use `-=~_:.`, vertical runs use `|!:'`, corners and tees use `+#*x@%`, crossings use `+#x*` |
| `ascii` | plain `\|`, `-`, `+` |
| `unicode` | box-drawing strokes `─│┌┐└┘├┤┬┴┼` |
| `block` | solid `█` walls |

```sh
cmaze -p --style=unicode    # the classic box-drawing maze
cmaze -l --style=block      # chunky block walls
```

The texture is deterministic: the same `--seed` always produces the same
glyphs, and each maze in infinite mode gets its own texture.

## Views

`-V` picks how the maze is drawn. Nothing about generation changes — the
same seed always produces the same maze, whatever the view.

| View | Look |
| --- | --- |
| `top` (default) | the classic flat plan |
| `iso` | 2:1 dimetric isometric; walls become blocks with a top and two side faces |
| `oblique` | front-facing with a diagonal depth offset; a cheaper, simpler 3D look |

The isometric view is a parallel projection (no perspective), approximated
with the classic 2:1 pixel-art slope so it lands cleanly on a character grid.
Tiles are drawn back to front, so walls in front hide what is behind them.
Mazes are auto-sized to fit the terminal and centered; live carving looks
like corridors being dug out of a solid block.

```sh
cmaze -V iso -l              # watch it carve in 3D
cmaze -V iso -H 3 -R 1       # taller walls, rotated 90 degrees
cmaze -V oblique -l          # simpler oblique blocks
cmaze -p -V iso > maze.txt   # print the isometric maze (plain ASCII)
```

`--iso-style` chooses the glyph set: `ascii` (`/ \ _ |`), `box` (box drawing
and diagonals) or `block` (`█▓▒░`). The curses view defaults to `box` on a
UTF-8 terminal, printed output always defaults to `ascii`. Wall faces are
shaded via the theme's wall colour (top lightest, left medium, right
darkest); `--no-shade` turns that off, and 8/16-color terminals fall back
to denser glyphs instead of colors.

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
view=iso
wall-height=3
rotate=1
```

## Project layout

```
cmaze.h        shared types: Maze, Rng, Algo, Options, Render
main.c         CLI parsing glue, curses setup, main loop, signals
options.c      getopt_long, help/version, save/load file format
grid.c         grid + wall bookkeeping, braiding, endpoints
algo.c         algorithm registry
canvas.c       off-screen character/colour buffer, curses + stdout output
project.c      maze -> tile grid expansion, rotation math
render.c       top-down canvas build, message box, print mode, layout
render_iso.c   isometric + oblique renderers (projection, faces, shading)
theme.c        color names, xterm-256 mapping, themes
rng.c          splitmix64/xoshiro-style PRNG
algos/         the 11 algorithms, one file each
```

## See also

- `cmaze(6)` — the man page (`make install` installs it)
- `cmaze-pitch.md` — the original design pitch
