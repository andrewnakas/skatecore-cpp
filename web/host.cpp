// Web demo host: the skate library on a triangle-soup skatepark, driven from JavaScript.
// The page builds the park (Y-up metres), hands its triangles here, and steps the skater at 60 Hz.
#include "../include/skate_api.h"
#include <emscripten/emscripten.h>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

struct Tri
{
	float a[3], b[3], c[3];
	float n[3];
	float lo[3], hi[3];
};

std::vector<Tri> tris;
skate_world *world = nullptr;
skate_skater *skater = nullptr;

// Möller–Trumbore against every triangle whose box the ray's box touches; parks are small.
int raycast(void *, const float o[3], const float d[3], skate_hit *hit)
{
	float rlo[3], rhi[3];
	for (int i = 0; i < 3; ++i)
	{
		rlo[i] = std::fmin(o[i], o[i] + d[i]);
		rhi[i] = std::fmax(o[i], o[i] + d[i]);
	}
	float best = 2.0f;
	const Tri *hit_tri = nullptr;
	for (const Tri &t : tris)
	{
		if (t.hi[0] < rlo[0] || t.lo[0] > rhi[0] || t.hi[1] < rlo[1] || t.lo[1] > rhi[1] || t.hi[2] < rlo[2] || t.lo[2] > rhi[2])
			continue;
		float e1[3] = {t.b[0] - t.a[0], t.b[1] - t.a[1], t.b[2] - t.a[2]};
		float e2[3] = {t.c[0] - t.a[0], t.c[1] - t.a[1], t.c[2] - t.a[2]};
		float p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
		float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
		if (std::fabs(det) < 1e-9f)
			continue;
		float inv = 1.0f / det;
		float s[3] = {o[0] - t.a[0], o[1] - t.a[1], o[2] - t.a[2]};
		float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
		if (u < 0 || u > 1)
			continue;
		float q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
		float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
		if (v < 0 || u + v > 1)
			continue;
		float f = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
		if (f < 0 || f > 1 || f >= best)
			continue;
		best = f;
		hit_tri = &t;
	}
	if (!hit_tri)
		return 0;
	// one-sided answer: the normal faces back along the ray
	float n[3] = {hit_tri->n[0], hit_tri->n[1], hit_tri->n[2]};
	if (n[0] * d[0] + n[1] * d[1] + n[2] * d[2] > 0)
		for (float &x : n)
			x = -x;
	for (int i = 0; i < 3; ++i)
	{
		hit->point[i] = o[i] + d[i] * best;
		hit->normal[i] = n[i];
	}
	hit->fraction = best;
	hit->surface = 0;
	return 1;
}

skate_event last_event;

} // namespace

extern "C" {

// vertices: xyz per vertex; indices: 3 per triangle. Returns the number of grind rails found.
EMSCRIPTEN_KEEPALIVE int demo_set_world(const float *v, int vertex_count, const int *idx, int triangle_count)
{
	tris.clear();
	for (int t = 0; t < triangle_count; ++t)
	{
		Tri tri;
		float *dst[3] = {tri.a, tri.b, tri.c};
		for (int k = 0; k < 3; ++k)
			std::memcpy(dst[k], &v[idx[t * 3 + k] * 3], sizeof(float) * 3);
		float e1[3] = {tri.b[0] - tri.a[0], tri.b[1] - tri.a[1], tri.b[2] - tri.a[2]};
		float e2[3] = {tri.c[0] - tri.a[0], tri.c[1] - tri.a[1], tri.c[2] - tri.a[2]};
		float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
		float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if (l < 1e-9f)
			continue;
		for (int i = 0; i < 3; ++i)
		{
			tri.n[i] = n[i] / l;
			tri.lo[i] = std::fmin(tri.a[i], std::fmin(tri.b[i], tri.c[i])) - 1e-4f;
			tri.hi[i] = std::fmax(tri.a[i], std::fmax(tri.b[i], tri.c[i])) + 1e-4f;
		}
		tris.push_back(tri);
	}
	if (!world)
		world = skate_world_new(raycast, nullptr);
	if (!skater)
		skater = skate_skater_new();
	skate_world_clear_rails(world);
	return skate_world_derive_rails(world, v, vertex_count, idx, triangle_count);
}

EMSCRIPTEN_KEEPALIVE void demo_spawn(float x, float y, float z, float heading)
{
	float p[3] = {x, y, z}, v[3] = {0, 0, 0};
	skate_skater_enter(skater, p, v, heading);
}

EMSCRIPTEN_KEEPALIVE void demo_step(float lx, float ly, float rx, float ry, unsigned buttons)
{
	skate_input in{};
	in.left_stick[0] = lx;
	in.left_stick[1] = ly;
	in.right_stick[0] = rx;
	in.right_stick[1] = ry;
	in.buttons = buttons;
	skate_skater_step(skater, world, &in);
}

// out[0..2] position, [3..6] board quaternion, [7] heading, [8] travel heading, [9] crouch, [10] lean,
// [11] state, [12] fakie, [13] combo value, [14] combo multiplier, [15] banked, [16..18] velocity
EMSCRIPTEN_KEEPALIVE void demo_pose(float *out)
{
	skate_pose p;
	skate_skater_get_pose(skater, &p);
	std::memcpy(out, p.position, sizeof(float) * 3);
	std::memcpy(out + 3, p.board_rotation, sizeof(float) * 4);
	out[7] = p.heading;
	out[8] = p.travel_heading;
	out[9] = p.crouch;
	out[10] = p.lean;
	out[11] = (float)p.state;
	out[12] = (float)p.fakie;
	out[13] = (float)p.combo_value;
	out[14] = (float)p.combo_multiplier;
	out[15] = (float)p.banked;
	std::memcpy(out + 16, p.velocity, sizeof(float) * 3);
}

// Returns the event type + 1, or 0 when none; name/points via demo_event_name/demo_event_points.
EMSCRIPTEN_KEEPALIVE int demo_poll_event()
{
	if (!skate_skater_poll_event(skater, &last_event))
		return 0;
	return (int)last_event.type + 1;
}
EMSCRIPTEN_KEEPALIVE const char *demo_event_name() { return last_event.name; }
EMSCRIPTEN_KEEPALIVE unsigned demo_event_points() { return last_event.points; }
EMSCRIPTEN_KEEPALIVE int demo_event_value() { return last_event.value; }
EMSCRIPTEN_KEEPALIVE const char *demo_current_trick() { return skate_skater_current_trick(skater); }
EMSCRIPTEN_KEEPALIVE void demo_bail() { skate_skater_force_bail(skater); }

EMSCRIPTEN_KEEPALIVE int demo_rail_count() { return skate_world_rail_count(world); }
EMSCRIPTEN_KEEPALIVE const float *demo_rail_points(int rail, int *count) { return skate_world_rail_points(world, rail, count); }

} // extern "C"
