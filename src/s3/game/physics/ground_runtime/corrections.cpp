// Port of skate-game/src/physics/ground_runtime/corrections.rs.
#include "s3/game/physics/ground_runtime/corrections.hpp"

#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/game/physics/ground_runtime.hpp"

namespace s3::game::physics::ground_runtime {

using sc::physics::board::BodyId;
using sc::physics::board::index;
namespace gcm = sc::riding::ground_correction_math;

rs::Result<float, GroundRuntime::Error> GroundRuntime::angle_between(std::array<float, 4> deck_z,
                                                                    std::array<float, 4> reference_z,
                                                                    std::array<float, 4> reference_x) {
    return sc::riding::collision_response::signed_angle(corrections::xyz(deck_z), corrections::xyz(reference_z),
                                                        corrections::xyz(reference_x));
}

rs::Result<float, GroundRuntime::Error> GroundRuntime::dot3(std::array<float, 4> a, std::array<float, 4> b) {
    return gcm::dot_product(a, b);
}

rs::Result<std::array<float, 4>, GroundRuntime::Error> GroundRuntime::scale_to_magnitude(std::array<float, 4> vector,
                                                                                        float squared_length,
                                                                                        float magnitude) {
    return gcm::scale_to_magnitude(vector, squared_length, magnitude);
}

rs::Result<bool, std::string> GroundRuntime::detect_hung_up_geometry(
    const sc::physics::board_world::BoardWorld& world,
    sc::physics::ground_hang_geometry::HangGeometryInput input) const {
    auto r = sc::physics::ground_hang_geometry::detect_hung_geometry(world, input, deck_center_to_truck);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return r.unwrap();
}

float GroundRuntime::collision_force_projection(std::array<float, 4> force, std::array<float, 4> velocity) const {
    return gcm::collision_force_projection(force, velocity);
}

std::array<float, 4> GroundRuntime::build_hang_force(const sc::physics::board_runtime::BoardRuntime& board,
                                                     std::array<float, 4> edge_start,
                                                     std::array<float, 4> edge_end) const {
    const auto p = board.part_transforms()[index(BodyId::Deck)].translation;
    return gcm::hang_force(edge_start, edge_end, {p.x, p.y, p.z, 0.0f});
}

void GroundRuntime::apply_hang_force(sc::physics::board_runtime::BoardRuntime& board, std::array<float, 4> force) {
    const auto position = board.part_transforms()[index(BodyId::Deck)].translation;
    gcm::apply_world_force(board.bodies_mut()[index(BodyId::Deck)], position, corrections::xyz(force), position);
}

void GroundRuntime::pin_to_position(sc::physics::board_runtime::BoardRuntime& board, float captured_x,
                                    float captured_z, float dt) {
    const auto p = board.part_transforms()[index(BodyId::Deck)].translation;
    const auto velocity = gcm::pinning_velocity({p.x, p.y, p.z, 0.0f}, captured_x, captured_z, dt);
    set_animated_velocity(board, velocity);
}

namespace corrections {
sc::math::Vector3 xyz(std::array<float, 4> v) { return sc::math::Vector3::new_(v[0], v[1], v[2]); }
}  // namespace corrections

}  // namespace s3::game::physics::ground_runtime
