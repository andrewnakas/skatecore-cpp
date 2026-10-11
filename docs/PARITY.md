# Trace parity: C++ `s3trace` vs the simref Rust golden reference

Stage P3 (2026-10-11). The reference is `~/Documents/haloskate-ref/simref`: a Bevy-free Rust copy of the
skate-game simulation. Its `trace` binary (`crates/sim/src/bin/trace.rs`) runs the game's real input path
and `frame::advance` once per tick and writes one CSV row per tick. `build/s3trace` is the C++ port of that
tool. It runs `s3::game::driver::Sim` (the port of `driver.rs`) over the cleanroom `core.pack` and the
`.skate` map, and prints the same CSV.

## Result: bit-identical

**Every trace matches the Rust reference byte for byte.** That holds for the default output (floats in
shortest round-trip form, Rust `{:?}`) and for `--hex` (the raw IEEE-754 bits of every float). No tick
differs in any column, so there is no first divergence and the max error is 0 in every column.

| script | map | ticks | decimal CSV | `--hex` CSV | first divergence |
|---|---|---|---|---|---|
| simref `idle.txt` | KenneyPark | 300 | identical | identical | none |
| simref `push.txt` | KenneyPark | 400 | identical | identical | none |
| simref `ollie.txt` | KenneyPark | 360 | identical | identical | none |
| simref `kickflip.txt` | KenneyPark | 360 | identical | identical | none |
| simref `carve.txt` | KenneyPark | 360 | identical | identical | none |
| simref `push.txt`, `kickflip.txt` | test-world | 400 / 360 | identical | identical | none |
| `kickflip.txt --difficulty normal` | KenneyPark | 360 | identical | identical | none |
| `carve.txt --difficulty hardcore` | KenneyPark | 360 | identical | identical | none |
| `tools/parity_scripts/{chaos,dismount,fast}.txt` | KenneyPark and test-world | 2400 | identical | identical | none |
| `tools/parity_scripts/rand0..7.txt` | KenneyPark and test-world | 2400 | identical | identical | none |

The saved reference traces in `simref/traces/*.csv` are byte-identical to fresh runs of the Rust binary.
They are also byte-identical to the C++ output.

### Coverage of the extra scripts

The extra scripts are hand-written and seeded-random input sequences that reach far beyond the five
simref scripts. Together they visit the physical states PhysicsGround, PhysicsAir, KnownAir,
GroundAnimation, Teleporting (respawn), BipedGround and BipedAir (off-board), WipeoutGround (bails),
GrindBoardslide and SlideGround (powerslide). Tricks and scores cover ollie, heelflip, late heelflip,
180 and 360 spins, and FS/BS grabs.

## Tools

- `make s3trace` builds `build/s3trace <core.pack> <map.skate|test-world> <script> <ticks> [--difficulty d] [--hex]`.
  It has the same CLI semantics, CSV header and columns, float formatting and `clean()` rules as
  `trace.rs`.
- `tools/trace_diff.py <ref.csv> <cand.csv> [--tol x]` compares two traces. It reports the first differing
  tick and column, the first tick beyond the tolerance, the max |err| per numeric column (with its tick),
  and the first difference in each text column.
- `tools/parity.sh [--extra]` runs both binaries, in decimal and hex, over the simref scripts. With
  `--extra` it also runs `tools/parity_scripts/*.txt` on both maps plus the difficulty variants. It
  exits non-zero on any difference and prints the `trace_diff.py` summary.

## stderr

The CSV goes to stdout and is identical. The log lines on stderr are not part of the parity contract:

- **INFO lines:** by default the C++ port prints only some of them. Setting `S3_GAME_LOG=1` prints the
  game's `info!` lines too, and then the logs match the Rust logs.
- **Map timing lines:** the Rust `MAP_DECODE_TIMING` / `MAP_READ_TIMING` lines come from its file-based
  map loader and contain wall-clock times. The C++ port does not print them.

## Ported Rust tests that fail against the current Rust simulation

Six Rust `#[ignore]` private-asset playback tests are ported, but they are skipped unless
`S3_RUN_STALE=1` is set. They need the private stock assets, so the Rust project never runs them by
default. Cargo test builds are off-limits here, so they could not be run in Rust. Their expectations do
not hold for the current simulation:

- **Four tests replace the world with `BoardWorld::new(triangles)`:**
  `stock_skater_startup_builds_the_graphs_and_physical_body_from_the_same_rig`,
  `stock_push_off_edge_runs_air_and_landing_in_production`,
  `airborne_grabs_release_through_stock_graphs` and
  `airborne_tailwalk_from_raw_controller_reaches_stock_cycle`. `BoardWorld::new` carries no query
  metadata, so skate-core's `BoardWorld::query_metadata` returns
  `"Canonical world has no authored query metadata"` as soon as a query needs it. The Rust code returns
  the same error on the same path.
- **`raw_controller_powerslide_uses_stock_graph_and_releases_both_sides`:** the diagonal stick never
  starts the slide latch. A Rust reference trace with equivalent input never enters SlideGround either:
  `normal` difficulty, push and then the lateral/diagonal stick phases on test-world and KenneyPark. The
  C++ trace of that input is bit-identical. SlideGround is still reached and stays identical in the
  `rand4` stress trace.
- **`marker_reply_restores_on_foot`:** the on-foot manual reply goes Teleporting -> BipedGround, and
  then the stock selector requests PhysicsAir because the board controller is still on. The test
  expects PhysicsGround, with the board at the rider's checkpoint, at the tick after the reply. Its
  on-board sibling `authored_checkpoint_reply_runs_teleport_state_and_restores_riding` passes. This
  path (session-marker reply) is not reachable from the trace driver, so it is the one item here not
  cross-checked against the Rust binary.

## Not ported (by design)

- **Dead files:** the orphan `physics/offboard/*.rs` files that `offboard/mod.rs` does not declare.
  Also `bin/unpack.rs`, and `map_library.rs`, which the simulation driver does not use.
- **Unmounted test files:** `tests/action_host.rs`, `trick_turn.rs`, `board_bail_assembly.rs`,
  `difficulty.rs` and `offboard_root_trace.rs`. No module mounts them (`offboard_root_trace` is a
  `#[cfg(test)]` debug hook).
- **Inactive multiplayer path:** `physics/network.rs` is ported, but its `Schema` fingerprint hashes a
  C++ serialization instead of Rust `{:?}` text. That only matters between peers.
