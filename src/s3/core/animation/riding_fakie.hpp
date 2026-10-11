// Port of skate-core/src/animation/riding_fakie.rs.
//! UpdateRidingFakie82BB2330. The two persistent clocks belong to the graph
//! instance, so re-entering another leaf does not restart them.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::riding_fakie {

struct Settings {
    float high_speed = 0.0f;
    float low_speed = 0.0f;
    float slowly_backwards_seconds = 0.0f;
    float after_teleport_seconds = 0.0f;
};
S3_DERIVE_EQ(Settings, high_speed, low_speed, slowly_backwards_seconds, after_teleport_seconds)

struct Physical {
    std::uint32_t category = 0;
    std::uint32_t grind_state = 0;
    bool doing_trick = false;
    /// PhysOutSkeleton+0, as produced by Skeleton::GetData.
    std::array<float, 4> board_axis{};
    std::array<float, 4> deck_velocity{};
    /// PhysOut bundle+36, used outside filtered Ground category.
    std::array<float, 4> external_velocity{};
    float ground_projected_speed = 0.0f;
};

struct State {
    float slowly_backwards = 0.0f;
    float after_teleport = 0.0f;

    /// None preserves the existing fakie flag during grounded/air trick execution.
    std::optional<bool> update(Physical physical, float dt, Settings settings) {
        bool eligible;
        if (physical.category == 5) {
            after_teleport = 0.0f;
            eligible = false;
        } else {
            eligible = after_teleport > settings.after_teleport_seconds;
            after_teleport += dt;
        }
        if (!eligible) {
            slowly_backwards = 0.0f;
            return false;
        }
        const bool ground_or_air = physical.category == 1 || physical.category == 2;
        const bool allowed =
            (ground_or_air || (physical.category == 6 && physical.grind_state == 503)) && !physical.doing_trick;
        if (!allowed) {
            slowly_backwards = 0.0f;
            return !ground_or_air ? std::optional<bool>(false) : std::nullopt;
        }
        const std::array<float, 4>& velocity =
            physical.category == 1 ? physical.deck_velocity : physical.external_velocity;
        const float projection = physics::native_arithmetic::dot3(velocity, physical.board_axis);
        if (physical.ground_projected_speed > settings.high_speed && projection < -0.5f) {
            slowly_backwards = 0.0f;
            return true;
        } else if (physical.ground_projected_speed > settings.low_speed && projection < -0.5f) {
            slowly_backwards = dt + slowly_backwards;
            return slowly_backwards > settings.slowly_backwards_seconds;
        }
        slowly_backwards = 0.0f;
        return false;
    }
};

}  // namespace s3::animation::riding_fakie
