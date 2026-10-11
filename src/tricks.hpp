// Gesture -> trick intent (port of skate-game `input/gesture_mapping.rs` + the Square table of
// `gesture_mapping_data.rs`), plus the procedural flip table that stands in for the ABIN board clips.
#pragma once
#include <string>

namespace skate {

// select(): normal or mirrored intent for a recognised gesture; DarkCatch override.
// Returns nullptr if the gesture has no row.
const char *trick_select(const char *gesture, bool mirrored, bool dark_catch, bool dont_mirror);

// Kickflip -> KickflipHold etc. (State::begin successor), or nullptr.
const char *trick_hold_successor(const char *trick);

// Procedural substitute for the clip's board bone (docs/PORTMAP.md 1.3 item 5).
struct FlipSpec
{
	float roll_turns;  // board roll about its long axis, + = kickflip direction
	float yaw_turns;   // board yaw (shove-it), + = backside
	float duration;    // seconds for the full rotation
	unsigned points;
	bool nollie;       // pops off the nose
	const char *display; // HUD name
};

// nullptr for gestures that only pop (Ollie / Nollie return a spec with zero turns).
const FlipSpec *trick_flip_spec(const char *trick);

} // namespace skate
