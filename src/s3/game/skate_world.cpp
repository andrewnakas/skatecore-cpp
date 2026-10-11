// Port of skate-game/src/skate_world.rs (simulation half, as kept by simref).
#include "s3/game/skate_world.hpp"

#include <map>
#include <optional>
#include <vector>

namespace s3::game::skate_world {

using sc::math::Vector3;
using sc::physics::board_world::BoardWorld;
using sc::physics::board_world::WorldTriangle;
using sc::physics::board_world::query_metadata::Bounds;
using sc::physics::board_world::query_metadata::QueryMesh;
using sc::physics::board_world::query_metadata::QueryMetadata;
using sc::physics::board_world::query_metadata::QueryPool;
using sc::physics::board_world::query_metadata::Range;
using sc::physics::collision::TriangleFeature;
using sc::physics::contact::RetailContactMaterial;
using sc::physics::drive_frames::RetailAffineTransform;

namespace {

/// glam 0.30 scalar `Vec3` (bevy re-export): the operations collision_world uses.
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    static Vec3 from_array(const std::array<float, 3>& a) { return Vec3{a[0], a[1], a[2]}; }
    Vec3 operator-(Vec3 r) const { return Vec3{x - r.x, y - r.y, z - r.z}; }
    Vec3 operator*(float s) const { return Vec3{x * s, y * s, z * s}; }
    float dot(Vec3 r) const { return (x * r.x) + (y * r.y) + (z * r.z); }
    Vec3 cross(Vec3 r) const {
        return Vec3{y * r.z - r.y * z, z * r.x - r.z * x, x * r.y - r.x * y};
    }
    float length() const { return rs::sqrt(dot(*this)); }
    float length_recip() const { return rs::recip(length()); }
    std::optional<Vec3> try_normalize() const {
        const float rcp = length_recip();
        if (rs::is_finite(rcp) && rcp > 0.0f) return *this * rcp;
        return std::nullopt;
    }
};

std::string utf8_lossy(const std::array<std::uint8_t, 4>& tag) {
    // Tags are ASCII in practice; invalid bytes become U+FFFD like from_utf8_lossy.
    std::string out;
    for (std::uint8_t c : tag) {
        if (c < 0x80) out.push_back(char(c));
        else out += "\xEF\xBF\xBD";
    }
    return out;
}

bool tag_eq(const std::array<std::uint8_t, 4>& tag, const char* t) {
    return tag[0] == std::uint8_t(t[0]) && tag[1] == std::uint8_t(t[1]) && tag[2] == std::uint8_t(t[2]) &&
           tag[3] == std::uint8_t(t[3]);
}

/// Rust `Option<&[u8]>`.
rs::Result<const std::vector<std::uint8_t>*, std::string> retail_archive(const sd::SkateMap& map) {
    const sd::Extension* first = nullptr;
    bool second = false;
    for (const auto& e : map.extensions) {
        if (!tag_eq(e.tag, "RWCM")) continue;
        if (first == nullptr) {
            first = &e;
        } else {
            second = true;
            break;
        }
    }
    if (first == nullptr) return static_cast<const std::vector<std::uint8_t>*>(nullptr);
    if (first->schema != 1 || second) {
        return rs::make_err(std::string("SKATE requires one RWCM extension with schema 1"));
    }
    return &first->payload;
}

std::string debug_vec3f(const sd::Vec3f& v) {
    return fmt::cat("[", fmt::debug_f32(v[0]), ", ", fmt::debug_f32(v[1]), ", ", fmt::debug_f32(v[2]), "]");
}

rs::Result<BoardWorld, std::string> retail_collision_world(const std::vector<std::uint8_t>& archive,
                                                           RetailContactMaterial material) {
    std::vector<WorldTriangle> triangles;
    std::vector<std::uint16_t> packed_surfaces;
    std::vector<QueryMesh> meshes;
    auto count = sd::retail_collision::visit_clusters(
        archive.data(), archive.size(),
        [&](const std::string&, const std::vector<sd::RetailTriangle>& cluster) -> sd::Status {
            // Preserve cluster order and partition further only for group filters.
            std::size_t cursor = 0;
            while (cursor < cluster.size()) {
                const std::uint16_t group = cluster[cursor].group;
                const std::size_t start = triangles.size();
                while (cursor < cluster.size() && cluster[cursor].group == group) {
                    const sd::RetailTriangle& source = cluster[cursor];
                    std::uint32_t flags = 0;
                    std::array<float, 3> cosines{};
                    if (source.edges.has_value()) {
                        auto decoded = decode_native_edges(*source.edges);
                        if (decoded.is_err()) return sd::Error{decoded.unwrap_err()};
                        flags = decoded.unwrap().first;
                        cosines = decoded.unwrap().second;
                        flags &= ~TriangleFeature::ONE_SIDED;
                        if (source.one_sided) flags |= TriangleFeature::ONE_SIDED;
                    } else {
                        // TriangleVolume ctor82AC7770 retains these defaults when
                        // the unit has no edge data; the mesh sidedness is not read.
                        flags = 0x1e1;
                        cosines = {-1.0f, -1.0f, -1.0f};
                    }
                    std::array<Vector3, 3> points{};
                    for (std::size_t k = 0; k < 3; ++k)
                        points[k] = Vector3::new_(source.points[k][0], source.points[k][1], source.points[k][2]);
                    auto t = WorldTriangle::from_vertices(points, material, std::uint32_t(source.surface), flags,
                                                          cosines, 0.0f);
                    if (!t) return sd::Error{"Invalid RWCM collision triangle"};
                    triangles.push_back(*t);
                    packed_surfaces.push_back(source.surface);
                    cursor += 1;
                }
                const Range range{start, triangles.size()};
                std::vector<Vector3> pts;
                for (std::size_t i = range.start; i < range.end; ++i)
                    for (const auto& v : triangles[i].triangle.vertices) pts.push_back(v);
                auto bounds = Bounds::from_points(pts);
                if (!bounds) return sd::Error{"Invalid RWCM cluster bounds"};
                QueryMesh mesh{};
                mesh.geometry = 0;
                mesh.rejection_flags = 0;
                mesh.triangle_range = range;
                mesh.local_to_world = RetailAffineTransform::IDENTITY;
                mesh.world_to_local = RetailAffineTransform::IDENTITY;
                mesh.local_bounds = *bounds;
                // Static registration `sub_82776B58` hands the mesh-record ctor
                // `sub_8276CB18` a hardcoded `li r8,-1` for matchingID (+180) (see Rust comment).
                mesh.matching_group = -1;
                mesh.pool = QueryPool::Ground;
                meshes.push_back(mesh);
            }
            return sd::Unit{};
        });
    if (!count.ok) return rs::make_err(count.error);
    log::eprintln(fmt::cat("SKATE_RWCM_READY triangles=", count.value, " query_clusters=", meshes.size(),
                           " source=embedded"));
    QueryMetadata metadata{};
    metadata.packed_surfaces = std::move(packed_surfaces);
    metadata.meshes = std::move(meshes);
    metadata.island_flags = 0;
    auto world = BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata));
    if (world.is_err()) return rs::make_err(std::string(world.unwrap_err()));
    return std::move(world.unwrap());
}

}  // namespace

Status validate_runtime(const sd::SkateMap& map) {
    S3G_TRY(const auto* archive, retail_archive(map));
    if (map.geometry.collision.empty() && archive == nullptr) {
        return rs::make_err(std::string("SKATE map has no collision geometry"));
    }
    if (archive == nullptr) {
        for (const auto& triangle : map.geometry.collision) {
            if (triangle.native_edges.has_value()) {
                S3G_CHECK(decode_native_edges(*triangle.native_edges));
            }
        }
    }
    if (!map.doors.empty()) {
        return rs::make_err(fmt::cat(
            "Map '", map.name, "' contains ", map.doors.size(),
            " hinged doors. This imported game has no door body/controller adapter yet; refusing to drop their "
            "geometry or turn them into static walls."));
    }
    for (const auto& extension : map.extensions) {
        const std::string tag = utf8_lossy(extension.tag);
        if (tag_eq(extension.tag, "RWCM")) continue;
        if (tag_eq(extension.tag, "MOBJ")) {
            S3G_CHECK(sd::validate_static_objects(map, extension));
            continue;
        }
        if (tag_eq(extension.tag, "SKYB") && extension.schema == 1) {
            log::eprintln(
                "SKATE LIMITATION: SKYB retail sky retained; using the map horizon until its shader adapter is "
                "available.");
            continue;
        }
        if (!tag_eq(extension.tag, "WMET") && !tag_eq(extension.tag, "WCFG") && !tag_eq(extension.tag, "BMAT")) {
            return rs::make_err(fmt::cat(
                "SKATE extension ", tag, " schema ", extension.schema,
                " is decoded but its runtime adapter is not implemented. Refusing to silently omit potentially "
                "required world geometry."));
        }
        log::eprintln(fmt::cat("SKATE LIMITATION: ", tag, " extension retained; its runtime behavior is not connected."));
    }
    bool retail_material = false;
    for (const auto& m : map.materials) retail_material = retail_material || m.retail_definition.has_value();
    if (retail_material) {
        log::eprintln(
            "SKATE_RENDER: retail world material adapter enabled; unsupported families retain portable PBR "
            "rendering.");
    }
    for (const auto& t : map.textures) {
        if (t.width == 0) {
            return rs::make_err(
                std::string("SKATE contains external texture placeholders; supply a package with embedded textures"));
        }
    }
    if (!map.routes.empty()) {
        log::eprintln(fmt::cat("SKATE LIMITATION: ", map.routes.size(),
                               " NPC routes parsed; supplied game has no NPC controller."));
    }
    for (const auto& l : map.lights) {
        if (l.kind == 2) {
            log::eprintln(
                "SKATE LIMITATION: area-light records retained; Bevy adapter currently renders point and spot "
                "lights only.");
            break;
        }
    }
    log::eprintln(
        "SKATE LIMITATION: native frame-lighting/day-night controller is not connected; using package lighting and "
        "retail district sky where available.");
    log::eprintln(fmt::cat("SKATE_MAP_LOADED name=\"", map.name, "\" version=", map.version,
                           " render_triangles=", map.geometry.indices.size() / 3,
                           " collision_triangles=", map.geometry.collision.size(), " textures=", map.textures.size(),
                           " spawn=", debug_vec3f(map.spawn)));
    return ok();
}

rs::Result<std::pair<std::uint32_t, std::array<float, 3>>, std::string> decode_native_edges(
    std::array<std::uint8_t, 3> edges) {
    std::uint32_t flags = 1u | TriangleFeature::ONE_SIDED | TriangleFeature::USE_EDGE_COSINES;
    std::array<float, 3> cosines{0.0f, 0.0f, 0.0f};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::uint8_t code = edges[i];
        const std::uint8_t exponent = code & 0x1f;
        // The native signed 32-bit shift becomes negative at 28 and zero
        // above it. Reject those malformed codes instead of producing NaNs.
        if (exponent >= 28) {
            return rs::make_err(
                fmt::cat("Invalid SKATE native edge angle code ", unsigned(exponent), " at corner ", i));
        }
        cosines[i] = 1.0f - rs::from_bits(0x411de9e7u) / float(8u << exponent);
        flags |= std::uint32_t(code & 0x20) << i;
        flags |= std::uint32_t(code & 0x40) << (i + 3);
    }
    return std::make_pair(flags, cosines);
}

rs::Result<BoardWorld, std::string> collision_world(const sd::SkateMap& map, RetailContactMaterial material) {
    S3G_TRY(const auto* archive, retail_archive(map));
    if (archive != nullptr) return retail_collision_world(*archive, material);
    // Match the reference RW mesh compiler's 1 mm vertex welding and reversed
    // edge pairing. Triangle diagonals are adjacency, never authored ledges.
    std::map<std::array<std::int64_t, 3>, std::size_t> welded;
    std::vector<Vec3> positions;
    std::vector<std::array<std::size_t, 3>> vertices;
    std::vector<Vec3> normals;
    for (const auto& tri : map.geometry.collision) {
        std::array<std::size_t, 3> ids{};
        for (std::size_t k = 0; k < 3; ++k) {
            const auto& p = tri.points[k];
            const double inverse = 1.0 / double(0.001f);
            std::array<std::int64_t, 3> key{};
            for (std::size_t j = 0; j < 3; ++j) {
                const double r = std::round(double(p[j]) * inverse);
                key[j] = std::isnan(r) ? 0 : static_cast<std::int64_t>(r);
            }
            auto it = welded.find(key);
            if (it == welded.end()) {
                const std::size_t id = positions.size();
                positions.push_back(Vec3::from_array(p));
                it = welded.emplace(key, id).first;
            }
            ids[k] = it->second;
        }
        const Vec3 a = Vec3::from_array(tri.points[0]);
        const Vec3 b = Vec3::from_array(tri.points[1]);
        const Vec3 c = Vec3::from_array(tri.points[2]);
        auto normal = (b - a).cross(c - a).try_normalize();
        if (!normal) return rs::make_err(std::string("Invalid SKATE collision triangle normal"));
        vertices.push_back(ids);
        normals.push_back(*normal);
    }
    std::vector<std::array<float, 3>> cosines(vertices.size(), std::array<float, 3>{1.0f, 1.0f, 1.0f});
    std::vector<std::uint32_t> flags(vertices.size(),
                                     TriangleFeature::ONE_SIDED | TriangleFeature::USE_EDGE_COSINES | 0xe0u);
    // Fully native maps need no reconstructed adjacency. Mixed maps still
    // include every face when finding neighbors for their authored geometry.
    bool any_portable = false;
    for (const auto& t : map.geometry.collision) any_portable = any_portable || !t.native_edges.has_value();
    if (any_portable) {
        std::map<std::pair<std::size_t, std::size_t>, std::pair<std::size_t, std::size_t>> open;
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto& ids = vertices[i];
            for (std::size_t edge = 0; edge < 3; ++edge) {
                const std::size_t a = ids[edge];
                const std::size_t b = ids[(edge + 1) % 3];
                auto found = open.find({b, a});
                if (found != open.end()) {
                    const auto [other, oe] = found->second;
                    open.erase(found);
                    const float cosine = rs::clamp(normals[i].dot(normals[other]), -1.0f, 1.0f);
                    const float orientation = (positions[b] - positions[a]).dot(normals[i].cross(normals[other]));
                    // ExtendedEdgeCosine / MakeEdgeCode in rw_collision_mesh.cpp:
                    // orientation >= -1e-6 is convex; flat edges have no convex bit.
                    const std::array<std::pair<std::size_t, std::size_t>, 2> pairs{
                        std::make_pair(i, edge), std::make_pair(other, oe)};
                    for (const auto& [ti, e] : pairs) {
                        cosines[ti][e] = cosine;
                        if (orientation <= -1.0e-6f || cosine >= 1.0f) {
                            flags[ti] &= ~(0x20u << e);
                        }
                    }
                } else {
                    open.emplace(std::make_pair(a, b), std::make_pair(i, edge));
                }
            }
        }
        std::vector<std::vector<std::size_t>> adjacent(positions.size());
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            for (std::size_t v : vertices[i]) adjacent[v].push_back(i);
        }
        for (std::size_t v = 0; v < adjacent.size(); ++v) {
            const auto& faces = adjacent[v];
            S3_ASSERT(!faces.empty());
            const Vec3 reference = normals[faces[0]];
            bool all = true;
            for (std::size_t i : faces) {
                if (!(rs::abs(reference.dot(normals[i]) - 1.0f) <= 0.01f)) {
                    all = false;
                    break;
                }
            }
            if (all) {
                for (std::size_t i : faces) {
                    for (std::size_t corner = 0; corner < 3; ++corner) {
                        if (vertices[i][corner] == v) flags[i] |= 0x200u << corner;
                    }
                }
            }
        }
    }
    std::vector<WorldTriangle> triangles;
    triangles.reserve(vertices.size());
    std::vector<std::uint16_t> packed_surfaces;
    packed_surfaces.reserve(vertices.size());
    for (std::size_t i = 0; i < map.geometry.collision.size(); ++i) {
        const auto& source = map.geometry.collision[i];
        if (source.native_edges.has_value()) {
            S3G_TRY(const auto decoded, decode_native_edges(*source.native_edges));
            flags[i] = decoded.first;
            cosines[i] = decoded.second;
        }
        S3_ASSERT(source.material >= 1 && std::size_t(source.material) - 1 < map.materials.size());
        const auto& m = map.materials[std::size_t(source.material) - 1];
        // Exact EncodeRwSurfaceId mapping from the reference native adapter.
        packed_surfaces.push_back(static_cast<std::uint16_t>(m.audio | (m.physics << 7) | (m.pattern << 12)));
        std::array<Vector3, 3> points{};
        for (std::size_t k = 0; k < 3; ++k) {
            const Vec3 p = positions[vertices[i][k]];
            points[k] = Vector3::new_(p.x, p.y, p.z);
        }
        // Keep the supplied game's original static-world contact combine values.
        auto t = WorldTriangle::from_vertices(points, material, source.surface, flags[i], cosines[i], 0.0f);
        if (!t) return rs::make_err(std::string("Invalid SKATE collision volume"));
        triangles.push_back(*t);
    }
    // Portable maps have no native cluster hierarchy. Bound contiguous ranges
    // once at load time so the existing BVH can reject distant geometry.
    std::vector<QueryMesh> meshes;
    for (std::size_t start = 0; start < triangles.size(); start += 64) {
        const std::size_t end = std::min(start + 64, triangles.size());
        std::vector<Vector3> pts;
        for (std::size_t i = start; i < end; ++i)
            for (const auto& v : triangles[i].triangle.vertices) pts.push_back(v);
        auto bounds = Bounds::from_points(pts);
        if (!bounds) return rs::make_err(std::string("SKATE collision bounds empty"));
        QueryMesh mesh{};
        mesh.geometry = 0;
        mesh.rejection_flags = 0;
        mesh.triangle_range = Range{start, end};
        mesh.local_to_world = RetailAffineTransform::IDENTITY;
        mesh.world_to_local = RetailAffineTransform::IDENTITY;
        mesh.local_bounds = *bounds;
        mesh.matching_group = -1;
        mesh.pool = QueryPool::Ground;
        meshes.push_back(mesh);
    }
    QueryMetadata metadata{};
    metadata.packed_surfaces = std::move(packed_surfaces);
    metadata.meshes = std::move(meshes);
    metadata.island_flags = 0;
    auto world = BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata));
    if (world.is_err()) return rs::make_err(std::string(world.unwrap_err()));
    return std::move(world.unwrap());
}

}  // namespace s3::game::skate_world
