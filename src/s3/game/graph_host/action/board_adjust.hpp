// Port of skate-game/src/graph_host/action_board_adjust.rs (module `action::board_adjust`, #[path] inside action.rs).
//! TU3 BoardAdjust instance+8/+12, Begin82BA2BA8, Update82BA2BC8.
#pragma once
#include <cstdint>
#include <optional>
#include <utility>

#include "s3/core/input/graph_intents.hpp"
#include "s3/core/rs.hpp"

namespace s3::game::graph_host::action::board_adjust {

struct State {
    std::uint32_t wrap = 0;
    float previous_angle = 0.0f;

    void begin() { *this = State{}; }

    std::optional<std::pair<float, float>> update(std::optional<float> magnitude, std::optional<float> angle_,
                                                  std::uint32_t filter, bool negate_on_mirror, bool mirrored) {
        if (!magnitude || !angle_) {
            //82BA2E88: remove both outputs and reset both instance fields.
            begin();
            return std::nullopt;
        }
        float angle = *angle_;
        if (filter <= 7) {
            angle = s3::input::graph_intents::apply_filter(angle, filter);
        }
        if (negate_on_mirror && mirrored) {
            angle *= -1.0f;
        }
        //82BA2D70: crossing the +/-pi seam retains the previous side until
        //the raw angle crosses back. Test the previous RAW angle, not output.
        const bool crossed = previous_angle * angle < 0.0f && rs::abs(previous_angle) > rs::from_bits(0x3fc90fdbu);
        if (wrap == 0 && crossed && previous_angle < 0.0f) {
            wrap = 1;
        } else if (wrap == 0 && crossed && previous_angle > 0.0f) {
            wrap = 2;
        } else if (wrap == 1 && crossed && previous_angle > 0.0f) {
            wrap = 0;
        } else if (wrap == 2 && crossed && previous_angle < 0.0f) {
            wrap = 0;
        }
        previous_angle = angle;
        float output = angle;
        if (wrap == 1) {
            output = rs::from_bits(0xc0490fdbu);  //822F8908
        } else if (wrap == 2) {
            output = rs::from_bits(0x40490fdbu);  //82060C44
        }
        return std::make_pair(*magnitude, output);
    }
};

}  // namespace s3::game::graph_host::action::board_adjust
