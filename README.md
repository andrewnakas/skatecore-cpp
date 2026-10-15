# skatecore

**Skate 3's skateboarding runtime in C++17**, and a web demo that runs it.

▶ **Play: https://andrewnakas.github.io/skatecore-cpp/**

## The demo

The page runs the full C++ port of the Skate 3 cleanroom engine (`src/s3`: skate-core, skate-data and the
skate-game simulation, tick-for-tick bit-identical to the Rust reference, see [docs/PARITY.md](docs/PARITY.md))
compiled to WebAssembly. Every fixed tick (60 Hz) the simulation reads the raw XInput-style pad packet and runs
the game's input, physics, animation graphs and gameplay camera; the page draws the skinned skater from the
simulation's render pose and looks through the game camera, interpolated between ticks like the game does. A
small HUD shows the trick names and scores, and the clean-room sound set plays the rolling, push, pop, landing,
grind, powerslide and bail sounds.

**Pack data is never committed or bundled.** The page downloads `core.pack` (animations, state graphs, tuning,
the skater model, sounds) and the chosen `.skate` map at run time from the
[Skate 3 cleanroom site](https://andrewnakas.github.io/skate3-cleanroom/), using its `pack.json` (the
`?base=URL/` parameter points at another copy, e.g. a local mirror). The **Map** menu at the top right lists
every map in `pack.json` (`?map=Name` opens one directly); the page opens on BlackBoxPark.

Controls (the same layout as the cleanroom web build; a gamepad works too after pressing a button):

| Pad | Keyboard |
| --- | --- |
| A (push) | Space |
| Left stick (steer, lean, crouch) | W A S D |
| Right stick (flicks: ↓ then ↑ ollies) | Arrow keys |
| B / X / Y | E / Left Shift / F |
| LT / RT (grabs) | Z / C |
| LB / RB | Q / R |
| Start / Back | Enter (or Escape) / Backspace |
| D-pad, stick clicks | 1–4, V / B |
| debug orbit camera | O (H hides the help) |

### Differences from the cleanroom web build

- **Same simulation, different renderer.** The physics, animation graphs, tricks, scoring and camera are the C++
  port, tick-for-tick identical to the Rust reference simulation (`tools/parity.sh`: idle, push, ollie, kickflip,
  carve traces identical in decimal and hex). Drawing is three.js, not Bevy: map materials are approximated with
  standard PBR materials plus the baked lightmaps, lighting follows the custom-map daylight values, and the
  retail sky (SKYB) and retail shader families are not reproduced.
- **Default map.** The cleanroom build opens on StartPark. Here StartPark's authored spawn falls through the floor
  and the skater respawns in a loop; the Rust reference simulation this port matches does exactly the same (byte
  identical 400-tick trace), so the newer cleanroom engine presumably has a fix this snapshot lacks. The page
  therefore opens on BlackBoxPark (MegaPark's moved spawn faces a dark corner). Every other map spawns on the
  ground (MegaPark's spawn is moved onto the nearest walkable ground, as the cleanroom game does).
- **Not included:** multiplayer, the Freestyle MX bike (F9), the FPS counter, and anything else outside the
  single-player skate simulation.

The small earlier "lite" engine (below) still has its own demo at
[/lite/](https://andrewnakas.github.io/skatecore-cpp/lite/).

## The lite engine

A small, engine-free first port of `skate-core` from [skate-3-rust-engine](https://github.com/SK8-ENGINE/skate-3-rust-engine)
(the Rust/Bevy Skate 3 reverse-engineering project) to C++ with a plain C API, so any engine can host it:
it runs inside the Halo CE web port in [HaloSkate 3](https://github.com/andrewnakas/haloskate3wasm), and here on a
three.js skatepark (the lite demo). No dependencies; the host only supplies a raycast.

### What's in it

- **Flick-it** gesture recognizer, ported line for line (`src/gesture.cpp`), with 16 authored stick patterns
  (the 360 flip uses skate-core's recovered test vector).
- **Trick mapping**: the Square group of the original gesture→trick table, mirror pairs, dark-catch and hold
  successors (`src/tricks.cpp`).
- **Grind rails derived from any triangle soup** (`src/derive.cpp`, port of `grind_world/derive.rs`): ledges,
  handrails, hubbas, flat bars are found automatically.
- **Skater controller** (`src/skater.cpp`): kick pushes, carving, ollie/nollie popped by the flick, flips with
  landing judgement, spins, grabs, manuals with balance, powerslides, grinds, bails and respawn, combo scoring.
  Where Skate 3 is driven by animation and tuning data (not shipped), a procedural layer stands in —
  see [PORTING.md](PORTING.md).

### Use it

```c
#include "skate_api.h"
skate_world *w = skate_world_new(my_raycast, my_user);       // your collision
skate_world_derive_rails(w, vertices, vcount, indices, tcount); // optional: rails from your level
skate_skater *s = skate_skater_new();
skate_skater_enter(s, spawn, zero, heading);
for (;;) { skate_input in = read_pad(); skate_skater_step(s, w, &in); /* 60 Hz */ skate_pose p; skate_skater_get_pose(s, &p); draw(&p); }
```
Space: metres, +Y up, board nose +Z at heading 0. Step: 1/60 s. Events (tricks, landings, bails, grinds,
banked combos) come from `skate_skater_poll_event`.

## Build

```sh
make test                  # lite engine: native tests (recognizer vectors, mapping, rails, controller)
make s3-test               # full runtime: the 1001 ported Rust tests (needs the cleanroom pack)
tools/build_web.sh site    # Emscripten: site/ = full-runtime demo, site/lite/ = lite demo, site/s3/ = redirect
python3 -m http.server -d site 8000
```
`tools/build_web.sh` produces the whole site: the full runtime at the root (`skate3.js` + `skate3.wasm`, about
4.0 MB, 1.4 MB gzipped), the lite demo under `lite/`, and `s3/` (the old address) redirecting to the root. The
full runtime is ~690 translation units, compiled with `make -j$JOBS` (default 4, each job needs about 1 GB) into
`build/wasm_obj` with `-O2 -ffp-contract=off -fexceptions` (Emscripten 6.0.10); a clean build takes about 10
minutes at `-j4`, an incremental one seconds. CI (`.github/workflows/pages.yml`) runs the lite tests, builds both
demos with ccache (`EM_COMPILER_WRAPPER=ccache`, cached between runs) and deploys `site/` to Pages on every push.
`make s3-test` (1001 ported Rust tests) and `tools/parity.sh` need the cleanroom pack locally, so they run on a
developer machine, not in CI.

## Controls (lite demo)

W/Space push · S brake · A D carve/spin · arrows flick (↓ then ↑ ollie, ↓ then ↖/↗ kick/heelflip, ↓ then sweep ←/→
shove-its, ← → hardflip, ↙ ↗ varial, scoop ← ↙ ↓ ↗ 360 flip) · Shift/C manual · Q/E grab · X powerslide · R reset.
Gamepad and touch work too.

## Credits

Skate 3 © Electronic Arts; this is a fan project and ships no Skate 3 assets or data files.
skate-core: SK8-ENGINE / skate-3-rust-engine contributors. Port, controller and demo: andrewnakas.
License: GPL-3.0 (see `LICENSE`), the same as the upstream [skate-3-rust-engine](https://github.com/SK8-ENGINE/skate-3-rust-engine) this is ported from.
