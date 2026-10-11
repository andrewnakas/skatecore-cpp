// Little-endian SKATE01..15 reader (skate-data skate_map.rs + storage_v15.rs + texture_decode.rs),
// RWCM retail collision archives (retail_collision.rs) and spawn grounding (spawn_ground.rs).
// Layout follows SK8R15/Source/tools/blender_owned_map/SKATE_FORMAT.md and
// owned/world/src/owned_map_package.cpp. No geometry or physics is inferred.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

using Vec3f = std::array<float, 3>;

struct Material
{
	std::string name;
	uint32_t flags = 0;
	float friction = 0;
	float restitution = 0;
	Vec3f color{};
	float roughness = 0;
	float emissive = 0;
	/// Albedo, indirect, normal, ORM, emissive. One-based, zero absent.
	std::array<uint32_t, 5> textures{};
	float indirect_strength = 0;
	uint32_t alpha_mode = 0;
	float alpha_cutoff = 0.5f;
	uint32_t audio = 3;
	uint32_t physics = 1;
	uint32_t pattern = 0;
	std::optional<uint32_t> depth_layer;
	/// Complete v12+ retail definition bytes; distinct from portable PBR fields.
	std::optional<std::vector<uint8_t>> retail_definition;
};

struct Texture
{
	std::string name;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t color_space = 0;
	std::vector<uint8_t> rgba;
};

struct Vertex
{
	Vec3f position{};
	Vec3f normal{};
	std::array<float, 2> uv{};
	std::array<float, 2> lightmap_uv{};
	uint32_t material = 0;
	std::optional<std::array<float, 2>> decal_uv;
	std::optional<std::array<uint8_t, 4>> tangent_frame;
};

struct Collision
{
	std::array<Vec3f, 3> points{};
	uint32_t surface = 0;
	uint32_t material = 0;
	std::optional<std::array<uint8_t, 3>> native_edges;
};

struct Geometry
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Collision> collision;
};

struct Rail
{
	std::string name;
	bool closed = false;
	std::vector<Vec3f> points;
	std::optional<std::vector<uint8_t>> native;
};

struct Door
{
	std::string name;
	/// Hinge position/axis, closed width/depth axes, local min/max.
	std::array<Vec3f, 6> frame{};
	/// Minimum/maximum/initial angle, mass, damping.
	std::array<float, 5> motion{};
	/// Spring, max angular speed, contact impulse scale (absent in v4).
	std::optional<Vec3f> response;
	float friction = 0;
	float restitution = 0;
	uint32_t surface = 0;
	Geometry geometry;
};

struct Light
{
	std::string name;
	uint32_t kind = 0;
	Vec3f position{};
	Vec3f direction{};
	Vec3f color{};
	float intensity = 0;
	float range = 0;
	float radius = 0;
	float inner_cos = 0;
	float outer_cos = 0;
};

struct Route
{
	Rail rail;
	uint32_t skaters = 0;
	float speed = 0;
	float spacing = 0;
};

struct Extension
{
	std::array<uint8_t, 4> tag{};
	uint32_t schema = 0;
	std::vector<uint8_t> payload;
	bool tag_is(const char *t) const { return tag[0] == (uint8_t)t[0] && tag[1] == (uint8_t)t[1] && tag[2] == (uint8_t)t[2] && tag[3] == (uint8_t)t[3]; }
};

struct SkateMap
{
	uint8_t version = 0;
	std::string name;
	Vec3f spawn{};
	float heading = 0;
	/// Authored environment fields in wire order (12, 14, or 45 floats).
	std::vector<float> environment;
	std::vector<Material> materials;
	std::vector<Texture> textures;
	Geometry geometry;
	std::vector<Rail> rails;
	std::vector<Door> doors;
	std::vector<Light> lights;
	std::vector<Route> routes;
	std::vector<Extension> extensions;

	// `path` is a VFS path such as "maps/KenneyPark.skate". (The Rust timing logs are not printed.)
	static Result<SkateMap> load(const Vfs &vfs, const std::string &path);
	static Result<SkateMap> parse(const uint8_t *data, size_t size);
	/// Data tools can request one worker for reproducible serial comparisons.
	/// (This port always decodes serially; the result is identical.)
	static Result<SkateMap> parse_with_decode_workers(const uint8_t *data, size_t size, size_t workers);
	/// Decode a supplemental presentation package without requiring collision.
	static Result<SkateMap> parse_render_only(const uint8_t *data, size_t size);
};

/// MOBJ schema 3 stores editor ownership of ranges in the base geometry.
/// Physics-disabled objects can use those unchanged static arrays. Explicit
/// body types still require a runtime adapter and must not be flattened.
Status validate_static_objects(const SkateMap &map, const Extension &extension);

namespace skate_map_storage {
/// Borrowed compressed payload (`StoredBlock`): method 0 raw, 1 zlib, 2 zstd,
/// 3..10 SKATE15 transforms.
struct StoredBlock
{
	size_t expected = 0;
	uint32_t method = 0;
	const uint8_t *bytes = nullptr;
	size_t size = 0;
	Result<std::vector<uint8_t>> decode() const;
};
/// Reversible SKATE15 storage transforms, matching the bundled exporter.
Result<std::vector<uint8_t>> decode_v15(uint32_t method, const uint8_t *bytes, size_t size, size_t expected);
/// Bounded texture decompression (serial port of texture_decode::decode); returns the
/// worker count the Rust decoder would report.
Result<size_t> decode_textures(std::vector<Texture> &textures, const std::vector<StoredBlock> &blocks, size_t workers);
} // namespace skate_map_storage

// ---- retail_collision.rs ----
//! Embedded RWCM v1 / RWCMSET1 archives. Cluster coordinates are already in
//! world space. Never weld them or replace their feature bytes with adjacency.

struct RetailTriangle
{
	std::array<Vec3f, 3> points{};
	std::optional<std::array<uint8_t, 3>> edges;
	uint16_t surface = 0;
	uint16_t group = 0;
	bool one_sided = false;
};

namespace retail_collision {
/// Visits complete clusters in archive order, retaining only one decoded
/// cluster at a time. The callback can build its own spatial query index.
Result<size_t> visit_clusters(const uint8_t *data, size_t size,
                              const std::function<Status(const std::string &, const std::vector<RetailTriangle> &)> &visit);
} // namespace retail_collision

// ---- spawn_ground.rs ----
namespace spawn_ground {
/// `None` when there is walkable ground within 200 m below `point`; otherwise
/// the point 1 m above the centre of the nearest walkable triangle.
Result<std::optional<Vec3f>> grounded_spawn(const SkateMap &map, Vec3f point);
} // namespace spawn_ground

} // namespace s3::data
