// Port of skate-core/src/animation/skeleton_input/contact_events.rs.
#include "s3/core/animation/skeleton_input/contact_events.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::skeleton_input::contact_events {

using name::encode;
using physics::native_arithmetic::dot3;
using physics::native_arithmetic::reciprocal_square_root_estimate;

namespace {
void replace(std::uint32_t& flags, std::uint32_t bit, bool set) {
    flags = (flags & ~(1u << bit)) | (static_cast<std::uint32_t>(set) << bit);
}
}  // namespace

rs::Result<rs::Unit, std::string> dispatch(const AnimationAttribute& attribute, const ContactEventPose& pose,
                                           ScalarAttributeInputs& fields, ContactEventState& state) {
    if (attribute.kind != 3 || (attribute.status & 12) == 0) {
        return rs::Unit{};
    }
    std::array<std::uint32_t, 6> words{};
    for (std::size_t i = 0; i < 6; ++i) {
        if (!attribute.payload._0[i]) {
            return rs::make_err(std::string("Active animation bone event has incomplete payload"));
        }
        words[i] = *attribute.payload._0[i];
    }
    const AttributeName name{{words[0], words[1], words[2], words[3], words[4]}};
    const float strength = rs::from_bits(words[5]);
    state.bone = -1;
    for (std::size_t i = 0; i < pose.bone_names.size(); ++i) {
        if (pose.bone_names[i] == name) {
            state.bone = static_cast<std::int32_t>(i);
            break;
        }
    }
    if (attribute.name == encode("push_contact")) {
        fields.flags2468 |= 1u << 27;
        replace(fields.flags2468, 26, state.bone == pose.right_toe_bone);
        replace(fields.flags2468, 25, strength == 1.0f);
        replace(fields.flags2468, 24, strength == -1.0f);
        const NativeMatrix* trajectory = pose.hierarchy.get(pose.trajectory_bone);
        if (!trajectory) {
            return rs::make_err(std::string("Push contact requires the actual animation trajectory bone"));
        }
        const std::array<float, 4> translation = (*trajectory)[3];
        const float squared = dot3(translation, translation);
        float inverse = reciprocal_square_root_estimate(squared);
        for (int i = 0; i < 2; ++i) {
            inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
        }
        // Native clears a zero-length result before multiplying by scalar
        // fdivs(1,dt). Estimate arithmetic remains hardware-unverified.
        const float length = squared == 0.0f ? 0.0f : squared * inverse;
        const float speed = length * (1.0f / pose.timestep);
        state.push_speed = strength == 1.0f ? speed : 0.0f;
    } else if (attribute.name == encode("brake_contact")) {
        fields.flags2468 |= 1u << 28;
        replace(fields.flags2468, 30, strength == 1.0f);
        replace(fields.flags2468, 23, strength == -1.0f);
    } else if (attribute.name == encode("right_hand_grab") || attribute.name == encode("left_hand_grab")) {
        // The payload bone name, not the event's left/right label, selects it.
        fields.flags2472 |= name == encode("righthand") ? 0x100u : 0x80u;
    }
    return rs::Unit{};
}

}  // namespace s3::animation::skeleton_input::contact_events
