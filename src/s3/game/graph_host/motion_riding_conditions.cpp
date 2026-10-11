// Port of skate-game/src/graph_host/motion_riding_conditions.rs (module `motion_riding_conditions`).
#include "s3/game/graph_host/motion_riding_conditions.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/trigonometry.hpp"
#include "s3/game/graph_host/condition_nodes.hpp"

namespace s3::game::graph_host::motion_riding_conditions {

using s3::math::Vector3;
using s3::physics::board_motion_output::dot;
using s3::physics::board_motion_output::length;

namespace {
Vector3 xyz(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }

float slope(float up_y) {
    //82BA4DD0/82BA4F90 inline the same original inverse-sine polynomial as
    //DisablePushBrake82BA5150; no clamp or absolute value precedes asin.
    return 90.0f - s3::trigonometry::asin(up_y) * rs::from_bits(0x42652ee1u);
}
}  // namespace

rs::Result<MotionRidingCondition, std::string> MotionRidingCondition::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    const std::string name = name_ptr ? *name_ptr : std::string();
    // Factory82BC3CE8 adds no attributes beyond the base condition.
    if (name == "RandomCond") return MotionRidingCondition(Random{});
    if (name == "ComVelCompare") {
        // Ctor82BA3ED0 examines only the first byte, default "0".
        const std::string* axis_ptr = a.text("axis");
        const std::string axis_text = axis_ptr ? *axis_ptr : std::string("0");
        VelocityAxis axis = VelocityAxis::All;
        if (!axis_text.empty()) {
            switch (axis_text[0]) {
                case 'x':
                case 'X': axis = VelocityAxis::X; break;
                case 'y':
                case 'Y': axis = VelocityAxis::Y; break;
                case 'z':
                case 'Z': axis = VelocityAxis::Z; break;
                default: axis = VelocityAxis::All; break;
            }
        }
        return MotionRidingCondition(ComVelocity{axis, condition_nodes::numeric(a)});
    }
    // Factories82BC3258/82BC31B8 use NumericCondition82C126F0.
    if (name == "SkateSlope") return MotionRidingCondition(SkateSlope{condition_nodes::numeric(a)});
    if (name == "SurfaceSlope") return MotionRidingCondition(SurfaceSlope{condition_nodes::numeric(a)});
    return rs::make_err("Unknown riding condition " + name);
}

rs::Result<bool, std::string> MotionRidingCondition::evaluate_with(
    const MotionRandom& condition_random, const std::optional<RidingConditionInputs>& riding_conditions) const {
    if (std::holds_alternative<Random>(value)) {
        //82BA4D50 -> ISkaterMotionGraph256 ->8258FB60 ->82970628.
        // Advance only when the graph actually evaluates this leaf.
        auto next = condition_random.next_u32();
        if (next.is_err()) return rs::make_err(next.unwrap_err());
        return (next.unwrap() & 1u) == 0;
    }
    if (!riding_conditions) {
        return rs::make_err(std::string("MotionGraph requires the actual COM and slope publication"));
    }
    const RidingConditionInputs& p = *riding_conditions;
    if (const auto* c = std::get_if<ComVelocity>(&value)) {
        //82BA3FA8 uses relative X/Z but WORLD Y. The named Skate2
        //counterpart reads direct XYZ, so its axis behavior differs.
        const Vector3 velocity = xyz(p.com_velocity);
        float v = 0.0f;
        switch (c->axis) {
            case VelocityAxis::X: v = dot(velocity, xyz(p.skeleton_x)); break;
            case VelocityAxis::Y: v = velocity.y; break;
            case VelocityAxis::Z: v = dot(velocity, xyz(p.skeleton_z)); break;
            case VelocityAxis::All: v = length(velocity); break;
        }
        return c->numeric.matches(v);
    }
    if (const auto* c = std::get_if<SkateSlope>(&value)) return c->_0.matches(slope(p.skate_up_y));
    return std::get<SurfaceSlope>(value)._0.matches(slope(p.surface_up_y));
}

MotionRandom::MotionRandom()
    //FullMotionGraph6000/6004 start at zero, then6008..6031 receive the
    //original 24-byte literal821642B0. No external runtime RNG is used.
    : words_{0, 0, 0xf22d0e56u, 0x883126e9u, 0xc624dd2fu, 0x0702c49cu, 0x9e353f7du, 0x6fdf3b64u} {}

rs::Result<std::uint32_t, std::string> MotionRandom::next_u32() const {
    std::array<std::uint32_t, 8>& words = words_;
    //Complete original82970628 carry/update sequence, including the
    //separate counter rollover propagation after producing word2.
    const std::uint32_t last = words[7];
    const std::uint32_t previous = words[6];
    std::uint32_t sum = previous + last;
    std::uint32_t carry = (sum < last || sum < previous) ? 1u : 0u;
    words[6] = sum;
    for (int index = 5; index >= 2; --index) {
        const std::uint32_t prev = words[index];
        sum = prev + sum + carry;
        carry = sum < prev ? 1u : 0u;
        words[index] = sum;
    }
    words[7] = last + 1u;
    if (words[7] == 0) {
        for (int index = 6; index >= 2; --index) {
            words[index] += 1u;
            if (words[index] != 0) break;
        }
    }
    words[1] += 1u;
    return words[2];
}

}  // namespace s3::game::graph_host::motion_riding_conditions
