// Port of skate-game/src/physics/grind_camera.rs.
//! ZIP contact conditioner82DF0640, corroborated against S2 82E2D000.
//! Tangent selection82D87460 belongs to the manager, not to this conditioner.
#include "s3/game/physics/grind_camera.hpp"

#include "s3/core/rs.hpp"

namespace s3::game::physics::grind_camera {

namespace {
V vector(const std::array<std::uint32_t, 4>& v) { return ::s3::rs::from_bits(v); }
}  // namespace

void GrindCamera::condition_fields(::s3::player::input_phase::grind_output::GrindOutputFields& out) {
    std::optional<Contact> contact;
    if (out.grinding_316 != 0) {
        Contact c;
        c.point = vector(out.point_16);
        c.direction = vector(out.direction_0);
        c.endpoints = {vector(out.primitive_start_64), vector(out.primitive_end_80)};
        c.family = out.words_136_140[0];
        contact = c;
    }
    CameraGrindOutput camera;
    camera.direction_0 = vector(out.direction_0);
    camera.camera_target_96 = vector(out.camera_target_96);
    camera.grinding_316 = out.grinding_316;
    update(contact, camera);
    out.direction_0 = ::s3::rs::to_bits(camera.direction_0);
    out.camera_target_96 = ::s3::rs::to_bits(camera.camera_target_96);
}

void GrindCamera::update(const std::optional<Contact>& contact_opt, CameraGrindOutput& output) {
    if (!contact_opt) {
        active = false;
        return;
    }
    const Contact& contact = *contact_opt;
    V next_midpoint{};
    for (std::size_t i = 0; i < 4; ++i) next_midpoint[i] = (contact.endpoints[0][i] + contact.endpoints[1][i]) * 0.5f;
    // `midpoint[..3] != self.midpoint[..3]`: element-wise f32 comparison.
    const bool midpoint_changed = !(next_midpoint[0] == midpoint[0] && next_midpoint[1] == midpoint[1] &&
                                    next_midpoint[2] == midpoint[2]);
    if (!active) {
        error = {0.0f, 0.0f, 0.0f, 0.0f};
        current = contact.point;
        previous = contact.point;
    } else if (contact.family != family || midpoint_changed) {
        for (std::size_t i = 0; i < 4; ++i) error[i] = current[i] + (current[i] - previous[i]) - contact.point[i];
    }
    // S3 constructor82DF2514:0.95 per physical update, not dt-based smoothing.
    for (float& v : error) v = v * 0.95f;
    previous = current;
    for (std::size_t i = 0; i < 4; ++i) current[i] = contact.point[i] + error[i];
    family = contact.family;
    midpoint = next_midpoint;
    active = true;
    output.direction_0 = contact.direction;
    output.camera_target_96 = current;
    // grinding_316 is produced by the physical selector; never inferred here.
}

}  // namespace s3::game::physics::grind_camera
