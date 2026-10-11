// Port of skate-core/src/player/wipeout/requests.rs (whole file).
// Original request arrays20/56,count200,mode208 and timers192/196/204.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/player/wipeout/data.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::wipeout::requests {

struct Requests {
    std::array<bool, 34> reasons{};
    std::array<float, 34> values{};
    std::uint32_t count = 0;
    float cooldown = 0.0f;
    std::int32_t contact_frames = 0;
    float balance = 0.0f;
    std::uint32_t mode = 0;

    /// Player ctor82DB20B8..D8 and first ClearRequests initialize this storage.
    static Requests new_() { return Requests{}; }
    /// Player initialization82DB3024 supplies the short initial grace period.
    void initialize_player() { cooldown = rs::from_bits(0x3d23d70au); }
    /// Teleport82DB8DB4 overwrites only the cooldown; ResetSystems is separate.
    void teleport() { cooldown = rs::from_bits(0x3ecccccdu); }
    /// Ground Enter82D378E0..F4 and CheckForGroundWipeout share this state.
    void enter_ground() {
        if (mode != 1) {
            balance = 0.0f;
            mode = 1;
        }
    }
    /// A repeated request increments the original counter even if its bit was set.
    void request(std::size_t index, float value) {
        S3_ASSERT(index < 34);
        reasons[index] = true;
        values[index] = value;
        count = count + 1u;
    }
    /// SystemLogic82DB6000 after state selection: preserve cooldown and histories.
    void clear_after_selection() {
        reasons.fill(false);
        values.fill(0.0f);
        count = 0;
    }
    /// ResetSystems82DB9354..9360 clears mode and count only.
    void reset_systems() {
        mode = 0;
        count = 0;
    }
    /// 82DB9188: count reflects individual triggers, not unique reasons.
    bool requests_runout(const data::RequestInput& frame) const {
        return (count == 1 && (reasons[2] || reasons[24] || reasons[10]) &&
                (frame.flags_2468 & (1u << 5)) == 0 && (frame.flags_2476 & (1u << 26)) == 0 &&
                rs::abs(frame.animation_up_y) > rs::from_bits(0x3f4f5c29u)) ||
               ((frame.flags_2484 & (1u << 12)) != 0 && frame.category == 400);
    }
    /// 82DB9100 first excludes runout, then combines reason OR with2480bit3.
    bool requests_wipeout(const data::RequestInput& frame) const {
        bool any = false;
        for (bool v : reasons) any = any || v;
        return !requests_runout(frame) && (any || (frame.flags_2480 & (1u << 3)) != 0);
    }
};
S3_DERIVE_EQ(Requests, reasons, values, count, cooldown, contact_frames, balance, mode)

}  // namespace s3::player::wipeout::requests

namespace s3::player::wipeout {
using requests::Requests;
}  // namespace s3::player::wipeout
