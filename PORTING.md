# Porting notes: skate-core → `skate/` (C++17)

Source of truth for behaviour: `skate-3-rust-engine` branch `cleanroom-web`, crates `skate-core`
and `skate-game` (Rust). See `docs/PORTMAP.md` for the full call graph and data audit.

## Ported verbatim
| C++ | Rust | Notes |
|---|---|---|
| `src/gesture.{hpp,cpp}` `Recognizer` | `skate-core/src/input/gesture.rs` | line for line, incl. the two-sided 0.15 score clamp, no-FMA distance, refractory tick, held pattern, difficulty speed bands (1.75/4.4, 1.5/3.0). Tests: both recovered vectors. |
| `parse_patterns` | `skate_data::gesture_patterns::parse` | same directives; time/speed ignored. |
| `src/tricks.cpp` `SQUARE` table, `trick_select`, `trick_hold_successor` | `skate-game/src/input/gesture_mapping{,_data}.rs` | Square group rows verbatim (mirror pairs, dark-catch categories). Hash-bucket tie-break (sum of `intent_key` words mod 43) is not needed: one gesture intent per pop. |
| sample conditioning | `skate-game/src/input/gesture_input.rs` | Y negation, 0.1 per-component deadzone, 60 Hz sampling. |
| manual target tilt + error filter | `skate-core/src/physics/manual/controller.rs` | target = sign·(0.25+0.75·|bal|)·MaxTilt, filter 0.95/0.05. |
| landing judgement reasons | `skate-core/src/player/wipeout/air.rs` | unfinished flip / bad angle / into-surface speed > max landing speed. |

## Substituted (animation-driven or data-driven in the original)
| What | Original | Here |
|---|---|---|
| gesture patterns | stock `skater.pat` (retail data, not shipped) | `authored_right_stick_patterns()`: 16 authored shapes; the 360 flip uses the recovered test points exactly. |
| pop / push / flip board pose | ABIN clips + Action/Motion graphs drive physics flags and the board bone | procedural layer in `skater.cpp`: pop on recognised gesture, strength from crouch time and gesture strength; flip = eased roll/yaw turns over the durations in `tricks.cpp` (`FlipSpec`). |
| board + 26-body rider ragdoll | `physics/board_*`, `skeleton_*` | one body (sphere probes + ground ray) carrying board and rider. Full `BoardRuntime` port is a later stage behind the same C API. |
| tuning (VLT) | `skater-collections.json` | `Tuning` struct, hand-authored, every field TODO-DATA. |
| respawn history | `player/respawn.rs` (32-entry ring) | one last-safe entry; get up where you fell unless there is no floor. |
| grind rails | `grind_world/derive.rs` | M4 (stub returns 0 rails). |

## Conventions
Skate space: metres, +Y up, nose +Z at heading 0, right-handed, fixed step 1/60 s.
Halo host conversion (`engine/source/skate/`): `halo = (z, x, y) / 3.048` — a cyclic permutation, so
cross products are preserved. Halo ticks at 30 Hz: the host steps the skater twice per tick.
