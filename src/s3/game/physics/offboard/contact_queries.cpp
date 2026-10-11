// Port of skate-game/src/physics/offboard/contact_queries.rs
#include "s3/game/physics/offboard/contact_queries.hpp"

#include <algorithm>
#include <vector>

namespace s3::game::physics::offboard::contact_queries {

using sc::physics::board_world::query_metadata::QueryPool;

Vector3 xyz(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }

rs::Result<std::optional<Hit>, std::string> query(const BoardWorld& world, Probe probe, std::uint32_t matching_id) {
    auto metadata_r = world.query_metadata();
    if (metadata_r.is_err()) return rs::make_err(std::string(metadata_r.unwrap_err()));
    const auto* metadata = metadata_r.unwrap();
    const Vector3 start = xyz(probe.start);
    const Vector3 end = xyz(probe.end);
    const Vector3 delta = Vector3::new_(end.x - start.x, end.y - start.y, end.z - start.z);
    std::optional<Hit> nearest;
    // Accelerate the canonical identity-transform map representation. Authored
    // transformed meshes retain the same geometry path without incorrect culling.
    bool all_identity = true;
    for (const auto& m : metadata->meshes) {
        const auto& t = m.local_to_world;
        const auto& id = RetailAffineTransform::IDENTITY;
        all_identity = all_identity && t.basis == id.basis && t.translation == id.translation;
    }
    std::vector<std::size_t> candidates;
    if (all_identity) {
        for (const auto& c : world.line_candidates(start, end, probe.radius)) candidates.push_back(c.first);
    } else {
        for (std::size_t i = 0; i < world.triangles().size(); ++i) candidates.push_back(i);
    }
    for (QueryPool pool : {QueryPool::Ground, QueryPool::Island, QueryPool::Conditional}) {
        if (pool == QueryPool::Conditional && metadata->island_flags != 3) continue;
        for (std::size_t mesh_index = 0; mesh_index < metadata->meshes.size(); ++mesh_index) {
            const auto& mesh = metadata->meshes[mesh_index];
            if (mesh.pool != pool ||
                !(mesh.matching_group == -1 || mesh.matching_group == static_cast<std::int32_t>(matching_id))) {
                continue;
            }
            const auto first = std::lower_bound(candidates.begin(), candidates.end(), mesh.triangle_range.start);
            const auto last = std::lower_bound(candidates.begin(), candidates.end(), mesh.triangle_range.end);
            for (auto it = first; it != last; ++it) {
                const std::size_t index = *it;
                const auto& triangle = world.triangles()[index].triangle;
                const auto frame = mesh.local_to_world;
                std::array<Vector3, 3> vertices{};
                for (std::size_t k = 0; k < 3; ++k) {
                    const Vector3 v = triangle.vertices[k];
                    const auto t = frame.translation;
                    const auto& b = frame.basis.columns;
                    vertices[k] = Vector3::new_(
                        rs::mul_add(b[2][0], v.z, rs::mul_add(b[1][0], v.y, rs::mul_add(b[0][0], v.x, t.x))),
                        rs::mul_add(b[2][1], v.z, rs::mul_add(b[1][1], v.y, rs::mul_add(b[0][1], v.x, t.y))),
                        rs::mul_add(b[2][2], v.z, rs::mul_add(b[1][2], v.y, rs::mul_add(b[0][2], v.x, t.z))));
                }
                TriangleLineHit geometry{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
                if (sc::physics::triangle_query::triangle_segment(geometry, start, delta, vertices, probe.radius,
                                                                  triangle.fatness) &&
                    (!nearest.has_value() || geometry.fraction < nearest->geometry.fraction)) {
                    nearest = Hit{geometry, metadata->packed_surfaces[index], mesh_index, frame};
                }
            }
        }
    }
    return nearest;
}

}  // namespace s3::game::physics::offboard::contact_queries
