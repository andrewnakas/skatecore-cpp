// Port of skate-core/src/player/offboard/biped_air/height.rs.
// Original TU3 82D2FF50 and 82D2FDD0, recovered from disassembly.
// State::correct_height is defined in recovered.cpp.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::biped_air::recovered::height {

using Vector = std::array<float, 4>;

// pub(super)
inline float dot(Vector a, Vector b) { return rs::mul_add(a[2], b[2], rs::mul_add(a[1], b[1], a[0] * b[0])); }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector madd(Vector a, float b, Vector c) {
    return Vector{rs::mul_add(a[0], b, c[0]), rs::mul_add(a[1], b, c[1]), rs::mul_add(a[2], b, c[2]),
                  rs::mul_add(a[3], b, c[3])};
}
inline float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }

///Translations returned by82BE3220 for bones15/19, in that order.
inline float projected_height(Vector bone15, Vector bone19, Vector reference, Vector up) {
    const float a = dot(up, sub(bone15, reference));
    const float b = dot(up, sub(bone19, reference));
    return select(a - b, b, a);
}

struct HeightInput {
    Vector bone15{};
    Vector bone19{};
    ///82BE3170(skeleton,1) translation.
    Vector bone1{};
    Vector up_544{};
    Vector velocity_608{};
};

}  // namespace s3::player::offboard::biped_air::recovered::height
