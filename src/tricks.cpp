#include "tricks.hpp"
#include <cstring>

namespace skate {

namespace {

struct Row
{
	const char *gesture, *normal, *mirrored;
	unsigned dark_catch;
};

// TU3 82B98F70 records, Square group (TABLES[0] of gesture_mapping_data.rs), verbatim.
// Each tuple is (gesture key, normal intent, mirrored intent, dark-catch category).
const Row SQUARE[] = {
	{"Ollie", "Ollie", "Ollie", 1},
	{"PopShuvit", "PopShuvit", "FSPopShuvit", 1},
	{"FSPopShuvit", "FSPopShuvit", "PopShuvit", 2},
	{"VarialKickflip", "VarialKickflip", "VarialHeelflip", 1},
	{"VarialHeelflip", "VarialHeelflip", "VarialKickflip", 2},
	{"Hardflip", "Hardflip", "InwardHeelflip", 1},
	{"InwardHeelflip", "InwardHeelflip", "Hardflip", 2},
	{"360PopShuvit", "360PopShuvit", "FS360PopShuvit", 1},
	{"FS360PopShuvit", "FS360PopShuvit", "360PopShuvit", 2},
	{"360Flip", "360Flip", "Laserflip", 0},
	{"Laserflip", "Laserflip", "360Flip", 0},
	{"360Hardflip", "360Hardflip", "360InwardHeelflip", 0},
	{"360InwardHeelflip", "360InwardHeelflip", "360Hardflip", 0},
	{"Kickflip", "Kickflip", "Heelflip", 0},
	{"Heelflip", "Heelflip", "Kickflip", 0},
	{"Nollie", "Nollie", "Nollie", 3},
	{"N_PopShuvit", "N_PopShuvit", "N_FSPopShuvit", 3},
	{"N_FSPopShuvit", "N_FSPopShuvit", "N_PopShuvit", 4},
	{"N_VarialKickflip", "N_VarialKickflip", "N_VarialHeelflip", 3},
	{"N_VarialHeelflip", "N_VarialHeelflip", "N_VarialKickflip", 4},
	{"N_Hardflip", "N_Hardflip", "N_InwardHeelflip", 3},
	{"N_InwardHeelflip", "N_InwardHeelflip", "N_Hardflip", 4},
	{"N_360PopShuvit", "N_360PopShuvit", "N_FS360PopShuvit", 3},
	{"N_FS360PopShuvit", "N_FS360PopShuvit", "N_360PopShuvit", 4},
	{"N_360Flip", "N_360Flip", "N_Laserflip", 0},
	{"N_Laserflip", "N_Laserflip", "N_360Flip", 0},
	{"N_360Hardflip", "N_360Hardflip", "N_360InwardHeelflip", 0},
	{"N_360InwardHeelflip", "N_360InwardHeelflip", "N_360Hardflip", 0},
	{"N_Kickflip", "N_Kickflip", "N_Heelflip", 0},
	{"N_Heelflip", "N_Heelflip", "N_Kickflip", 0},
};

// Board motion per base trick. Timings: kickflip-ops clean sim (skate.rs:104-121); turns from the
// trick definitions (a kickflip is one full roll, a pop shove-it a half yaw, ...). TODO-DATA: the
// stock clips (B_<TRICK>) would give the exact curves.
struct Named
{
	const char *name;
	FlipSpec spec;
};
const Named FLIPS[] = {
	{"Ollie", {0, 0, 0.0f, 50, false, "Ollie"}},
	{"Nollie", {0, 0, 0.0f, 60, true, "Nollie"}},
	{"Kickflip", {1, 0, 0.42f, 150, false, "Kickflip"}},
	{"Heelflip", {-1, 0, 0.42f, 150, false, "Heelflip"}},
	{"PopShuvit", {0, 0.5f, 0.36f, 120, false, "Pop Shove-it"}},
	{"FSPopShuvit", {0, -0.5f, 0.36f, 120, false, "FS Pop Shove-it"}},
	{"360PopShuvit", {0, 1, 0.5f, 220, false, "360 Shove-it"}},
	{"FS360PopShuvit", {0, -1, 0.5f, 220, false, "FS 360 Shove-it"}},
	{"VarialKickflip", {1, 0.5f, 0.48f, 220, false, "Varial Kickflip"}},
	{"VarialHeelflip", {-1, -0.5f, 0.48f, 220, false, "Varial Heelflip"}},
	{"Hardflip", {1, -0.5f, 0.48f, 260, false, "Hardflip"}},
	{"InwardHeelflip", {-1, 0.5f, 0.48f, 260, false, "Inward Heelflip"}},
	{"360Flip", {1, 1, 0.55f, 300, false, "360 Flip"}},
	{"Laserflip", {-1, -1, 0.55f, 320, false, "Laser Flip"}},
	{"360Hardflip", {1, -1, 0.6f, 360, false, "360 Hardflip"}},
	{"360InwardHeelflip", {-1, 1, 0.6f, 360, false, "360 Inward Heelflip"}},
	{"KickflipHold", {1, 0, 0.42f, 150, false, "Kickflip"}},
	{"HeelflipHold", {-1, 0, 0.42f, 150, false, "Heelflip"}},
};

const Named NOLLIE_FLIPS[] = {
	{"N_Kickflip", {1, 0, 0.42f, 180, true, "Nollie Kickflip"}},
	{"N_Heelflip", {-1, 0, 0.42f, 180, true, "Nollie Heelflip"}},
	{"N_PopShuvit", {0, 0.5f, 0.36f, 140, true, "Nollie Shove-it"}},
	{"N_FSPopShuvit", {0, -0.5f, 0.36f, 140, true, "Nollie FS Shove-it"}},
	{"N_VarialKickflip", {1, 0.5f, 0.48f, 250, true, "Nollie Varial Kickflip"}},
	{"N_VarialHeelflip", {-1, -0.5f, 0.48f, 250, true, "Nollie Varial Heelflip"}},
	{"N_Hardflip", {1, -0.5f, 0.48f, 290, true, "Nollie Hardflip"}},
	{"N_InwardHeelflip", {-1, 0.5f, 0.48f, 290, true, "Nollie Inward Heelflip"}},
	{"N_360PopShuvit", {0, 1, 0.5f, 250, true, "Nollie 360 Shove-it"}},
	{"N_FS360PopShuvit", {0, -1, 0.5f, 250, true, "Nollie FS 360 Shove-it"}},
	{"N_360Flip", {1, 1, 0.55f, 340, true, "Nollie 360 Flip"}},
	{"N_Laserflip", {-1, -1, 0.55f, 360, true, "Nollie Laser Flip"}},
	{"N_360Hardflip", {1, -1, 0.6f, 400, true, "Nollie 360 Hardflip"}},
	{"N_360InwardHeelflip", {-1, 1, 0.6f, 400, true, "Nollie 360 Inward Heelflip"}},
	{"N_KickflipHold", {1, 0, 0.42f, 180, true, "Nollie Kickflip"}},
	{"N_HeelflipHold", {-1, 0, 0.42f, 180, true, "Nollie Heelflip"}},
};

} // namespace

// 82BA07F0. With one gesture intent per pop the hash-bucket ordering of the original
// (sum of intent_key words mod 43) never has to break a tie, so rows are matched directly.
const char *trick_select(const char *gesture, bool mirrored, bool dark_catch, bool dont_mirror)
{
	const Row *row = nullptr;
	for (const Row &r : SQUARE)
		if (std::strcmp(r.gesture, gesture) == 0)
			row = &r;
	if (!row)
		return nullptr;
	mirrored = mirrored && !dont_mirror;
	if (dark_catch && row->dark_catch != 0)
	{
		unsigned c = row->dark_catch;
		if ((c == 2 && !mirrored) || (c == 1 && mirrored))
			return "Heelflip";
		if ((c == 3 && mirrored) || (c == 4 && !mirrored))
			return "N_Heelflip";
		if ((c == 3 && !mirrored) || (c == 4 && mirrored))
			return "N_Kickflip";
		return "Kickflip";
	}
	return mirrored ? row->mirrored : row->normal;
}

const char *trick_hold_successor(const char *trick)
{
	if (!std::strcmp(trick, "Kickflip"))
		return "KickflipHold";
	if (!std::strcmp(trick, "Heelflip"))
		return "HeelflipHold";
	if (!std::strcmp(trick, "N_Kickflip"))
		return "N_KickflipHold";
	if (!std::strcmp(trick, "N_Heelflip"))
		return "N_HeelflipHold";
	return nullptr;
}

const FlipSpec *trick_flip_spec(const char *trick)
{
	for (const Named &n : FLIPS)
		if (!std::strcmp(n.name, trick))
			return &n.spec;
	for (const Named &n : NOLLIE_FLIPS)
		if (!std::strcmp(n.name, trick))
			return &n.spec;
	return nullptr;
}

} // namespace skate
