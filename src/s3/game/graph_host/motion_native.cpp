// Port of skate-game/src/graph_host/motion_native.rs (module `motion_native`).
#include "s3/game/graph_host/motion_native.hpp"

#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/game/graph_host/motion_animation.hpp"

namespace s3::game::graph_host::motion_native {

rs::Result<std::optional<Operation>, std::string> Operation::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    const std::string name = name_ptr ? *name_ptr : std::string();
    if (name == "StoreLandingData") return std::optional<Operation>(StoreLandingData{});
    if (name == "EndShimmy") {
        return std::optional<Operation>(EndShimmy{rs::from_bits(a.float_bits("blendtime", 0x3e4ccccdu))});
    }
    if (name == "SetScoreAugmentation") {
        //82BBFB28's source enum; absent mirrorAugment inherits augment.
        auto decode = [](const std::string& value) -> rs::Result<std::uint32_t, std::string> {
            static const char* const names[] = {"FSPowerslide", "BSPowerslide", "FSRevert", "BSRevert",
                                                "NoseManual",   "TailManual",   "Wipeout",  "Landing",
                                                "RideIdle",     "Switching"};
            for (std::uint32_t n = 0; n < 10; ++n) {
                if (value == names[n]) return n;
            }
            return rs::make_err("Unknown authored score augmentation " + value);
        };
        const std::string* augment = a.text("augment");
        auto regular = decode(augment ? *augment : std::string());
        if (regular.is_err()) return rs::make_err(regular.unwrap_err());
        std::uint32_t mirrored = regular.unwrap();
        if (const std::string* mirror = a.text("mirrorAugment")) {
            //Original82BBFE98 writes the regular slot for this name,
            //leaving the mirror slot uninitialized. No authored stock
            //riding node uses it; do not invent a deterministic value.
            if (*mirror == "Switching") {
                return rs::make_err(std::string("Native mirrorAugment=Switching has no initialized mirrored value"));
            }
            auto decoded = decode(*mirror);
            if (decoded.is_err()) return rs::make_err(decoded.unwrap_err());
            mirrored = decoded.unwrap();
        }
        return std::optional<Operation>(Score{regular.unwrap(), mirrored});
    }
    return std::optional<Operation>();
}

void ScorePacket::set(std::uint32_t value) {
    std::uint32_t bit = 0;
    switch (value) {
        case 0: bit = 31; break;
        case 1: bit = 30; break;
        case 2: bit = 29; break;
        case 3: bit = 28; break;
        case 4: bit = 27; break;
        case 5: bit = 26; break;
        case 6: bit = 23; break;
        case 7: bit = 21; break;
        case 8: bit = 22; break;
        case 9: bit = 20; break;
        default: return;
    }
    flags |= 1u << bit;
    if (value != 2 && value != 3) name = value;
}

void LandingData::update(std::uint32_t category, Physical p, RidingState& riding) {
    const bool air = category == 2;
    if (!was_air && air) riding.last_good_landing_velocity = 0.0f;
    was_air = air;
    const float velocity = s3::riding::ground_correction_math::dot_product(p.centre_of_mass_velocity, p.system_up);
    if (velocity < 0.0f && velocity < previous_velocity) riding.last_good_landing_velocity = rs::abs(velocity);
    previous_velocity = velocity;
}

void end_shimmy(MotionAnimation& animation, float seconds) {
    //82BBCC58; virtual40 uses EndChannelWithTime, not instantaneous removal.
    for (const char* name : {"SKCH_2H_SHIMMY_LEFT_CHANNEL", "SKCH_2H_SHIMMY_RIGHT_CHANNEL"}) {
        animation.channels.end_with(name, seconds, false);
    }
}

}  // namespace s3::game::graph_host::motion_native
