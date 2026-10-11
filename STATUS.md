# STATUS (skatecore full runtime port)

Live: https://andrewnakas.github.io/skatecore-cpp/ (currently the "lite" engine)

## Done (2026-10-11)
- Lite engine + web demo live. User approved using the cleanroom pack data (loaded at run time, never committed).
- Pack downloaded for native tests: ~/Documents/haloskate-ref/cleanpack/{core.pack,KenneyPark.skate}.

- **Stage R done.** `~/Documents/haloskate-ref/simref` (local only): Bevy-free copy of skate-core + skate-data + the
  skate-game simulation; `trace <root> KenneyPark <script> <ticks>` runs the game's real input path and
  `frame::advance`, deterministic (byte-identical reruns). Traces: idle, push, ollie (TRICK:OLLIE t182, lands t239),
  kickflip (t182/238), carve. CSV: input, phys/motion/action states, clip, 7 board bodies (pos, quat, vel, angvel),
  rider root, contacts, trick/score names, events. Unpacked pack root: ~/Documents/haloskate-ref/cleanpack/root.

- **Stage C done.** skate-core math + all of physics/ in C++ (`src/s3/core`), 209 ported Rust tests pass
  (incl. bit-exact hex vectors), plus partial ports of riding/air/player/animation pieces physics needs.
- **Stage D done.** skate-data loaders (`src/s3/data`): VFS over core.pack, VLT collections, physics skeleton, ABIN +
  VBR, metadata, frames, state graphs, .pat, input config, manifest, .skate maps (zlib/zstd). 59 tests; dumps of all
  3324 clips, poses, KenneyPark/MegaPark/StartPark/BlackBoxPark identical to Rust.

## In progress
- Stage A (animation + state graphs), Stage P1 (input/riding/air/scoring), Stage P2 (player/**).
- Next: Stage P3 (skate-game physics/* frame advance), Stage K (camera), trace harness, Stage W (web demo).

## Trace parity
(none yet)

## Questions
(none)
