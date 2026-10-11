#include "skater.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace skate {

static constexpr float DT = SKATE_DT;

void Combo::add(const std::string &name, unsigned base, std::deque<Event> &ev)
{
	multiplier += 1;
	unsigned pts = base * multiplier;
	value += pts;
	tricks.push_back(name);
	bank_timer = 0;
	ev.push_back({SKATE_EVENT_TRICK, name, pts, 0});
}

void Combo::tick_grind(float dt)
{
	value += (unsigned)std::ceil(40.0f * dt * (float)(multiplier ? multiplier : 1));
	bank_timer = 0;
}

void Combo::lose()
{
	tricks.clear();
	value = 0;
	multiplier = 0;
}

Skater::Skater()
{
	recognizer_.init(parse_patterns(authored_right_stick_patterns()));
}

void Skater::enter(Vec3 p, Vec3 v, float facing)
{
	pos = p;
	vel = v;
	Vec3 h{v.x, 0, v.z};
	heading = h.length() > 0.5f ? std::atan2(h.x, h.z) : facing;
	fakie = false;
	state = State::Air;
	spin = flip_roll = flip_yaw = 0;
	flip_ = nullptr;
	air_tricks_.clear();
	crouch_time_ = -1;
	air_time_ = 0;
	last_safe_ = p;
	last_safe_heading_ = heading;
	recognizer_.reset();
}

void Skater::force_bail()
{
	if (state != State::Bail)
		start_bail("forced");
}

const char *Skater::current_trick() const
{
	if (state == State::Grind)
		return grind_name_.c_str();
	if (state == State::Manual)
		return manual_dir_ > 0 ? "Manual" : "Nose Manual";
	if (state == State::Powerslide)
		return "Powerslide";
	if (flip_)
		return flip_->display;
	return "";
}

std::optional<std::string> Skater::sample_gesture(const skate_input &in)
{
	// Native manager negates mapped Y; component deadzone 0.1 (gesture_input.rs).
	auto dz = [](float v) { return std::fabs(v) < 0.1f ? 0.0f : v; };
	std::pair<float, float> s{dz(in.right_stick[0]), dz(-in.right_stick[1])};
	recognizer_.held(s);
	auto r = recognizer_.sample(s, GestureSettings{t.gesture_maximum_misses, 0});
	if (!r)
		return std::nullopt;
	const char *gesture = recognizer_.patterns()[r->pattern].name.c_str();
	const char *trick = trick_select(gesture, false, false, false);
	if (!trick)
		return std::nullopt;
	events.push_back({SKATE_EVENT_POP, trick, 0, (int)(r->strength * 100)});
	return std::string(trick);
}

const char *Skater::debug_feed(float x, float y)
{
	static std::string last;
	skate_input in{};
	in.right_stick[0] = x;
	in.right_stick[1] = y;
	auto g = sample_gesture(in);
	if (!g)
		return nullptr;
	last = *g;
	return last.c_str();
}

void Skater::step(World &w, const skate_input &in)
{
	wobble_ += DT;
	float steer = -in.left_stick[0]; // + = turn left (yaw toward +X)
	std::optional<std::string> gesture = sample_gesture(in);
	if (gesture)
		pending_flip_ = *gesture;

	switch (state)
	{
	case State::Rolling:
	case State::Manual:
	case State::Powerslide:
		step_ground(w, in, steer);
		break;
	case State::Air:
		step_air(w, in, steer);
		break;
	case State::Grind:
		step_grind(w, in, steer);
		break;
	case State::Bail:
		step_bail(w);
		break;
	}
	pending_flip_.clear();

	// Bank the combo after a stretch of clean rolling.
	if (!combo.tricks.empty())
	{
		if (state == State::Rolling || state == State::Powerslide)
		{
			combo.bank_timer += DT;
			if (combo.bank_timer >= t.bank_delay)
			{
				unsigned v = combo.value;
				combo.banked += v;
				combo.lose();
				events.push_back({SKATE_EVENT_BANKED, "", v, 0});
			}
		}
		else
		{
			combo.bank_timer = 0;
		}
	}
	while (events.size() > 64)
		events.pop_front();
}

void Skater::step_ground(World &w, const skate_input &in, float steer)
{
	Vec3 n = ground_normal;
	float speed = std::fmax(vel.dot(travel_dir()), 0.0f);
	bool down = in.right_stick[1] < -0.6f;

	// Flick-it: crouch while the stick is held back; the recognised gesture pops.
	if (down)
	{
		crouch_time_ = crouch_time_ < 0 ? 0 : crouch_time_ + DT;
		crouch = std::fmin(crouch_time_ / t.crouch_full, 1.0f);
	}
	else
	{
		crouch = std::fmax(crouch - DT * 5.0f, 0.0f);
	}
	if (!pending_flip_.empty())
	{
		float k = crouch_time_ < 0 ? 0.35f : std::fmin(crouch_time_ / t.crouch_full, 1.0f);
		float strength = events.empty() ? 1.0f : (float)events.back().value / 100.0f;
		float mix = std::fmax(k, 0.6f * strength);
		crouch_time_ = -1;
		crouch = 0;
		pop(t.pop_min + (t.pop_max - t.pop_min) * mix, pending_flip_.c_str());
		return;
	}
	if (!down && crouch_time_ >= 0)
	{
		// The flick passes through the centre: keep the crouch alive for a few ticks.
		crouch_time_ += DT;
		if (crouch_time_ > t.crouch_full + 0.25f)
			crouch_time_ = -1;
	}

	// Powerslide: board kicked sideways, heavy scrub, travel direction kept.
	bool sliding = (in.buttons & SKATE_BUTTON_POWERSLIDE) && speed > 2.0f && std::fabs(steer) > 0.2f;
	if (sliding)
	{
		if (state != State::Powerslide)
		{
			state = State::Powerslide;
			combo.add("Powerslide", 60, events);
		}
		speed = std::fmax(speed - t.powerslide_decel * DT, 0.0f);
	}
	else if (state == State::Powerslide)
	{
		state = State::Rolling;
	}

	// Carving: slower turn-in at very low speed, tighter at mid speed.
	if (!sliding)
	{
		float rate = t.carve_rate * (0.55f + 0.45f * std::fmin(speed / 6.0f, 1.0f));
		heading += steer * rate * DT;
	}
	lean += (clampf(steer * std::fmin(speed / 4.0f, 1.0f), -1, 1) - lean) * 0.2f;

	// Pushing is a series of kicks, not continuous thrust.
	push_timer_ -= DT;
	bool can_push = state == State::Rolling && crouch_time_ < 0;
	if (can_push && (in.buttons & SKATE_BUTTON_PUSH) && push_timer_ <= 0)
	{
		float room = std::fmax(t.push_cap - speed, 0.0f);
		speed += std::fmin(t.push_impulse, room);
		push_timer_ = t.push_interval;
		if (speed < 0.5f)
			fakie = false;
	}
	if ((in.buttons & SKATE_BUTTON_BRAKE) && !sliding)
		speed = std::fmax(speed - t.brake_decel * DT, 0.0f);

	// Slope: gravity along the surface in the travel direction.
	Vec3 dir = travel_dir();
	Vec3 along = (dir - n * dir.dot(n)).normalize_or_zero();
	speed += Vec3{0, -t.gravity, 0}.dot(along) * DT;
	if (speed < 0)
	{
		// Rolled back down: now going fakie.
		fakie = !fakie;
		speed = -speed;
	}
	speed = std::fmax(speed - (t.rolling_resistance + t.air_drag * speed * speed) * DT, 0.0f);
	dir = travel_dir();
	along = (dir - n * dir.dot(n)).normalize_or_zero();
	vel = along * speed;

	// Manuals (skate-core manual/controller.rs target tilt; balance with the left stick).
	int want_manual = (in.buttons & SKATE_BUTTON_MANUAL) ? 1 : (in.buttons & SKATE_BUTTON_NOSE_MANUAL) ? -1 : 0;
	if (want_manual != 0 && speed > 1.0f && state == State::Rolling)
	{
		state = State::Manual;
		manual_dir_ = want_manual;
		manual_balance = 0;
		combo.add(want_manual > 0 ? "Manual" : "Nose Manual", 80, events);
	}
	if (state == State::Manual)
	{
		if (want_manual == 0)
		{
			state = State::Rolling;
			manual_tilt_ = 0;
		}
		else
		{
			float drift = std::sin(wobble_ * 2.3f) * 0.6f + std::sin(wobble_ * 5.1f) * 0.25f;
			manual_balance += (drift - in.left_stick[1] * 1.6f * (float)manual_dir_) * DT;
			float target = (float)manual_dir_ * (0.25f + 0.75f * std::fabs(manual_balance)) * t.manual_max_tilt;
			// error filter 0.95/0.05 (manual/controller.rs)
			manual_tilt_ = manual_tilt_ * 0.95f + target * 0.05f;
			combo.tick_grind(DT * 0.5f);
			if (std::fabs(manual_balance) > 1.0f)
			{
				start_bail("manual balance");
				return;
			}
		}
	}

	// Integrate along the surface, then re-seat on the ground.
	float r = t.radius;
	std::optional<Vec3> contact;
	Vec3 center = w.resolve_sphere(pos + n * r + vel * DT, r, &contact);
	if (contact && contact->y < 0.5f)
	{
		Vec3 cn = *contact;
		float into = -vel.dot(cn);
		if (into > t.wall_bail_speed)
		{
			pos = center - UP * r;
			start_bail("wall");
			return;
		}
		if (into > 0)
		{
			vel += cn * into;
			Vec3 h{vel.x, 0, vel.z};
			if (h.length() > 0.3f)
			{
				float yaw = std::atan2(h.x, h.z);
				heading = fakie ? yaw + PI : yaw;
			}
		}
	}
	pos = center - UP * r;

	auto ground = w.raycast(pos + UP * 0.45f, {0, -1, 0}, 0.45f + 0.3f);
	if (ground && ground->normal.y > 0.55f)
	{
		pos = ground->point;
		ground_normal = ground_normal.lerp(ground->normal, 0.35f).normalize_or_zero();
		safe_timer_ += DT;
		if (state == State::Rolling && safe_timer_ > 0.5f)
		{
			last_safe_ = pos;
			last_safe_heading_ = heading;
			safe_timer_ = 0;
		}
	}
	else
	{
		// Rolled off a ledge.
		state = State::Air;
		air_time_ = 0;
		spin = 0;
		air_tricks_.clear();
	}
	heading = wrap_pi(heading);
}

void Skater::pop(float vy, const char *trick)
{
	state = State::Air;
	air_time_ = 0;
	vel += ground_normal * vy;
	spin = 0;
	air_tricks_.clear();
	manual_tilt_ = 0;
	const FlipSpec *spec = trick_flip_spec(trick);
	bool nollie = spec && spec->nollie;
	const char *name = nollie ? "Nollie" : fakie ? "Fakie Ollie" : "Ollie";
	air_tricks_.push_back({name, nollie ? 60u : 50u});
	if (spec && (spec->roll_turns != 0 || spec->yaw_turns != 0))
		start_flip(trick);
	pos += UP * 0.02f;
}

void Skater::start_flip(const char *trick)
{
	const FlipSpec *spec = trick_flip_spec(trick);
	if (!spec || (spec->roll_turns == 0 && spec->yaw_turns == 0))
		return;
	flip_ = spec;
	flip_name_ = trick;
	flip_elapsed_ = 0;
	// Ollie credit is folded into the flip trick.
	if (!air_tricks_.empty() && (air_tricks_.back().name.find("Ollie") != std::string::npos ||
		air_tricks_.back().name == "Nollie"))
		air_tricks_.pop_back();
}

void Skater::step_air(World &w, const skate_input &in, float steer)
{
	air_time_ += DT;
	spin += steer * t.air_spin_rate * DT;
	bool grab_l = in.buttons & SKATE_BUTTON_GRAB_LEFT, grab_r = in.buttons & SKATE_BUTTON_GRAB_RIGHT;
	grabbing = grab_l || grab_r;
	crouch = grabbing ? 0.8f : std::fmax(crouch - DT * 4.0f, 0.25f);
	lean *= 0.95f;

	// A late flick right after the pop still flips (no second pop).
	if (!pending_flip_.empty() && !flip_ && air_time_ < 0.3f)
		start_flip(pending_flip_.c_str());

	if (flip_)
	{
		float dur = flip_->duration;
		float prev = std::fmin(flip_elapsed_ / dur, 1.0f);
		flip_elapsed_ += DT;
		float now = std::fmin(flip_elapsed_ / dur, 1.0f);
		auto ease = [](float x) { return x * x * (3.0f - 2.0f * x); };
		flip_roll += flip_->roll_turns * TAU * (ease(now) - ease(prev));
		flip_yaw += flip_->yaw_turns * TAU * (ease(now) - ease(prev));
		if (now >= 1.0f)
		{
			air_tricks_.push_back({flip_->display, flip_->points});
			heading += flip_->yaw_turns * TAU; // a finished shove-it leaves the board turned
			flip_roll = 0;
			flip_yaw = 0;
			flip_ = nullptr;
		}
	}
	if (grabbing)
	{
		const char *g = grab_l && grab_r ? "Stalefish" : grab_l ? "Melon Grab" : "Indy Grab";
		bool have = false;
		for (auto &a : air_tricks_)
			have = have || a.name.find("Grab") != std::string::npos || a.name == "Stalefish";
		if (!have)
			air_tricks_.push_back({g, 100});
	}

	vel.y -= t.gravity * DT;
	vel -= vel * (t.air_drag * vel.length() * DT);
	float r = t.radius;
	Vec3 center = pos + UP * r + vel * DT;

	// Rails: latch when coming down onto one (not onto its very tip, not right after leaving one).
	grind_cooldown_ = std::fmax(grind_cooldown_ - DT, 0.0f);
	if (vel.y < 0.5f && grind_cooldown_ <= 0)
	{
		auto rail = w.nearest_rail(center - UP * r, t.grind_snap);
		if (rail && !rail->at_end && center.y - r >= rail->point.y - 0.15f)
		{
			enter_grind(*rail);
			return;
		}
	}

	std::optional<Vec3> contact;
	Vec3 before = vel;
	center = w.resolve_sphere(center, r, &contact);
	pos = center - UP * r;
	if (contact)
	{
		Vec3 cn = *contact;
		float into = vel.dot(cn);
		Vec3 flat_before{vel.x, 0, vel.z};
		if (into < 0)
			vel -= cn * into;
		// A glancing wall in the air turns rider and board with the new direction of travel.
		Vec3 flat_after{vel.x, 0, vel.z};
		if (cn.y < 0.55f && flat_before.length() > 0.8f && flat_after.length() > 0.8f)
			heading += wrap_pi(std::atan2(flat_after.x, flat_after.z) - std::atan2(flat_before.x, flat_before.z));
		if (cn.y > 0.55f && vel.y <= 0.5f)
			land(cn, -before.dot(cn));
		else if (cn.y < 0.3f && -into > t.wall_bail_speed)
			start_bail("wall");
	}
	else if (vel.y <= 0)
	{
		// The sphere probes can miss a thin floor at speed: check the swept path down.
		auto ground = w.raycast(pos + UP * 0.3f, {0, -1, 0}, 0.3f + std::fmax(-vel.y * DT, 0.0f) + 0.02f);
		if (ground && ground->normal.y > 0.55f)
		{
			pos = ground->point;
			land(ground->normal, -before.dot(ground->normal));
		}
	}
}

void Skater::land(Vec3 normal, float into_speed)
{
	// Flip still rotating on touchdown = feet on a spinning board (wipeout air.rs reason 4/5).
	bool flip_unfinished = flip_ && flip_elapsed_ / flip_->duration < 0.82f;
	Vec3 travel{vel.x, 0, vel.z};
	float board = heading + spin + flip_yaw;
	float mis;
	bool land_fakie;
	if (travel.length() > 0.8f)
	{
		float tyaw = std::atan2(travel.x, travel.z);
		float d = std::fabs(wrap_pi(board - tyaw));
		if (d <= PI / 2)
		{
			mis = d;
			land_fakie = false;
		}
		else
		{
			mis = PI - d;
			land_fakie = true;
		}
	}
	else
	{
		float s = std::fabs(wrap_pi(spin));
		mis = std::fmin(s, std::fabs(PI - s));
		land_fakie = fakie;
	}
	float deg = mis * 180.0f / PI;
	std::vector<AirTrick> tricks;
	tricks.swap(air_tricks_);
	if (flip_unfinished || deg > t.bail_land_deg || grabbing || into_speed > t.max_landing_speed)
	{
		for (auto &a : tricks)
			combo.add(a.name, a.points, events);
		char why[48];
		std::snprintf(why, sizeof(why), "%s %.0f deg %.1f m/s", flip_unfinished ? "flip" : grabbing ? "grab" :
			into_speed > t.max_landing_speed ? "impact" : "angle", deg, into_speed);
		start_bail(why);
		return;
	}
	// Spins count once they're within landing tolerance.
	float spun = std::fabs(spin) * 180.0f / PI;
	const char *spin_name = spun > 880 ? "900" : spun > 700 ? "720" : spun > 520 ? "540" : spun > 340 ? "360" : spun > 150 ? "180" : nullptr;
	unsigned spin_pts = spun > 880 ? 900 : spun > 700 ? 700 : spun > 520 ? 500 : spun > 340 ? 300 : 120;
	bool only_pops = true;
	for (auto &a : tricks)
	{
		bool pop_name = a.name.find("Ollie") != std::string::npos || a.name == "Nollie" ||
			a.name.find("Grab") != std::string::npos || a.name == "Stalefish";
		only_pops = only_pops && pop_name;
		std::string name = (spin_name && !pop_name) ? std::string(spin < 0 ? "FS " : "BS ") + spin_name + " " + a.name : a.name;
		combo.add(name, a.points, events);
	}
	if (spin_name && only_pops)
		combo.add(std::string(spin < 0 ? "FS " : "BS ") + spin_name, spin_pts, events);
	bool clean = deg <= t.clean_land_deg;
	if (!clean)
		vel *= 0.6f; // sketchy landing scrubs speed
	heading = wrap_pi(board);
	fakie = land_fakie;
	spin = 0;
	flip_ = nullptr;
	flip_roll = flip_yaw = 0;
	ground_normal = normal;
	float into = vel.dot(normal);
	if (into < 0)
		vel -= normal * into;
	state = State::Rolling;
	crouch = 0.6f;
	events.push_back({SKATE_EVENT_LANDED, "", 0, clean ? 1 : 0});
}

void Skater::enter_grind(const RailSample &rail)
{
	float along = vel.dot(rail.tangent);
	float sign = along >= 0 ? 1.0f : -1.0f;
	float speed = std::fmax(vel.length(), 2.5f);
	float rail_yaw = std::atan2(rail.tangent.x * sign, rail.tangent.z * sign);
	float board = heading + spin + flip_yaw;
	float rel = std::fabs(wrap_pi(board - rail_yaw));
	rel = std::fmin(rel, PI - rel);
	const char *name = rel > 55 * PI / 180 ? "Boardslide" : rel > 25 * PI / 180 ? "Crooked Grind" :
		vel.y < -3.0f ? "5-0 Grind" : "50-50 Grind";
	// Air tricks into the rail count.
	for (auto &a : air_tricks_)
		if (a.name.find("Ollie") == std::string::npos || combo.tricks.empty())
			combo.add(a.name, a.points, events);
	air_tricks_.clear();
	flip_ = nullptr;
	flip_roll = flip_yaw = spin = 0;
	heading = rel > 55 * PI / 180 ? rail_yaw + PI / 2 : rail_yaw;
	fakie = false;
	grind_rail_ = rail.rail;
	grind_sign_ = sign;
	grind_name_ = name;
	grind_time_ = 0;
	vel = rail.tangent * (sign * speed);
	pos = rail.point;
	manual_balance = 0;
	state = State::Grind;
	combo.add(name, 100, events);
	events.push_back({SKATE_EVENT_GRIND, name, 0, 0});
}

void Skater::step_grind(World &w, const skate_input &in, float steer)
{
	grind_time_ += DT;
	float speed = std::fmax(vel.length() - t.grind_friction * DT, 0.8f);
	Vec3 next = pos + vel * DT;
	auto r = w.nearest_rail(next, 0.6f);
	// Off the end of the rail, or too slow to keep sliding (grind/advance.rs: slow exit after 60 updates).
	bool off_end = r && r->rail == grind_rail_ && r->at_end;
	bool stalled = grind_time_ > 1.0f && speed < 0.9f;
	if (stalled)
	{
		grind_cooldown_ = 0.5f;
		state = State::Rolling;
		ground_normal = UP;
		vel = vel * 0.5f;
		events.push_back({SKATE_EVENT_LANDED, "", 0, 1});
		return;
	}
	if (r && r->rail == grind_rail_ && !off_end && r->point.distance(next) < 0.3f)
	{
		// Downhill rails speed you up.
		Vec3 tangent = r->tangent * grind_sign_;
		float s = speed - t.gravity * tangent.y * DT;
		vel = tangent * std::fmax(s, 0.8f);
		pos = r->point;
	}
	else
	{
		// Ran off the end.
		grind_cooldown_ = 0.3f;
		state = State::Air;
		air_time_ = 0;
		air_tricks_.clear();
		return;
	}
	combo.tick_grind(DT);
	manual_balance += (std::sin(wobble_ * 1.9f + pos.x) * 0.5f - steer * 1.2f) * DT;
	lean = clampf(manual_balance, -1, 1);
	if (std::fabs(manual_balance) > 1.15f)
	{
		start_bail("grind balance");
		return;
	}
	// Any recognised flick pops off the rail (Ollie out, or a flip out).
	if (!pending_flip_.empty())
	{
		ground_normal = UP;
		grind_cooldown_ = 0.3f;
		pop(t.pop_min, pending_flip_.c_str());
	}
	(void)in;
}

void Skater::start_bail(const char *why)
{
	state = State::Bail;
	bail_timer_ = t.bail_time;
	combo.lose();
	flip_ = nullptr;
	flip_roll = flip_yaw = 0;
	air_tricks_.clear();
	crouch_time_ = -1;
	manual_tilt_ = 0;
	events.push_back({SKATE_EVENT_BAIL, why, 0, 0});
	Vec3 h{vel.x, 0, vel.z};
	vel = h * 0.45f + UP * 2.5f;
}

void Skater::step_bail(World &w)
{
	bail_timer_ -= DT;
	vel.y -= t.gravity * DT;
	float r = t.radius;
	std::optional<Vec3> n;
	Vec3 c = w.resolve_sphere(pos + UP * r + vel * DT, r, &n);
	pos = c - UP * r;
	if (n)
	{
		float into = vel.dot(*n);
		if (into < 0)
			vel -= *n * (into * 1.3f);
		vel *= 1.0f - std::fmin(3.0f * DT, 1.0f);
	}
	if (bail_timer_ <= 0)
	{
		// Get back up where you fell, board under your feet; fell into a pit: last safe spot
		// (core/player/respawn.rs History, simplified to one entry).
		auto ground = w.raycast(pos + UP * 0.5f, {0, -1, 0}, 3.0f);
		if (!ground || ground->normal.y < 0.55f)
		{
			pos = last_safe_;
			heading = last_safe_heading_;
		}
		vel = {};
		spin = 0;
		fakie = false;
		state = State::Air;
		manual_balance = 0;
		lean = 0;
	}
}

Quat Skater::board_rotation() const
{
	Vec3 up = state == State::Air || state == State::Bail ? UP : ground_normal;
	Quat tilt = Quat::arc(UP, up.normalize_or_zero());
	Quat yaw = Quat::rotation_y(board_yaw());
	Quat slide = state == State::Powerslide ? Quat::rotation_y(PI / 2) : Quat{};
	Quat manual = state == State::Manual ? Quat::rotation_x(-manual_tilt_ - 0.12f * (float)manual_dir_) : Quat{};
	Quat roll = Quat::rotation_z(flip_roll);
	return (tilt * yaw * slide * manual * roll).normalized();
}

} // namespace skate
