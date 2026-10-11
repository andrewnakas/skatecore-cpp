// Port of skate-core/src/player/wipeout_state/recovery.rs (whole file).
#include "s3/core/player/wipeout_state/recovery.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::player::wipeout_state::data {

using physics::native_arithmetic::dot3;

namespace {
const float DT = rs::from_bits(0x3C888889u);
}

float State::response_strength() const {
    const float speed = maximum_speed * 0.1f;
    const float count = static_cast<float>(static_cast<std::int32_t>(4u - response_count));
    const float remaining = (count + 1.0f) * 0.5f;
    const float result = speed - remaining >= 0.0f ? speed : remaining;
    if (slow_time > 1.0f || result <= 0.2f || special_surface || below_surface) {
        return 0.0f;
    }
    return result;
}

void State::manage_recovery(std::uint32_t flags2468, std::uint32_t flags2472, std::uint32_t flags2484,
                            float timestep, const recovery::Settings& settings) {
    prevent_manual |= (flags2468 & 1u) != 0;
    ignore_reset |= (flags2472 & 0x80000000u) != 0;
    reset_ever |= (flags2472 & 0x00100000u) != 0;
    ever_settled |= over;
    recovery_eligible |= ever_settled || time > 1.6f;
    settled_time = over ? settled_time + DT : 0.0f;
    if (teleport_countdown < 0 && should_teleport(flags2472, flags2484, timestep, settings)) {
        teleport_countdown = 2;
    }
    if (teleport_countdown == 0) {
        request_teleport = true;
    } else if (teleport_countdown > 0) {
        teleport_countdown -= 1;
    }
}

bool State::should_teleport(std::uint32_t flags2472, std::uint32_t flags2484, float timestep,
                            const recovery::Settings& settings) {
    if (ever_impaled) {
        impaled_time += DT;
    }
    if ((flags2484 & 0x00400000u) != 0) {
        ever_impaled = true;
        if (impaled_time > 0.5f) {
            return true;
        }
    }
    if (prevent_manual) {
        return false;
    }
    if (recovery_eligible && reset_ever && !ignore_reset) {
        return true;
    }
    if (teleport_pending) {
        time_until_teleport -= timestep;
        return time_until_teleport <= 0.0f;
    }
    bool automatic;
    if ((flags2472 & 0x10000000u) != 0) {
        automatic = time > 8.0f;
    } else {
        automatic = (time > settings.maximum_time && response_time > settings.maximum_time) ||
                    (time > settings.minimum_time && settled_time > settings.minimum_settled &&
                     response_time > settings.minimum_settled);
    }
    if (automatic) {
        teleport_pending = true;
        time_until_teleport = settings.fade_time;
    }
    return false;
}

void State::post_physics(std::array<float, 4> hips_velocity, std::array<float, 4> neck_velocity,
                         bool support_contact, const recovery::Settings& settings) {
    const bool bypass_neck = extra_weight_zero_time > 0.5f;
    const float speed = bypass_neck ? settings.over_speed * 2.0f : settings.over_speed;
    const bool minimum_time = time > settings.over_minimum_time;
    const float hips_squared = dot3(hips_velocity, hips_velocity);
    const float neck_squared = dot3(neck_velocity, neck_velocity);
    over = hips_squared < speed * speed && (neck_squared < speed * speed || bypass_neck) && minimum_time &&
           response_strength() == 0.0f;
    slow = hips_squared < 1.0f && minimum_time;
    no_support_time = support_contact ? 0.0f : no_support_time + DT;
}

}  // namespace s3::player::wipeout_state::data
