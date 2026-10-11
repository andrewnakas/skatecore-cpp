# Port plan: skate-3-rust-engine runtime -> C++ (`src/s3/`)

Source size: skate-core ~95k lines, skate-game simulation (physics, graph_host, graph_runtime, camera,
grind_world, input, skater_animation, skate_world) ~60k, skate-data loaders ~11k.

## Stages
- **R. Reference** — `~/Documents/haloskate-ref/simref`: copy skate-core + skate-data + the simulation modules
  of skate-game into a Bevy-free crate (replace bevy logging/prelude), add `trace` binary:
  load core.pack + map, run N ticks of a scripted input file, print per-tick board/skater state.
- **D. Data** — SKPK1 VFS, JSON (small parser), skate-data: collections (VLT), physics skeletons, ABIN banks,
  animation frames/metadata, state graphs, gesture patterns (.pat), input config, .skate maps + collision.
- **C. Core math/physics** — skate-core math, rigid bodies, board runtime/solver, world contact, collision,
  construction, skeleton bodies.
- **A. Animation + graphs** — skate-core animation/graph, skate-game graph_host/graph_runtime/skater_animation.
- **P. Player** — skate-core input/riding/air/player (incl. offboard), skate-game physics/* (frame advance,
  phases, grinds, wipeouts, respawn, handplants, climbing).
- **K. Camera** — skate-core camera + skate-game camera.
- **W. Web demo** — load pack+map in the browser, render map mesh, skinned skater.glb driven by the C++ pose,
  Skate camera, clean-audio, HUD; KenneyPark default, map picker for the cleanroom maps.

## Parity
Trace columns per tick: board body positions/orientations/velocities, skater state ids, graph node ids.
Target: identical state ids; positions within 1e-4 m for 600 ticks of each scripted run (push, carve,
ollie, kickflip, grind, manual, bail).
