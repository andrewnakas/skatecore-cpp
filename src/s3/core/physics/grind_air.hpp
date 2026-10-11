// Port of skate-core/src/physics/grind_air.rs (+ grind_air/{angles,pose,prediction,selection,targets}.rs).
//
// Original S3 82D71138/82D34FE8/82D712E0, with persistent native histories.
// Independent PC arithmetic; not a bit-exact Xenon emulation claim.
//
// The submodule declarations (targets, pose, prediction, selection, angles) live in this header,
// in Rust declaration-dependency order; grind_air/<name>.hpp only forwards here. Their
// definitions are in grind_air/<name>.cpp.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/rs.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_air {

using Frame = skeleton_animation_record::AnimationPartTransform;
using grind_contact::Primitive;
using air::trajectory::grind_surface::LandingOrientation;
namespace arithmetic = native_arithmetic;

using V = std::array<float, 4>;
inline constexpr V ZERO = {0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr V UP = {0.0f, 1.0f, 0.0f, 0.0f};
inline constexpr float DEG = rs::from_bits(0x3c8efa35u);
inline constexpr float RAD = rs::from_bits(0x42652ee1u);
inline constexpr float PI = rs::from_bits(0x40490fdbu);
inline constexpr float TAU = rs::from_bits(0x40c90fdbu);

// ---- private helpers of grind_air.rs (visible to the submodules via `use super::*`) ----
inline float dot(const V& a, const V& b) { return arithmetic::dot3(a, b); }
inline V sub(const V& a, const V& b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V add(const V& a, const V& b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline V scale(const V& v, float s) { return V{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
inline V madd(const V& v, float s, const V& a) {
    return V{rs::mul_add(v[0], s, a[0]), rs::mul_add(v[1], s, a[1]), rs::mul_add(v[2], s, a[2]),
             rs::mul_add(v[3], s, a[3])};
}
inline V cross(const V& a, const V& b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}
inline float inverse_length(float square) {
    float r = arithmetic::reciprocal_square_root_estimate(square);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-square, r * r, 1.0f), r);
    }
    return r;
}
inline float reciprocal(float value) {
    float r = arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r, rs::mul_add(-r, value, 1.0f), r);
    }
    return r;
}
inline float length(const V& v) {
    const float s = dot(v, v);
    return s == 0.0f ? 0.0f : s * inverse_length(s);
}
inline V unit(const V& v) {
    const float s = dot(v, v);
    const float r = inverse_length(s);
    const float magnitude = s == 0.0f ? 0.0f : s * r;
    if (magnitude > rs::from_bits(0x358637bdu)) {
        return scale(v, r);
    }
    return ZERO;
}
inline V limited(const V& v, float maximum) {
    const float magnitude = length(v);
    if (magnitude > maximum) {
        return scale(v, maximum * reciprocal(magnitude));
    }
    return v;
}
inline float wrap(float angle) {
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * TAU;
}
/// 8296EC98: one refinement, positive full-turn signed angle then8258DB98 wrap.
inline float signed_angle(const V& a_in, const V& b_in, const V& axis) {
    const float aa = dot(a_in, a_in);
    const float bb = dot(b_in, b_in);
    if (!(aa > rs::from_bits(0x38d1b717u) && bb > rs::from_bits(0x38d1b717u))) {
        return 0.0f;
    }
    auto normalize = [](const V& v, float square) {
        const float r = arithmetic::reciprocal_square_root_estimate(square);
        return scale(v, rs::mul_add(r * 0.5f, rs::mul_add(-square, r * r, 1.0f), r));
    };
    const V a = normalize(a_in, aa);
    const V b = normalize(b_in, bb);
    const float angle = trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
    return wrap(dot(cross(a, b), axis) < 0.0f ? TAU - angle : angle);
}

struct Adjustment;
struct GrindAir;
struct Input;
struct Target;

}  // namespace s3::physics::grind_air


// ---- grind_air/targets.rs (submodule `targets`) ----

namespace s3::physics::grind_air::targets {

inline constexpr std::array<std::size_t, 7> CONTACTS = {0, 1, 4, 0, 2, 2, 3};
inline constexpr std::array<float, 7> DESIRED = {90.0f, 90.0f, 90.0f, 90.0f, 0.0f, 0.0f, 0.0f};

struct Settings {
    std::size_t frames = 0;
    float stomp = 0.0f;
    std::array<V, 5> points{};
    std::array<float, 7> ranges{};
    std::array<float, 7> distances{};
    std::array<float, 7> yaw_assist{};
    float max_offset = 0.0f;
    float max_delta = 0.0f;
    float max_angle = 0.0f;

    rs::Result<rs::Unit, std::string> validate() const {
        // Host data validation, not a gameplay clamp or a fabricated setting.
        if (!(frames >= 2 && frames <= 12)) {
            return rs::make_err(std::string("Stock GrindAir NumFramesToTest exceeds native 2..12 storage"));
        }
        return rs::Unit{};
    }
};

struct DeckDimensions {
    float middle_length = 0.0f;
    float front_end_size = 0.0f;
    float front_end_angle_degrees = 0.0f;
    float truck_z_front = 0.0f;
    float truck_y = 0.0f;

    /// 82D72B40 uses sine for tip height but adds the unprojected tip length to Z.
    std::array<V, 5> contact_points(float tip_fraction) const {
        const float half = middle_length * 0.5f;
        const float tip = front_end_size * tip_fraction;
        const float y = trigonometry::sin_cos(front_end_angle_degrees * DEG).first * tip;
        const float z = half + truck_z_front;
        return {
            ZERO,
            V{0.0f, y, tip + half, 0.0f},
            V{0.0f, truck_y, z, 0.0f},
            V{0.0f, truck_y, -z, 0.0f},
            V{0.0f, y, -(tip + half), 0.0f},
        };
    }
};

}  // namespace s3::physics::grind_air::targets

namespace s3::physics::grind_air {
using targets::DeckDimensions;
using targets::Settings;
}  // namespace s3::physics::grind_air

// ---- grind_air/pose.rs (submodule `pose`) ----

namespace s3::physics::grind_air::pose {

struct PoseInput {
    /// Skeleton11920, not the processed physical board frame.
    Frame animation_to_world{};
    /// Skeleton11984, already produced by UpdateRootTransforms.
    Frame world_to_animation{};
    /// Skeleton12528: retained physical-board Z axis.
    V physical_forward{};
    /// Skeleton12672: this frame's mapped animation board translation.
    V animation_board_position{};
};

/// Full-angle axis matrix used by82D71430 and82BDCFB0 (not half-angle quaternion).
Frame axis_rotation(V axis, float angle);
V direction(const Frame& frame, V vector);

}  // namespace s3::physics::grind_air::pose

namespace s3::physics::grind_air {
using pose::PoseInput;
}  // namespace s3::physics::grind_air

namespace s3::physics::grind_air {

struct Target {
    Primitive edge;
    std::uint32_t primitive_flags = 0;
    LandingOrientation orientation;
};

struct Input {
    bool active = false;
    Frame board{};
    V velocity{};
    V angular_velocity{};
    V up{};
    float timestep = 0.0f;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
};

struct Adjustment {
    V axis{};
    V offset{};
    V angles{};

    /// 82BDCFB0: R(up,Y)*R(rail,X)*R(physicalZ,Z), converted to animation space,
    /// then pivot around the current mapped board and add the local displacement.
    Frame local_transform(PoseInput input) const;
};

struct GrindAir {
    std::optional<Target> target;
    // Private in Rust; public here so the submodule functions (angles::update) can reach them.
    V offset_delta = ZERO;
    V offset = ZERO;
    V angle_delta = ZERO;
    V angles = ZERO;
    std::optional<std::size_t> selected_kind;
    // Native array is initialized once, not cleared on each prediction.
    std::array<float, 12> headings{};

    static GrindAir default_() { return GrindAir{}; }

    /// 82D34FE8. Started/activated latches are owned by the existing KnownAir caller.
    void start(Target target);
    rs::Result<std::optional<Adjustment>, std::string> update(Input input, const Settings& settings);
};

}  // namespace s3::physics::grind_air


// ---- grind_air/prediction.rs (submodule `prediction`) ----

namespace s3::physics::grind_air::prediction {

using Samples = std::array<std::array<V, 5>, 12>;

/// 82D71430: frame zero is the actual processed deck; advance velocity before position.
Samples project(Input input, const Settings& s, V rail, V normal, std::array<float, 12>& headings);

}  // namespace s3::physics::grind_air::prediction

// ---- grind_air/selection.rs (submodule `selection`) ----

namespace s3::physics::grind_air::selection {

struct Contacts {
    std::array<float, 5> times{};
    std::array<float, 5> distances{};
    std::array<V, 5> corrections{};
    float earliest = 0.0f;
};

Contacts contacts(const prediction::Samples& samples, std::size_t count, V origin, V rail, V normal);

std::optional<std::size_t> choose(const Contacts& c, const std::array<float, 12>& headings,
                                  const Settings& s, bool inverted,
                                  std::optional<std::size_t> previous);

}  // namespace s3::physics::grind_air::selection

// ---- grind_air/angles.rs (submodule `angles`) ----

namespace s3::physics::grind_air::angles {

void update(GrindAir& state, Input input, const Settings& s, Target target,
            const prediction::Samples& samples, V rail, V normal, std::size_t kind, float time);

}  // namespace s3::physics::grind_air::angles
