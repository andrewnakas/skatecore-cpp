// Port of skate-game/src/graph_host/motion_landing_execute.rs.
#include "s3/game/graph_host/motion_landing_execute.hpp"

#include <algorithm>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion_landing_execute {

namespace {

void set(MotionAnimation& animation, const char* name, float value) {
    animation.set_attribute(s3::animation::playback_parameters::SettableAttribute{
        s3::animation::skeleton_input::name::encode(name), value, false, -1});
}

}  // namespace

rs::Result<rs::Unit, std::string> execute(const Operation& operation, State& state, Flags& flags,
                                          MotionAnimation& animation, const MotionRandom& random,
                                          std::optional<Physical> physical, std::optional<bool> mirrored, float dt,
                                          std::uint8_t phase) {
    using s3::animation::skeleton_input::name::encode;
    if (std::holds_alternative<Operation::IsAnticipating>(operation.value)) {
        //82BACB40/82BACB90: v80 ->8258F870, CA4 bit30.
        if (phase != 1) flags.anticipating = phase == 0;
    } else if (std::holds_alternative<Operation::IsLanding>(operation.value)) {
        //82BACBE0/82BACC30: v88 ->8258F898, CA4 bit29.
        if (phase != 1) flags.landing = phase == 0;
    } else if (std::holds_alternative<Operation::IsManualing>(operation.value)) {
        //82BACC80/82BACCD0: v96 ->8258F8C0, CA4 bit28.
        if (phase != 1) flags.manualing = phase == 0;
    } else if (std::holds_alternative<Operation::IsDoingTrick>(operation.value)) {
        //82BBDFF8/82BBE048: v104 ->8258F8E8, CA4 bit27.
        if (phase != 1) flags.doing_trick = phase == 0;
    } else if (std::holds_alternative<Operation::SetLandingData>(operation.value)) {
        switch (phase) {
            case 0: {
                //82BB0180: sign follows ISkaterAnim v28, then MG v36.
                if (!physical) return rs::make_err(std::string("SetLandingData requires actual landing publication"));
                const Physical& p = *physical;
                if (!mirrored) return rs::make_err(std::string("SetLandingData requires animation stance"));
                set(animation, "Spin", *mirrored ? p.spin : -p.spin);
                set(animation, "AvgVelY", p.last_good_landing_velocity);
                state.value = p.height;
                break;
            }
            case 1: set(animation, "disttocog", state.value); break;  //82BB0338.
            default: break;                                            //Original End82B61BB8 is empty.
        }
    } else if (const auto* disable = std::get_if<Operation::DisableTricks>(&operation.value)) {
        switch (phase) {
            case 0:
                //82BB8DC8.
                flags.tricks_allowed = false;
                state.value = disable->length;
                state.complete = false;
                break;
            case 1:
                //82BB8E58; strict comparison, no reference counting.
                if (!state.complete) {
                    state.value -= dt;
                    if (state.value < 0.0f) {
                        flags.tricks_allowed = true;
                        state.complete = true;
                    }
                }
                break;
            default: flags.tricks_allowed = true; break;  //82BB8F20 unconditionally.
        }
    } else if (const auto* choose = std::get_if<Operation::ChooseRandomLanding>(&operation.value)) {
        switch (phase) {
            case 0: {
                //82BB0398 draws from the SAME specific MotionGraph RNG v256.
                //The original traps for a nonpositive divisor; reject that asset.
                const std::int32_t count = choose->count;
                if (count <= 0) return rs::make_err(std::string("ChooseRandomLanding requires numlandings > 0"));
                auto next = random.next_u32();
                if (next.is_err()) return rs::make_err(std::string(next.unwrap_err()));
                // (x as i32).wrapping_abs() % count
                const std::int32_t signed_value = static_cast<std::int32_t>(next.unwrap());
                const std::int32_t magnitude =
                    signed_value < 0 ? static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(signed_value))
                                     : signed_value;
                const std::int32_t value = magnitude % count;
                const auto name = encode("Random");
                const auto encoded = encode(std::to_string(value));
                auto& values = animation.construction_values;
                auto found = std::find_if(values.begin(), values.end(),
                                          [&](const auto& item) { return item.first == name; });
                if (found != values.end()) {
                    found->second = encoded;
                } else {
                    values.emplace_back(name, encoded);
                }
                break;
            }
            case 2: {
                const auto name = encode("Random");
                auto& values = animation.construction_values;
                values.erase(std::remove_if(values.begin(), values.end(),
                                            [&](const auto& item) { return item.first == name; }),
                             values.end());
                break;
            }
            default: break;  //Update82B61BB8 is empty; End82BB0498 erases the key.
        }
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_landing_execute
