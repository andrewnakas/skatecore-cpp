// Native tests for the skate library: recognizer vectors ported from skate-core, mapping table
// checks ported from skate-game, and controller behaviour on a flat test world.
#include "../include/skate_api.h"
#include "../src/gesture.hpp"
#include "../src/skater.hpp"
#include "../src/tricks.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0, checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

using namespace skate;

// ---------- test world: floor y=0, optional wall plane z=wall (facing -z)
struct TestWorld
{
	float wall_z = 1e9f;
};

static int test_raycast(void *user, const float o[3], const float d[3], skate_hit *hit)
{
	TestWorld *w = (TestWorld *)user;
	float best = 2.0f;
	float n[3] = {0, 0, 0};
	if (d[1] < 0 && o[1] >= 0)
	{
		float t = o[1] / -d[1];
		if (t <= 1 && t < best) { best = t; n[0] = 0; n[1] = 1; n[2] = 0; }
	}
	if (d[2] > 0 && o[2] <= w->wall_z)
	{
		float t = (w->wall_z - o[2]) / d[2];
		if (t <= 1 && t < best) { best = t; n[0] = 0; n[1] = 0; n[2] = -1; }
	}
	if (best > 1)
		return 0;
	for (int i = 0; i < 3; ++i)
	{
		hit->point[i] = o[i] + d[i] * best;
		hit->normal[i] = n[i];
	}
	hit->fraction = best;
	hit->surface = 0;
	return 1;
}

static Recognizer tre_recognizer()
{
	Recognizer r;
	r.init({Pattern{"360Flip", {{-0.965714f, 0.268571f}, {-0.497143f, 0.84f}, {0.211429f, 0.988571f}, {0.908571f, -0.405714f}}, 0.4f * 0.4f}});
	return r;
}
static const GestureSettings SETTINGS{3, 0};

static void test_gesture_vectors()
{
	// skate-core gesture.rs: anticipation_hold_does_not_weaken_authored_scoop
	Recognizer r = tre_recognizer();
	auto pts = r.patterns()[0].points;
	r.sample({0, 0}, SETTINGS);
	bool none = true;
	for (int i = 0; i < 120; ++i)
		none = none && !r.sample(pts[0], SETTINGS);
	CHECK(none);
	CHECK(!r.sample(pts[1], SETTINGS));
	CHECK(!r.sample(pts[2], SETTINGS));
	auto res = r.sample(pts[3], SETTINGS);
	CHECK(res && res->pattern == 0 && res->strength == 1.0f && res->elapsed == 4.0f);
	CHECK(r.held(pts[3]) && *r.held(pts[3]) == 0);
	CHECK(!r.sample(pts[3], SETTINGS));
	CHECK(!r.held({0, 0}));

	// disconnected_scoop_and_reverse_path_do_not_trigger
	Recognizer a = tre_recognizer();
	a.sample({0, 0}, SETTINGS);
	a.sample(pts[0], SETTINGS);
	bool quiet = true;
	for (int i = 0; i < 4; ++i)
		quiet = quiet && !a.sample({0, 0}, SETTINGS);
	for (size_t i = 1; i < pts.size(); ++i)
		quiet = quiet && !a.sample(pts[i], SETTINGS);
	CHECK(quiet);
	Recognizer b = tre_recognizer();
	b.sample({0, 0}, SETTINGS);
	bool rev = true;
	for (int i = (int)pts.size() - 1; i >= 0; --i)
		rev = rev && !b.sample(pts[i], SETTINGS);
	CHECK(rev);
}

static void test_mapping()
{
	// gesture_mapping.rs: tre_flip_uses_original_mirror_mapping_and_presence_not_strength
	CHECK(!std::strcmp(trick_select("360Flip", false, false, false), "360Flip"));
	CHECK(!std::strcmp(trick_select("360Flip", true, false, false), "Laserflip"));
	CHECK(!std::strcmp(trick_select("360Flip", true, false, true), "360Flip"));
	CHECK(!std::strcmp(trick_hold_successor("Kickflip"), "KickflipHold"));
	CHECK(!std::strcmp(trick_select("PopShuvit", false, true, false), "Kickflip")); // DarkCatch cat 1
	CHECK(!std::strcmp(trick_select("FSPopShuvit", false, true, false), "Heelflip"));
	// every authored pattern maps to a trick with a flip spec, and parses
	auto pats = parse_patterns(authored_right_stick_patterns());
	CHECK(pats.size() == 16);
	for (auto &p : pats)
	{
		const char *t = trick_select(p.name.c_str(), false, false, false);
		CHECK(t && trick_flip_spec(t));
		CHECK(std::fabs(p.tolerance_squared - 0.16f) < 1e-6f);
	}
}

// Drive the right stick through points with a few in-between samples (a real flick).
static std::vector<std::pair<float, float>> path(std::vector<std::pair<float, float>> pts, int hold_first = 8)
{
	std::vector<std::pair<float, float>> out;
	for (int i = 0; i < hold_first; ++i)
		out.push_back(pts[0]);
	for (size_t i = 1; i < pts.size(); ++i)
	{
		for (int k = 1; k <= 2; ++k)
		{
			float t = k / 2.0f;
			out.push_back({pts[i - 1].first + (pts[i].first - pts[i - 1].first) * t,
				pts[i - 1].second + (pts[i].second - pts[i - 1].second) * t});
		}
	}
	return out;
}

struct Rig
{
	TestWorld tw;
	skate_world *w;
	skate_skater *s;
	std::vector<skate_event> ev;
	Rig()
	{
		w = skate_world_new(test_raycast, &tw);
		s = skate_skater_new();
		float p[3] = {0, 0.3f, 0}, v[3] = {0, 0, 0};
		skate_skater_enter(s, p, v, 0);
	}
	~Rig()
	{
		skate_skater_delete(s);
		skate_world_delete(w);
	}
	void step(skate_input in = {}, int n = 1)
	{
		for (int i = 0; i < n; ++i)
		{
			skate_skater_step(s, w, &in);
			skate_event e;
			while (skate_skater_poll_event(s, &e))
				ev.push_back(e);
		}
	}
	// right stick in y-up space: down = (0,-1)
	void flick(std::vector<std::pair<float, float>> pts, unsigned buttons = 0)
	{
		for (auto p : path(pts))
		{
			skate_input in{};
			in.right_stick[0] = p.first;
			in.right_stick[1] = p.second;
			in.buttons = buttons;
			step(in);
		}
	}
	skate_pose pose()
	{
		skate_pose p;
		skate_skater_get_pose(s, &p);
		return p;
	}
	bool has(skate_event_type t, const char *name = nullptr)
	{
		for (auto &e : ev)
			if (e.type == t && (!name || std::strstr(e.name, name)))
				return true;
		return false;
	}
	void roll(float speed_target)
	{
		skate_input in{};
		in.buttons = SKATE_BUTTON_PUSH;
		for (int i = 0; i < 600 && std::hypot(pose().velocity[0], pose().velocity[2]) < speed_target; ++i)
			step(in);
	}
};

static void test_lands_and_pushes()
{
	Rig r;
	r.step({}, 60);
	CHECK(r.pose().state == SKATE_STATE_ROLLING);
	CHECK(std::fabs(r.pose().position[1]) < 0.01f);
	r.roll(5.0f);
	float v = r.pose().velocity[2];
	CHECK(v > 4.5f && v < 9.5f); // pushed forward (+Z), below the cap
	skate_input in{};
	in.buttons = SKATE_BUTTON_PUSH;
	r.step(in, 1200);
	CHECK(r.pose().velocity[2] <= 9.05f); // capped
}

static void test_ollie_by_flick()
{
	Rig r;
	r.step({}, 30);
	r.roll(5.0f);
	r.flick({{0, -1}, {0, 1}}); // pull back, flick forward
	CHECK(r.has(SKATE_EVENT_POP, "Ollie"));
	CHECK(r.pose().state == SKATE_STATE_AIR);
	float peak = 0;
	for (int i = 0; i < 120; ++i)
	{
		r.step();
		peak = std::fmax(peak, r.pose().position[1]);
	}
	CHECK(peak > 0.4f && peak < 1.4f);
	CHECK(r.pose().state == SKATE_STATE_ROLLING);
	CHECK(r.has(SKATE_EVENT_LANDED));
	CHECK(r.has(SKATE_EVENT_TRICK, "Ollie"));
}

static void test_kickflip_and_tre()
{
	Rig r;
	r.step({}, 30);
	r.roll(5.0f);
	r.flick({{0, -1}, {-0.707f, 0.707f}}); // back, then up-left
	CHECK(r.has(SKATE_EVENT_POP, "Kickflip"));
	r.step({}, 120);
	CHECK(r.has(SKATE_EVENT_TRICK, "Kickflip"));
	CHECK(!r.has(SKATE_EVENT_BAIL));

	Rig t;
	t.step({}, 30);
	t.roll(5.0f);
	// skate-core's 360 flip vector, in y-up stick space (y negated back)
	t.flick({{-0.965714f, -0.268571f}, {-0.497143f, -0.84f}, {0.211429f, -0.988571f}, {0.908571f, 0.405714f}});
	CHECK(t.has(SKATE_EVENT_POP, "360Flip"));
	t.step({}, 120);
	CHECK(t.has(SKATE_EVENT_TRICK, "360 Flip"));
}

static void test_spin_landing_and_bail()
{
	Rig r;
	r.step({}, 30);
	r.roll(5.0f);
	r.flick({{0, -1}, {0, 1}});
	// spin ~90 degrees and land sideways: bail
	skate_input in{};
	in.left_stick[0] = 1.0f;
	r.step(in, 15);
	r.step({}, 120);
	CHECK(r.has(SKATE_EVENT_BAIL));
	r.step({}, 120);
	CHECK(r.pose().state == SKATE_STATE_ROLLING); // got back up
}

static void test_wall_bail()
{
	Rig r;
	r.tw.wall_z = 30.0f;
	r.step({}, 30);
	r.roll(8.5f);
	r.step({}, 300);
	CHECK(r.has(SKATE_EVENT_BAIL, "wall"));
}

static void test_grind()
{
	Rig r;
	float rail[6] = {0, 0.45f, 4, 0, 0.45f, 30};
	CHECK(skate_world_add_rail(r.w, rail, 2) == 0);
	r.step({}, 30);
	r.roll(5.0f);
	r.flick({{0, -1}, {0, 1}});
	r.step({}, 60);
	CHECK(r.has(SKATE_EVENT_GRIND, "50-50"));
	CHECK(r.pose().state == SKATE_STATE_GRIND || r.has(SKATE_EVENT_TRICK, "50-50"));
	// rides off the far end instead of sticking to it
	r.step({}, 600);
	CHECK(r.pose().state != SKATE_STATE_GRIND);
	CHECK(r.pose().position[2] > 29.0f || r.pose().state == SKATE_STATE_ROLLING);
}

static void test_manual()
{
	Rig r;
	r.step({}, 30);
	r.roll(5.0f);
	skate_input in{};
	in.buttons = SKATE_BUTTON_MANUAL;
	r.step(in, 30);
	CHECK(r.pose().state == SKATE_STATE_MANUAL);
	CHECK(r.has(SKATE_EVENT_TRICK, "Manual"));
	r.step({}, 5);
	CHECK(r.pose().state == SKATE_STATE_ROLLING);
}


// ---------- grind_world/derive.rs tests, ported
using Tri = std::array<std::array<float, 3>, 3>;
static void quad(std::vector<Tri> &out, std::array<std::array<float, 3>, 4> q)
{
	out.push_back({q[0], q[1], q[2]});
	out.push_back({q[0], q[2], q[3]});
}
static void floor_(std::vector<Tri> &out, float half)
{
	quad(out, {{{-half, 0, -half}, {-half, 0, half}, {half, 0, half}, {half, 0, -half}}});
}
// Closed box, faces wound outward.
static void solid(std::vector<Tri> &out, std::array<float, 3> mn, std::array<float, 3> mx)
{
	float x0 = mn[0], y0 = mn[1], z0 = mn[2], x1 = mx[0], y1 = mx[1], z1 = mx[2];
	quad(out, {{{x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}}});
	quad(out, {{{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}}});
	quad(out, {{{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}}});
	quad(out, {{{x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}}});
	quad(out, {{{x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}}});
	quad(out, {{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}});
}
static float span(const std::vector<Vec3> &line)
{
	float s = 0;
	for (size_t i = 0; i + 1 < line.size(); ++i)
		s += line[i].distance(line[i + 1]);
	return s;
}

static void test_derive()
{
	{
		// flat_ground_and_gentle_banks_have_no_edges
		std::vector<Tri> t;
		floor_(t, 10);
		quad(t, {{{0, 0, -2}, {0, 0, 2}, {1.732f, 1, 2}, {1.732f, 1, -2}}});
		quad(t, {{{1.732f, 1, -2}, {1.732f, 1, 2}, {5, 1, 2}, {5, 1, -2}}});
		auto found = derive_rail_lines(t);
		CHECK(!found.empty());
		bool ok = true;
		for (auto &l : found)
			for (auto &p : l)
				ok = ok && (p.x > 4.99f || std::fabs(p.z) > 1.99f);
		CHECK(ok);
	}
	{
		// ledge_box_gives_its_four_top_edges
		std::vector<Tri> t;
		floor_(t, 10);
		solid(t, {-2, 0, -0.5f}, {2, 0.5f, 0.5f});
		auto found = derive_rail_lines(t);
		CHECK(found.size() == 4);
		bool ok = true;
		int four = 0, one = 0;
		for (auto &l : found)
		{
			ok = ok && l.size() == 2;
			for (auto &p : l)
				ok = ok && p.y == 0.5f;
			four += std::fabs(span(l) - 4) < 1e-4f;
			one += std::fabs(span(l) - 1) < 1e-4f;
		}
		CHECK(ok && four == 2 && one == 2);
		// winding_does_not_matter
		for (auto &tri : t)
			std::swap(tri[1], tri[2]);
		CHECK(derive_rail_lines(t).size() == 4);
	}
	{
		// stair_noses_are_not_rails_but_the_drop_behind_is
		std::vector<Tri> t;
		floor_(t, 10);
		for (int step = 0; step < 3; ++step)
		{
			float s = (float)step;
			solid(t, {-1, 0, s * 0.3f}, {1, 0.15f * (s + 1), s * 0.3f + 0.3f});
		}
		auto found = derive_rail_lines(t);
		CHECK(found.size() == 1);
		bool ok = !found.empty();
		for (auto &l : found)
			for (auto &p : l)
				ok = ok && std::fabs(p.y - 0.45f) < 1e-5f && std::fabs(p.z - 0.9f) < 1e-5f;
		CHECK(ok);
	}
	{
		// thin_bar_becomes_one_centred_rail
		std::vector<Tri> t;
		floor_(t, 10);
		solid(t, {-2, 0.35f, -0.05f}, {2, 0.45f, 0.05f});
		auto found = derive_rail_lines(t);
		CHECK(found.size() == 1);
		bool ok = found.size() == 1 && std::fabs(span(found[0]) - 4) < 1e-4f;
		for (auto &l : found)
			for (auto &p : l)
				ok = ok && std::fabs(p.z) < 1e-5f && std::fabs(p.y - 0.45f) < 1e-5f;
		CHECK(ok);
	}
	{
		// wide_bar_keeps_both_edges (no own rails here)
		std::vector<Tri> t;
		floor_(t, 10);
		solid(t, {-2, 0.35f, -0.15f}, {2, 0.45f, 0.15f});
		auto found = derive_rail_lines(t);
		int long_ = 0;
		bool ok = true;
		for (auto &l : found)
			if (span(l) > 3.9f)
			{
				++long_;
				for (auto &p : l)
					ok = ok && std::fabs(std::fabs(p.z) - 0.15f) < 1e-5f;
			}
		CHECK(long_ == 2 && ok);
	}
}

int main()
{
	test_derive();
	test_gesture_vectors();
	test_mapping();
	test_lands_and_pushes();
	test_ollie_by_flick();
	test_kickflip_and_tre();
	test_spin_landing_and_bail();
	test_wall_bail();
	test_grind();
	test_manual();
	std::printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
