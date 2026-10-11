// Port of skate-game/src/physics/animation_grind.rs.
//! Completed physical publication -> both stock graphs. No state selection.
#include "s3/game/physics/animation_grind.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::physics::animation_phase::animation_grind {

using ::s3::animation::skeleton_input::name::encode;
using ::s3::physics::filtered_state::GrindState;

rs::Result<PhysicalStateInputs, std::string> publish(::s3::game::graph_host::motion::MotionHost& host,
                                                     const PhysicalPlayerInput& physical,
                                                     const FilteredStateOutput* filtered, float height,
                                                     bool mirrored, std::array<float, 3> board_forward) {
    S3G_TRY(auto obs, observations(physical, filtered, height, mirrored, board_forward));
    host.grind_physical = std::get<1>(obs);
    host.grind_conditions = std::get<2>(obs);
    return std::move(std::get<0>(obs));
}

rs::Result<std::tuple<PhysicalStateInputs, motion_grind::Physical, motion_grind::conditions::Physical>, std::string>
observations(const PhysicalPlayerInput& physical, const FilteredStateOutput* filtered, float height, bool mirrored,
             std::array<float, 3> board_forward) {
    bool grinding = false;
    GrindState grind{};
    if (filtered != nullptr) {
        if (static_cast<std::uint32_t>(filtered->category) != physical.filtered_state_0) {
            return rs::make_err(std::string("Grind graph received inconsistent completed filtered outputs"));
        }
        grinding = filtered->grinding;
        grind = filtered->grind;
    } else if (physical.filtered_state_0 == 0) {
        // FilteredState reset82DE5588: category0, null names and crouch0.
        // This is the initial reset publication, not a substitute grind state.
        grinding = false;
        grind = GrindState{};
    } else {
        return rs::make_err(std::string("Grind graph requires the completed filtered state owner"));
    }
    S3G_TRY(std::string grind_name, name_text(grind.name));
    motion_grind::Physical grind_observation{
        grinding,
        grind.name,
        // Bundle4 is Motion, not Ground. Basic128 is supplied by the
        // completed BoardMotionOutput effective basis, not Motion128.
        xyz(physical.skateboard.vector_80),
        board_forward,
        physical.ground.flag_273 != 0,
        // Execution refreshes bit30 for the two branches which consume it.
        mirrored,
        height,
        grind.crouch,
        physical.skeleton.twist_504,
    };
    motion_grind::conditions::Physical conditions;
    conditions.filtered_grinding_80 = grinding;
    conditions.blunting_136 = physical.grinds.words_136_140[0];
    conditions.approach_268 = physical.grinds.animation_chromosome_268[0];
    conditions.trick_out_240 = physical.grinds.trick_out_240;
    conditions.air_grind_443 = physical.air.flag_443 != 0;
    conditions.air_time_184 = physical.air.scalar_184;
    conditions.dropping_in_324 = physical.grinds.dropping_in_324 != 0;
    PhysicalStateInputs state;
    state.category = physical.filtered_state_0;
    state.grinding = grinding;
    state.grind_name = std::move(grind_name);
    return std::make_tuple(std::move(state), grind_observation, conditions);
}

std::array<float, 3> xyz(std::array<std::uint32_t, 4> raw) {
    return {rs::from_bits(raw[0]), rs::from_bits(raw[1]), rs::from_bits(raw[2])};
}

rs::Result<std::string, std::string> name_text(AttributeName name) {
    std::string text;
    bool ended = false;
    for (std::uint32_t word : name._0) {
        for (const std::uint32_t weight : {79235168u, 2085136u, 54872u, 1444u, 38u, 1u}) {
            const std::uint32_t digit = word / weight;
            word %= weight;
            if (digit == 0) {
                ended = true;
                continue;
            }
            if (ended) {
                return rs::make_err(std::string("Filtered grind name contains a nonterminal NUL"));
            }
            char byte = 0;
            if (digit >= 1 && digit <= 10) {
                byte = char('0' + (std::uint8_t(digit) - 1));
            } else if (digit >= 11 && digit <= 36) {
                byte = char('A' + (std::uint8_t(digit) - 11));
            } else if (digit == 37) {
                byte = '_';
            } else {
                return rs::make_err(std::string("Filtered grind name is outside the stock alphabet"));
            }
            text.push_back(byte);
        }
    }
    if (encode(std::string_view(text)) != name) {
        return rs::make_err(std::string("Filtered grind name did not round trip"));
    }
    return text;
}

}  // namespace s3::game::physics::animation_phase::animation_grind
