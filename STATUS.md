# STATUS (skatecore full runtime port)

Live: https://andrewnakas.github.io/skatecore-cpp/ (full C++ Skate 3 runtime; lite engine at /lite/)

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

- **Stage P1 done.** input/riding/air/scoring (flick-it gestures, intentions, grounded riding, pumping, ground jump,
  known air, trajectories, scoring): 94 tests.
- **Stage P2 done.** player/** (state machine, selector, input phase, wipeout/slide states, offboard, respawn): 265 tests.
- **Stage K done.** skate-core camera + game CameraRuntime (stock shots from the pack): 10 tests.
- **Stage A done.** animation + graph + game graph runtime/hosts/skater animation; stock ActionGraph (234 states) and
  MotionGraph (2088 states) compile and run on real data: 171 tests. Bit-exact glam (NEON order) port.
- Ported tests total: 209 core + 59 data + 94 p1 + 265 p2 + 171 anim + 10 cam = 808, all passing.

## Resumed after a usage-limit pause (2026-10-11)
- Stage P3 (skate-game simulation + s3trace) and Stage W1 (web demo on the real pack) were mid-port when every agent
  stopped. Their partial files are in the working tree, UNCOMMITTED on purpose (src/s3/game/physics*, input*,
  grind_world*, skate_world*, web/s3/, tools/s3trace.cpp, mk/game.mk ...). Resume: re-launch P3 (prompt in
  docs/PORT_PLAN.md stages P/W), have it inventory what exists, finish, then run the trace parity.

- **Stage P3 done (uncommitted).** skate-game simulation in C++ (src/s3/game: physics/**, input/**, grind_world/**,
  scoring_runtime, skate_world, driver) + `s3trace` + tools/trace_diff.py + tools/parity.sh; 193 ported game tests
  (`make s3-game-test`), 6 stale Rust #[ignore] playback tests skipped unless S3_RUN_STALE=1 (docs/PARITY.md).

## In progress (when resumed)
- Stage W1: web demo loading the cleanroom pack + map + skater.glb (under /s3/).

## Trace parity
Stage P3: C++ `build/s3trace` vs simref `trace` is **bit-identical** (decimal and `--hex` CSV, every tick, every column) on
idle/push/ollie/kickflip/carve (KenneyPark), test-world, normal/hardcore difficulty and 22 extra 2400-tick stress traces
(`tools/parity.sh --extra`; covers air, grabs, flips, spins, off-board, bails, grinds, powerslide, respawn). See docs/PARITY.md.

## Questions
(none)

## Done: full runtime live (2026-10-11)
- W2: the web demo runs the C++ runtime (driver::Sim) in WebAssembly: skinned regenerated skater from the sim
  pose, game CameraRuntime, cleanroom controls, clean-audio, HUD, 9-map picker. Pack + maps are fetched at run time
  from the skate3-cleanroom site (never committed). Verified on the live site (docs/shots/live-keys.png:
  BlackBoxPark, push to 30 km/h, Ollie scored). 1001 ported tests pass; traces bit-identical to the Rust reference.

## Known issues
- StartPark: the skater falls through the floor (the Rust snapshot we ported does the same, tick for tick); the page
  opens on BlackBoxPark instead.
- Rendering is three.js with PBR approximations: retail sky (SKYB) and retail shaders are not reproduced.
- No multiplayer, no Freestyle MX bike mod.
- First load downloads the 95 MB core.pack (~60 s on a typical connection), then the map.

## For the user
Open https://andrewnakas.github.io/skatecore-cpp/ , wait for the pack to load, Space to push, arrows to flick
(down then up = ollie), WASD to steer/crouch. Try MegaPark from the map menu. A gamepad works (press a button first).
