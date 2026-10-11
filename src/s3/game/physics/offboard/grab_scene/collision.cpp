// Port of skate-game/src/physics/offboard/grab_scene/collision.rs
#include "s3/game/physics/offboard/grab_scene/collision.hpp"

#include <algorithm>
#include <utility>

#include "s3/core/physics/triangle_query.hpp"

namespace s3::game::physics::offboard::grab_scene::collision {

using sc::math::Vector3;
using sc::physics::board_world::query_metadata::QueryPool;
using sc::physics::triangle_query::TriangleLineHit;

namespace {
Vector3 xyz(const std::array<float, 4>& v) { return Vector3::new_(v[0], v[1], v[2]); }
}  // namespace

rs::Result<std::optional<Hit>, std::string> line(const Scene& scene, Line line) {
    bool bad = false;
    for (float v : line.start) bad = bad || !rs::is_finite(v);
    for (float v : line.end) bad = bad || !rs::is_finite(v);
    if (line.radius < 0.0f || !rs::is_finite(line.radius) || bad) {
        return rs::make_err(std::string("Invalid grab validation swept line"));
    }
    std::array<float, 4> delta{};
    for (std::size_t i = 0; i < 4; ++i) delta[i] = line.end[i] - line.start[i];
    bool any = false;
    for (int i = 0; i < 3; ++i) any = any || rs::abs(delta[i]) > rs::from_bits(0x37800000u);
    if (!any) return std::optional<Hit>{};
    auto metadata_r = scene.world->query_metadata();
    if (metadata_r.is_err()) return rs::make_err(std::string(metadata_r.unwrap_err()));
    const auto* metadata = metadata_r.unwrap();
    const auto bounds = scene.world->line_candidate_bounds(xyz(line.start), xyz(line.end), line.radius);
    auto meshes_r = scene.world->candidate_mesh_indices(bounds);
    if (meshes_r.is_err()) return rs::make_err(std::string(meshes_r.unwrap_err()));
    const auto meshes = meshes_r.unwrap();
    std::vector<std::size_t> candidates;
    for (const auto& c : scene.world->line_candidates(xyz(line.start), xyz(line.end), line.radius)) {
        candidates.push_back(c.first);
    }
    float nearest = rs::f32::MAX;
    std::optional<Hit> result;
    const std::array<std::pair<std::uint8_t, QueryPool>, 3> pools{
        std::pair<std::uint8_t, QueryPool>{1, QueryPool::Ground},
        std::pair<std::uint8_t, QueryPool>{2, QueryPool::Island},
        std::pair<std::uint8_t, QueryPool>{4, QueryPool::Conditional},
    };
    for (const auto& [bit, pool] : pools) {
        if ((line.source_pool_mask & bit) == 0) continue;
        if (pool == QueryPool::Conditional && metadata->island_flags != 3) continue;
        for (std::size_t mesh_index : meshes) {
            const auto& mesh = metadata->meshes[mesh_index];
            if (mesh.pool != pool || (mesh.rejection_flags & line.reject_flags) != 0 ||
                !(line.group == -1 || mesh.matching_group == -1 || line.group == mesh.matching_group)) {
                continue;
            }
            const auto first = std::lower_bound(candidates.begin(), candidates.end(), mesh.triangle_range.start);
            const auto last = std::lower_bound(candidates.begin(), candidates.end(), mesh.triangle_range.end);
            for (auto it = first; it != last; ++it) {
                const std::size_t index = *it;
                TriangleLineHit hit{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
                const auto vertices = scene.world->triangles()[index].triangle.vertices;
                if (!sc::physics::triangle_query::triangle_segment(hit, xyz(line.start), xyz(delta), vertices,
                                                                   line.radius, 0.0f)) {
                    continue;
                }
                const float lower = -hit.fraction >= 0.0f ? 0.0f : hit.fraction;
                const float fraction = 1.0f - lower >= 0.0f ? lower : 1.0f;
                if (fraction < nearest) {
                    nearest = fraction;
                    result = Hit{fraction, scene.registry->assembly(mesh.geometry)};
                }
            }
        }
    }
    return result;
}

}  // namespace s3::game::physics::offboard::grab_scene::collision
