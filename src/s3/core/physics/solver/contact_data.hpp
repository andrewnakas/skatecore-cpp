// Port of skate-core/src/physics/solver/contact_data.rs (module solver::contact::data).
// Named view of the compiled contact and shared reaction records.
// Fourth components retain the native packed layout, including inverse mass
// alongside each body's first angular response.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/slice.hpp"

namespace s3::physics::solver::contact::data {

using Components = std::array<float, 4>;

struct ContactRows {
    Components arm_a{};
    Components arm_b{};
    std::array<Components, 3> correction_columns{};
    float static_friction = 0.0f;
    float dynamic_friction = 0.0f;
    Components accumulated{};
    Components target{};
    // private in Rust
    std::array<Components, 3> axes{};
    std::array<Components, 3> angular_a{};
    std::array<Components, 3> angular_b{};

    static ContactRows read(Slice<std::uint32_t> words);
    void publish_impulses(Slice<std::uint32_t> words, Components impulse) const;
};

struct Reaction {
    Components linear{};
    Components position{};
    Components angular{};
    Components orientation{};

    static Reaction read(Slice<std::uint32_t> words);
    Components point_position(Components arm) const;
    Components point_velocity(Components arm) const;
    void apply_contact(const ContactRows& rows, Components change, bool body_a);
    void write(Slice<std::uint32_t> words) const;
};

}  // namespace s3::physics::solver::contact::data
