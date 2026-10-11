# skatecore

**Skate 3's skateboarding mechanics as a small, engine-free C++17 library**, and a web demo that runs it.

▶ **Play the core: https://andrewnakas.github.io/skatecore-cpp/**

A port of `skate-core` from [skate-3-rust-engine](https://github.com/SK8-ENGINE/skate-3-rust-engine)
(the Rust/Bevy Skate 3 reverse-engineering project) to C++ with a plain C API, so any engine can host it:
it runs inside the Halo CE web port in [HaloSkate 3](https://github.com/andrewnakas/haloskate3wasm), and here on a
three.js skatepark. No dependencies; the host only supplies a raycast.

## What's in it

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

## Use it

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
make test                  # native tests (recognizer vectors, mapping, rails, controller): 84 checks
tools/build_web.sh site    # Emscripten: site/ = the demo (skatecore.wasm ~180 KB)
python3 -m http.server -d site 8000
```
CI runs the tests, builds the demo and deploys it to Pages on every push.

## Controls (demo)

W/Space push · S brake · A D carve/spin · arrows flick (↓ then ↑ ollie, ↓ then ↖/↗ kick/heelflip, ↓ then sweep ←/→
shove-its, ← → hardflip, ↙ ↗ varial, scoop ← ↙ ↓ ↗ 360 flip) · Shift/C manual · Q/E grab · X powerslide · R reset.
Gamepad and touch work too.

## Credits

Skate 3 © Electronic Arts; this is a fan project and ships no Skate 3 assets or data files.
skate-core: SK8-ENGINE / skate-3-rust-engine contributors. Port, controller and demo: andrewnakas.
License: undecided (upstream skate-3-rust-engine has no license file; its README mentions GPL-3.0).
