// Port of skate-game/src/physics/ground_runtime/surface.rs.
//! Native wheel surface vote82C08818 on live wheel lines and contact reports.
#include "s3/game/physics/ground_runtime/surface.hpp"

#include "s3/core/physics/contact_feedback.hpp"
#include "s3/game/physics/riding_outputs.hpp"

namespace s3::game::physics::ground_runtime::surface {

std::uint32_t active_surface(const RidingOutputs& riding, const sc::physics::board_runtime::BoardRuntime& board) {
    std::array<bool, 4> contacts{};
    for (std::size_t i = 0; i < 4; ++i) contacts[i] = riding.ground.parts[i].in_contact;
    bool forced = false;
    for (const auto& r : board.contact_reports()) {
        if ((r.other_surface & 0x0f80u) == 0x0600u) {
            forced = true;
            break;
        }
    }
    return sc::physics::contact_feedback::choose_surface(riding.wheel_lines.physics_surfaces, contacts, forced);
}

std::uint32_t active_audio_surface(const RidingOutputs& riding) {
    std::array<bool, 4> contacts{};
    for (std::size_t i = 0; i < 4; ++i) contacts[i] = riding.ground.parts[i].in_contact;
    return choose_audio_surface(riding.wheel_lines.audio_surfaces, contacts);
}

std::uint32_t choose_audio_surface(std::array<std::uint32_t, 4> surfaces, std::array<bool, 4> contacts) {
    std::array<std::uint32_t, 128> votes{};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::uint32_t surface = surfaces[i];
        if (surface != 0) {
            votes.at(surface) += contacts[i] ? 4u : 1u;
        }
    }
    // max_by_key((count, Reverse(surface))) over surfaces 1..: highest count, lowest ID on ties.
    std::size_t best = 1;
    for (std::size_t s = 2; s < votes.size(); ++s) {
        if (votes[s] > votes[best]) best = s;
    }
    if (votes[best] == 0) return 1;
    return static_cast<std::uint32_t>(best);
}

rs::Result<const char*, std::string> surface_key(std::uint32_t mode) {
    switch (mode) {
        case 1: return "smooth";
        case 2: return "rough";
        case 3: return "slow";
        case 4: return "slippery";
        case 5: return "veryslow";
        default:
            return rs::make_err(fmt::cat("Processed surface mode ", mode, " was not normalized by SurfacePhysics"));
    }
}

}  // namespace s3::game::physics::ground_runtime::surface
