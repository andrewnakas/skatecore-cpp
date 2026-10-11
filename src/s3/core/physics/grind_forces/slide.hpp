// Port of skate-core/src/physics/grind_forces/slide.rs.
// Translation branches82D419A0/82D41438; S2 boardslide82D90970.
#pragma once
#include <vector>

#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces::slide {

enum class Slide {
    Boardslide,
    Darkslide,
};

/// All vectors are the physical-state/processed observations. Preparing-jump
/// is the retained state byte241, not a newly sampled analog input.
struct Input {
    V position{};
    V point{};
    V direction{};
    V across{};
    V velocity{};
    float translation_2796 = 0.0f;
    float total_mass_2660 = 0.0f;
    /// Source global update frequency, not an invented fixed60 or render rate.
    float update_frequency = 0.0f;
    bool preparing_jump = false;
    std::uint32_t geometry_kind = 0;
};

/// Forces at the processed deck position, in original application order.
/// The caller applies these directly, not through the21-entry tagged queue.
inline std::vector<V> control(Slide slide, Input input) {
    const float translation = input.translation_2796;
    const float mass = input.total_mass_2660;
    const float offset = dot3(sub(input.position, input.point), input.across);
    const V inward = scale(input.across, offset > 0.0f ? -1.0f : 1.0f);
    if (input.preparing_jump) {
        if (rs::abs(offset) > 0.12f) {
            return {scale(inward, 201.0f)};
        }
        return {};
    }
    const float translation_strength =
        slide == Slide::Boardslide ? 25.0f : (1.0f - rs::abs(offset) * 6.25f) * 40.0f;
    const V translate = scale(input.across, translation * translation_strength);
    if (input.geometry_kind == 2) {
        return {translate};
    }
    const V perpendicular = sub(input.velocity, scale(input.direction, dot3(input.velocity, input.direction)));
    if (rs::abs(offset) > 0.16f) {
        std::vector<V> forces{scale(inward, 10.0f)};
        if (dot3(inward, perpendicular) < 0.0f) {
            V stop = scale(perpendicular, -(mass * input.update_frequency));
            stop[1] = 0.0f;
            forces.push_back(stop);
        }
        return forces;
    }
    V damping = scale(perpendicular, -20.0f);
    damping[1] = 0.0f;
    return {translate, damping};
}

}  // namespace s3::physics::grind_forces::slide
