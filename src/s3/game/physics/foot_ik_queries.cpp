// Port of skate-game/src/physics/foot_ik_queries.rs.
#include "s3/game/physics/foot_ik_queries.hpp"

#include <optional>

#include "s3/core/camera/trajectory_query.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/triangle_query.hpp"

namespace s3::game::physics::foot_ik_queries {

using sc::math::Vector3;

namespace {
Vector3 vector(std::array<float, 4> v) { return Vector3{v[0], v[1], v[2]}; }
std::array<float, 4> lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
}  // namespace

void SkeletonLineTests::publish(sc::player::input_phase::PlayerInputState& player) const {
    auto fields = [](const SkeletonLineHit& hit) {
        sc::player::input_phase::LineTestFields f;
        f.position = rs::to_bits(hit.position);
        f.normal = rs::to_bits(hit.normal);
        f.surface = hit.surface;
        f.valid = static_cast<std::uint8_t>(hit.collision_time >= 0.0f ? 1 : 0);
        return f;
    };
    player.hips_line_test_1488 = fields(hips);
    player.hips_line_origin_1632 = rs::to_bits(hips_origin);
    player.left_line_test_1536 = fields(feet[0]);
    player.right_line_test_1584 = fields(feet[1]);
}

rs::Result<SkeletonLineTests, std::string> query(const sc::physics::board_world::BoardWorld& world,
                                                 const sc::physics::skeleton_body::SkeletonBody& body) {
    const auto parts = body.part_transforms();
    //82DB63EC..647C selects hips23,lefttoe15,righttoe19 from the
    //physical pose. Body COM and the reparented animated targets are different.
    const std::array<float, 4> hips_origin = parts[23][3];
    S3G_TRY(const SkeletonLineHit hips, query_trajectory(world, hips_origin, {0.0f, -100.0f, 0.0f, 0.0f}));
    const std::array<float, 4> raised = {0.0f, rs::from_bits(0x3EA8F5C3u), 0.0f, 0.0f};
    const std::array<float, 4> lowered = {0.0f, -1.5f, 0.0f, 0.0f};
    //Keep the subtraction and subsequent FMA evaluation separate: do not
    //replace this with an independently rounded literal -1.83 or finalY-1.5.
    std::array<float, 4> velocity{};
    for (std::size_t lane = 0; lane < 4; ++lane) velocity[lane] = lowered[lane] - raised[lane];
    std::array<SkeletonLineHit, 2> feet = {SkeletonLineHit::pending_result(), SkeletonLineHit::pending_result()};
    const std::size_t part_ids[2] = {15, 19};
    for (std::size_t foot = 0; foot < 2; ++foot) {
        const std::size_t part = part_ids[foot];
        std::array<float, 4> start{};
        for (std::size_t lane = 0; lane < 4; ++lane) start[lane] = parts[part][3][lane] + raised[lane];
        S3G_TRY(feet[foot], query_trajectory(world, start, velocity));
    }
    SkeletonLineTests out;
    out.hips = hips;
    out.feet = feet;
    out.hips_origin = hips_origin;
    return out;
}

rs::Result<SkeletonLineHit, std::string> query_trajectory(const sc::physics::board_world::BoardWorld& world,
                                                          std::array<float, 4> start, std::array<float, 4> velocity) {
    //82E0B4E0 -> TestBatchManager backend virtual28. Its ctor82E0B460
    //uses Island virtual84=82764CF8, then82775E48/8276E010 creates
    //ClusteredMeshTrajectoryBatch vtable8231093C, AddTest8276E280.
    //S2 named827BDCC0 corroborates radius/time/error fields; TU3 preserves
    //f1..f4 through AggregateTrajectoryBatch8276EDD0 when multiple batches exist.
    sc::camera::TrajectoryQuery request;
    request.position = start;
    request.velocity = velocity;
    request.gravity = {0.0f, 0.0f, 0.0f, 0.0f};
    request.duration = 1.0f;
    request.radius = rs::from_bits(0x3CF5C28Fu);
    request.start_error = 0.0f;
    request.end_error = 0.0f;
    SkeletonLineHit result = SkeletonLineHit::pending_result();
    using F4 = std::array<float, 4>;
    auto line = [&](F4 seg_start, F4 seg_end, float radius) -> rs::Result<std::optional<F4>, std::string> {
        const Vector3 origin = vector(seg_start);
        const Vector3 direction{seg_end[0] - seg_start[0], seg_end[1] - seg_start[1], seg_end[2] - seg_start[2]};
        float nearest = rs::f32::MAX;
        std::optional<F4> position;
        for (const auto& [index, triangle] : world.line_candidates(origin, vector(seg_end), radius)) {
            (void)index;
            sc::physics::triangle_query::TriangleLineHit geometry{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
            //Same source82771D08 leaf as camera: zero triangle fatness,
            //authored face normal, fraction clamp and first equal hit retained.
            if (sc::physics::triangle_query::triangle_segment(geometry, origin, direction,
                                                               triangle->triangle.vertices, radius, 0.0f)) {
                const float lower = -geometry.fraction >= 0.0f ? 0.0f : geometry.fraction;
                const float fraction = 1.0f - lower >= 0.0f ? lower : 1.0f;
                if (fraction < nearest) {
                    nearest = fraction;
                    const F4 point = lanes(geometry.position);
                    result.position = point;
                    result.normal = lanes(triangle->triangle.feature.normal);
                    result.surface = triangle->tag;
                    position = point;
                }
            }
        }
        return position;
    };
    S3G_TRY(result.collision_time, request.collision_time<std::string>(line));
    result.hit = result.collision_time >= 0.0f;
    return result;
}

}  // namespace s3::game::physics::foot_ik_queries
