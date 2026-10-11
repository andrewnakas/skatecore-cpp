# skatecore — full Skate 3 runtime in C++ (overnight loop)

You are running **unattended**. Never stop to ask: write questions in `STATUS.md` ("Questions"), pick the
sensible default, continue. Log decisions and progress in `STATUS.md`. Plan: `docs/PORT_PLAN.md`.

## Goal
`skatecore` must skate and feel **exactly** like the user's Skate 3 cleanroom web build
(https://andrewnakas.github.io/skate3-cleanroom/, engine `skate-3-rust-engine@cleanroom-web`), as a C++17
library with a C API, plus a web demo with the Skate 3 look (skinned skater, Skate 3 camera, sounds, HUD).

## Pre-answered (do not ask)
- Data: the runtime reads the **cleanroom pack** (animations `data/anim/*.abin`, state graphs `data/state`,
  VLT `skater-collections.json`, `physics-skeletons.json`, joystick `.pat`, camera/config, skater.glb,
  clean-audio). The user approved this (their cleanroom policy keeps these as facts).
  **Never commit pack data to this repo.** The web demo fetches `core.pack` + a map from
  `https://andrewnakas.github.io/skate3-cleanroom/` at run time (same origin). Native tests read
  `~/Documents/haloskate-ref/cleanpack/core.pack` and `KenneyPark.skate` (skip if missing).
- Reference source (read only): `~/Documents/haloskate-ref/rust/crates/*` (skate-core, skate-game, skate-data).
- Port faithfully: same module/function names (namespace `s3::<module>`), same constants, keep the original
  address comments, f32 semantics, `-ffp-contract=off`, Rust `mul_add` -> `std::fma`, wrapping ints as
  unsigned. Port every Rust `#[test]` of a module next to it (`tests/s3/<module>_test.cpp`).
- Keep the existing simple engine (`src/*.cpp`, namespace `skate`) working as "lite" until the full runtime
  replaces it behind `include/skate_api.h`.
- Golden reference: `~/Documents/haloskate-ref/simref/` is a Bevy-free Rust extraction of the simulation that
  dumps per-tick traces (never published). C++ must match it tick for tick (tolerance in docs/PORT_PLAN.md).

## Machine budget (hard)
8 GB RAM, ~5 GB free disk: no Bevy builds; cargo only for the Bevy-free reference (`-j4`, one target dir,
`cargo clean` if disk < 2 GB). One headless browser at a time. `make -j4`.

## How to work
- Use subagents to port independent modules in parallel (max 3 at once, disjoint directories); you integrate,
  build, run tests, commit.
- Commit often (git user andre / treesixtyweather@gmail.com), push to main; end messages with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Pages redeploys on push; keep the live demo working.
- STATUS.md: Live URL, Done (dated), In progress, Trace parity table, Questions, For the user.
