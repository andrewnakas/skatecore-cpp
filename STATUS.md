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

## In progress
- Stage D (data loaders), Stage C (core math + physics).

## Trace parity
(none yet)

## Questions
(none)
