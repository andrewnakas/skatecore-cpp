// The skater: board + rider as one body on host collision.
// Structure: procedural substitute layer (docs/PORTMAP.md 2.5) driving ported skate-core pieces
// (flick-it recognizer, trick mapping, manual target tilt, landing judgement).
#pragma once
#include "gesture.hpp"
#include "math.hpp"
#include "tricks.hpp"
#include "world.hpp"
#include <deque>
#include <string>
#include <vector>

namespace skate {

// Every field is hand-authored (TODO-DATA: the stock VLT values are not available).
struct Tuning
{
	float gravity = 19.6f;        // arcade gravity, matches the kickflip-ops reference feel
	float push_impulse = 1.9f;    // m/s per kick
	float push_interval = 0.55f;  // s between kicks
	float push_cap = 9.0f;        // m/s: no kicking above this
	float rolling_resistance = 0.25f;
	float air_drag = 0.004f;
	float brake_decel = 5.0f;
	float powerslide_decel = 7.5f;
	float carve_rate = 2.2f;      // rad/s at speed
	float pop_min = 4.6f;         // m/s
	float pop_max = 6.4f;
	float crouch_full = 0.30f;    // s of crouch for a full pop
	float air_spin_rate = 6.0f;   // rad/s
	float grind_snap = 0.45f;     // m
	float grind_friction = 0.5f;
	float clean_land_deg = 24.0f;
	float bail_land_deg = 62.0f;
	float wall_bail_speed = 7.0f;
	float bail_time = 1.6f;
	float bank_delay = 1.0f;
	float radius = 0.28f;         // collision sphere around the board
	float max_landing_speed = 14.0f; // into-surface speed that always bails (wipeout reason 6)
	// manual/controller.rs: target tilt = sign*(0.25+0.75|bal|)*max_tilt
	float manual_max_tilt = 0.30f;
	unsigned char gesture_maximum_misses = 4; // recognizer config byte 33 (TODO-DATA)
};

enum class State { Rolling, Manual, Powerslide, Air, Grind, Bail };

struct Event
{
	skate_event_type type;
	std::string name;
	unsigned points = 0;
	int value = 0;
};

struct Combo
{
	std::vector<std::string> tricks;
	unsigned value = 0, multiplier = 0;
	unsigned long long banked = 0;
	float bank_timer = 0;
	void add(const std::string &name, unsigned base, std::deque<Event> &ev);
	void tick_grind(float dt);
	void lose();
};

class Skater
{
public:
	Skater();
	void enter(Vec3 pos, Vec3 vel, float facing);
	void step(World &w, const skate_input &in);
	void force_bail();
	void add_bonus(const std::string &name, unsigned points) { combo.add(name, points, events); }
	Quat board_rotation() const;
	float board_yaw() const { return heading + spin + flip_yaw; }
	const char *current_trick() const;
	const char *debug_feed(float x, float y);

	Tuning t;
	State state = State::Air;
	Vec3 pos, vel;
	float heading = 0; // board yaw
	bool fakie = false;
	Vec3 ground_normal = UP;
	float crouch = 0, spin = 0, flip_roll = 0, flip_yaw = 0, lean = 0;
	bool grabbing = false;
	float manual_balance = 0;
	Combo combo;
	std::deque<Event> events;

private:
	struct AirTrick
	{
		std::string name;
		unsigned points;
	};
	void step_ground(World &w, const skate_input &in, float steer);
	void step_air(World &w, const skate_input &in, float steer);
	void step_grind(World &w, const skate_input &in, float steer);
	void step_bail(World &w);
	void pop(float vy, const char *trick);
	void start_flip(const char *trick);
	void land(Vec3 normal, float into_speed);
	void enter_grind(const RailSample &rail);
	void start_bail(const char *why);
	Vec3 travel_dir() const { Vec3 f = forward(heading); return fakie ? -f : f; }
	std::optional<std::string> sample_gesture(const skate_input &in);

	Recognizer recognizer_;
	const FlipSpec *flip_ = nullptr;
	std::string flip_name_;
	float flip_elapsed_ = 0;
	std::vector<AirTrick> air_tricks_;
	float push_timer_ = 0;
	float crouch_time_ = -1; // <0: not crouching
	unsigned grind_rail_ = 0;
	float grind_sign_ = 1;
	std::string grind_name_;
	float grind_time_ = 0;
	float grind_cooldown_ = 0; // no re-latch right after leaving a rail
	float bail_timer_ = 0;
	float air_time_ = 0;
	int manual_dir_ = 0;
	float wobble_ = 0;
	float manual_tilt_ = 0;
	Vec3 last_safe_ = {};
	float last_safe_heading_ = 0;
	float safe_timer_ = 0;
	std::string pending_flip_; // gesture recognised in the same tick as the pop
};

} // namespace skate
