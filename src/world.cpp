#include "world.hpp"
#include <cmath>

namespace skate {

std::optional<Hit> World::raycast(Vec3 origin, Vec3 dir, float max) const
{
	if (!raycast_)
		return std::nullopt;
	skate_hit h{};
	float o[3] = {origin.x, origin.y, origin.z};
	float d[3] = {dir.x * max, dir.y * max, dir.z * max};
	if (!raycast_(user_, o, d, &h))
		return std::nullopt;
	Hit out;
	out.point = {h.point[0], h.point[1], h.point[2]};
	out.normal = Vec3{h.normal[0], h.normal[1], h.normal[2]}.normalize_or_zero();
	out.distance = h.fraction * max;
	out.surface = h.surface;
	return out;
}

Vec3 World::resolve_sphere(Vec3 center, float radius, std::optional<Vec3> *contact) const
{
	// 8 horizontal probes, down, up, and 4 diagonals downward: enough for a board-sized
	// sphere against level geometry. Two relaxation passes.
	static const Vec3 dirs[] = {
		{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
		{0.7071f, 0, 0.7071f}, {-0.7071f, 0, 0.7071f}, {0.7071f, 0, -0.7071f}, {-0.7071f, 0, -0.7071f},
		{0, -1, 0}, {0, 1, 0},
		{0.7071f, -0.7071f, 0}, {-0.7071f, -0.7071f, 0}, {0, -0.7071f, 0.7071f}, {0, -0.7071f, -0.7071f},
	};
	if (contact)
		contact->reset();
	float deepest = 0.0f;
	for (int pass = 0; pass < 2; ++pass)
	{
		for (const Vec3 &d : dirs)
		{
			auto h = raycast(center, d, radius);
			if (!h)
				continue;
			float depth = radius - h->distance;
			if (depth <= 0.0f)
				continue;
			// Only push along the surface normal if it faces the probe.
			if (h->normal.dot(d) >= 0.0f)
				continue;
			float push = depth * -h->normal.dot(d);
			center += h->normal * push;
			if (contact && depth > deepest)
			{
				deepest = depth;
				*contact = h->normal;
			}
		}
	}
	return center;
}

unsigned World::add_rail(std::vector<Vec3> points)
{
	rails_.push_back(std::move(points));
	return (unsigned)(rails_.size() - 1);
}

std::optional<RailSample> World::nearest_rail(Vec3 p, float max) const
{
	std::optional<RailSample> best;
	for (size_t r = 0; r < rails_.size(); ++r)
	{
		const auto &pts = rails_[r];
		for (size_t i = 0; i + 1 < pts.size(); ++i)
		{
			Vec3 a = pts[i], b = pts[i + 1];
			Vec3 ab = b - a;
			float len2 = ab.dot(ab);
			if (len2 < 1e-8f)
				continue;
			float raw = (p - a).dot(ab) / len2;
			float t = clampf(raw, 0.0f, 1.0f);
			Vec3 q = a + ab * t;
			float d = q.distance(p);
			bool end = (i == 0 && raw <= 0.0f) || (i + 2 == pts.size() && raw >= 1.0f);
			if (d <= max && (!best || d < best->distance))
				best = RailSample{q, ab.normalize_or_zero(), d, (unsigned)r, i, end};
		}
	}
	return best;
}

} // namespace skate
