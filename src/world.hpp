// Collision adapter (host raycasts) and grind rails.
#pragma once
#include "../include/skate_api.h"
#include "math.hpp"
#include <array>
#include <optional>
#include <vector>

namespace skate {

struct Hit
{
	Vec3 point, normal;
	float distance;
	int surface;
};

struct RailSample
{
	Vec3 point, tangent;
	float distance;
	unsigned rail;
	size_t segment;
	bool at_end; // clamped to the first or last point of the rail
};

class World
{
public:
	World(skate_raycast_proc raycast, void *user) : raycast_(raycast), user_(user) {}

	// Nearest solid hit along unit dir within max metres.
	std::optional<Hit> raycast(Vec3 origin, Vec3 dir, float max) const;
	// Pushes a sphere out of solids using a fan of probes. Returns the corrected centre and
	// the steepest-into-motion contact normal, if any.
	Vec3 resolve_sphere(Vec3 center, float radius, std::optional<Vec3> *contact) const;

	unsigned add_rail(std::vector<Vec3> points);
	std::optional<RailSample> nearest_rail(Vec3 p, float max) const;
	void clear_rails() { rails_.clear(); }
	size_t rail_count() const { return rails_.size(); }
	const std::vector<Vec3> *rail(size_t i) const { return i < rails_.size() ? &rails_[i] : nullptr; }

	// Port of skate-game `grind_world/derive.rs`: rails from a raw triangle soup (Y-up metres).
	size_t derive_rails(const float *vertices, int vertex_count, const int *indices, int triangle_count);

private:
	skate_raycast_proc raycast_;
	void *user_;
	std::vector<std::vector<Vec3>> rails_;
};

// The derivation itself (Y-up metres in, rails out), for tests.
std::vector<std::vector<Vec3>> derive_rail_lines(const std::vector<std::array<std::array<float, 3>, 3>> &collision);

} // namespace skate
