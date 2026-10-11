// C ABI over the skate library.
#include "../include/skate_api.h"
#include "skater.hpp"
#include <cstring>

using namespace skate;

struct skate_world
{
	World world;
};
struct skate_skater
{
	Skater skater;
};

extern "C" {

skate_world *skate_world_new(skate_raycast_proc raycast, void *user)
{
	return new skate_world{World(raycast, user)};
}

void skate_world_delete(skate_world *w) { delete w; }

int skate_world_add_rail(skate_world *w, const float *points, int count)
{
	std::vector<Vec3> pts;
	for (int i = 0; i < count; ++i)
		pts.push_back({points[i * 3], points[i * 3 + 1], points[i * 3 + 2]});
	return (int)w->world.add_rail(std::move(pts));
}

int skate_world_derive_rails(skate_world *w, const float *vertices, int vertex_count, const int *indices, int triangle_count)
{
	return (int)w->world.derive_rails(vertices, vertex_count, indices, triangle_count);
}

int skate_world_rail_count(const skate_world *w) { return (int)w->world.rail_count(); }
void skate_world_clear_rails(skate_world *w) { w->world.clear_rails(); }

const float *skate_world_rail_points(const skate_world *w, int rail, int *count)
{
	const std::vector<Vec3> *r = rail >= 0 ? w->world.rail((size_t)rail) : nullptr;
	if (!r)
	{
		*count = 0;
		return nullptr;
	}
	*count = (int)r->size();
	static_assert(sizeof(Vec3) == sizeof(float) * 3, "Vec3 is three packed floats");
	return &(*r)[0].x;
}

int skate_world_nearest_rail(const skate_world *w, const float p[3], float max, float point[3], float tangent[3])
{
	auto r = w->world.nearest_rail({p[0], p[1], p[2]}, max);
	if (!r)
		return -1;
	point[0] = r->point.x;
	point[1] = r->point.y;
	point[2] = r->point.z;
	tangent[0] = r->tangent.x;
	tangent[1] = r->tangent.y;
	tangent[2] = r->tangent.z;
	return (int)r->rail;
}

skate_skater *skate_skater_new(void) { return new skate_skater{}; }
void skate_skater_delete(skate_skater *s) { delete s; }

void skate_skater_enter(skate_skater *s, const float p[3], const float v[3], float facing)
{
	s->skater.enter({p[0], p[1], p[2]}, {v[0], v[1], v[2]}, facing);
}

void skate_skater_step(skate_skater *s, skate_world *w, const skate_input *in)
{
	s->skater.step(w->world, *in);
}

void skate_skater_get_pose(const skate_skater *s_, skate_pose *p)
{
	const Skater &s = s_->skater;
	std::memset(p, 0, sizeof(*p));
	p->position[0] = s.pos.x;
	p->position[1] = s.pos.y;
	p->position[2] = s.pos.z;
	p->velocity[0] = s.vel.x;
	p->velocity[1] = s.vel.y;
	p->velocity[2] = s.vel.z;
	Quat q = s.board_rotation();
	p->board_rotation[0] = q.x;
	p->board_rotation[1] = q.y;
	p->board_rotation[2] = q.z;
	p->board_rotation[3] = q.w;
	p->heading = s.board_yaw();
	Vec3 h{s.vel.x, 0, s.vel.z};
	p->travel_heading = h.length() > 0.3f ? std::atan2(h.x, h.z) : s.heading;
	p->crouch = s.crouch;
	p->lean = s.lean;
	p->state = (int)s.state;
	p->fakie = s.fakie;
	p->combo_value = s.combo.value;
	p->combo_multiplier = s.combo.multiplier;
	p->banked = s.combo.banked;
}

void skate_skater_force_bail(skate_skater *s) { s->skater.force_bail(); }

void skate_skater_add_bonus(skate_skater *s, const char *name, unsigned points)
{
	s->skater.add_bonus(name, points);
}

int skate_skater_poll_event(skate_skater *s, skate_event *e)
{
	auto &q = s->skater.events;
	if (q.empty())
		return 0;
	const Event &ev = q.front();
	e->type = ev.type;
	std::strncpy(e->name, ev.name.c_str(), sizeof(e->name) - 1);
	e->name[sizeof(e->name) - 1] = 0;
	e->points = ev.points;
	e->value = ev.value;
	q.pop_front();
	return 1;
}

const char *skate_skater_current_trick(const skate_skater *s) { return s->skater.current_trick(); }

const char *skate_gesture_debug_feed(skate_skater *s, float x, float y) { return s->skater.debug_feed(x, y); }

} // extern "C"
