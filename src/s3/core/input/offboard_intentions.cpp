// Port of skate-core/src/input/offboard_intentions.rs.
// Off-board inputs from original TU3 Fill825999F0 and helper8259C260.
#include "s3/core/input/offboard_intentions.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::input::offboard_intentions {

using rs::from_bits;
using rs::mul_add;

namespace {

float dot(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

std::array<float, 4> safe_unit(const std::array<float, 4>& vector) {
    const float squared = dot(vector, vector);
    float inverse = physics::reciprocal_sqrt::estimate(squared);
    for (int i = 0; i < 2; ++i) {
        const float correction = mul_add(-squared, inverse * inverse, 1.0f);
        inverse = mul_add(inverse * 0.5f, correction, inverse);
    }
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    //Initializer82F826F8 supplies830BD350 from82181A88: LENGTH threshold1e-6.
    if (length > from_bits(0x358637bdu)) {
        return rs::map(vector, [inverse](float value) { return value * inverse; });
    }
    return {0.0f, 0.0f, 0.0f, 0.0f};
}

}  // namespace

std::vector<OffboardIntent> produce_discrete(const DerivedControllerInput& controller, std::uint32_t actor_flags,
                                             bool air_reckoning_active) {
    const auto& words = controller.words();
    const std::uint32_t previous = words[6];
    const std::uint32_t current = words[13];
    auto axis = [&words](std::size_t slot) { return from_bits(words[slot]); };
    auto held = [current](std::uint32_t bit) { return (current & (1u << bit)) != 0; };
    auto rising = [&](std::uint32_t bit) { return held(bit) && (previous & (1u << bit)) == 0; };
    std::vector<OffboardIntent> output;
    output.reserve(12);
    auto emit = [&output](std::string_view name, float value) { output.push_back(OffboardIntent{name, value}); };

    //8259AA8C invokes8259C260 before the later toggle/sprint publications.
    if (rising(23)) {
        emit("OB_Jump", 1.0f);
    }
    const float right_x = axis(9);
    const float right_y = axis(10);
    if (right_x != 0.0f) {
        emit("OB_LookAtX", right_x);
    }
    if (right_y != 0.0f) {
        emit("OB_LookAtY", right_y);
    }
    const bool drop_board = axis(4) != 1.0f && axis(11) == 1.0f;
    const bool throw_board = axis(5) != 1.0f && axis(12) == 1.0f;
    if (drop_board) {
        emit("OB_DropBoard", 1.0f);
    }
    if (throw_board) {
        emit("OB_ThrowBoard", 1.0f);
    }
    if (drop_board || throw_board) {
        emit("OB_RetrieveBoard", 1.0f);
    }
    if (held(28) || held(30)) {
        emit("OB_DoAirBodyTweak", 1.0f);
    }
    emit("OB_AirBodyTweakX", right_x);
    emit("OB_AirBodyTweakY", right_y);

    //8259A604..A6C0 computes both;8259AF80 gates only NewToggle by actor bit10.
    //The listener has no biped/category gate. Stock OffBoard AG maps these
    //same toggle intentions to OB_Mount/OB_MountRaw; trigger edges recall.
    if (!air_reckoning_active && !held(29) && held(22)) {
        //Native bge does not take the branch for unordered timer values.
        if ((rising(22) || !(axis(23) >= from_bits(0x3cf5c28fu))) && (actor_flags & (1u << 10)) == 0) {
            emit("NewToggleOffBoardState", 1.0f);
        }
        emit("ToggleOffBoardState", 1.0f);
    }
    if (!(axis(21) > 0.0f) && axis(20) > 0.0f) {
        emit("OB_Sprint", 1.0f);
    }
    return output;
}

std::array<OffboardIntent, 4> produce_analog(const DerivedControllerInput& controller,
                                             AnalogObservation observation) {
    const auto& words = controller.words();
    const std::array<float, 4> stick = {from_bits(words[7]), 0.0f, from_bits(words[8]), 0.0f};
    std::array<float, 4> horizontal = observation.effective_skeleton_z;
    horizontal[1] = 0.0f;
    const auto forward = safe_unit(horizontal);
    float magnitude;
    if (observation.biped_correction) {
        const std::array<float, 4> normal = *observation.biped_correction;
        const auto negative =
            rs::map(normal, [](float value) { return from_bits(rs::to_bits(value) ^ 0x80000000u); });
        const float projection = dot(stick, negative);
        const float lower = -projection >= 0.0f ? 0.0f : projection;
        const float coefficient = 1.0f - lower >= 0.0f ? lower : 1.0f;
        //8259A994 ble takes only ordered <=. Unordered follows the zero branch.
        if (!(dot(safe_unit(stick), negative) <= from_bits(0x3f666666u))) {
            magnitude = 0.0f;
        } else {
            const auto corrected =
                rs::from_fn<4>([&](std::size_t i) { return mul_add(normal[i], coefficient, stick[i]); });
            magnitude = dot(forward, corrected);
        }
    } else {
        magnitude = dot(forward, stick);
    }
    return {
        OffboardIntent{"OB_Mag", magnitude},
        OffboardIntent{"OB_BipedWorldZ", stick[2]},
        OffboardIntent{"OB_BipedWorldX", stick[0]},
        OffboardIntent{"OB_BipedStickMag", controller::magnitude(dot(stick, stick))},
    };
}

}  // namespace s3::input::offboard_intentions
