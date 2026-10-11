// Port of skate-game/src/physics/grind/condition.rs.
#include "s3/game/physics/grind/condition.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::condition_ {

using ::s3::physics::board::BodyId;

/// Run on EVERY completed publication, not merely physical grind frames.
///82DF2130 installs chromosome at slot1 and filtered at slot5;82DF2710
///dispatches in ascending order. Board/Skeleton/state Fill must precede this.
Status condition(const GamePhysics& physics, SkaterRuntime& skater) {
    auto& out = skater.player_input.physical.grinds;
    //Common82DB7948..7968 derives this after state Fill, not from category400.
    out.grinding_316 = (out.words_136_140[1] == 1 || out.words_136_140[1] == 2) ? 1 : 0;
    std::optional<Family> family;
    switch (out.words_136_140[0]) {
        case 0: family = Family::FiftyFifty; break;
        case 1: family = Family::Boardslide; break;
        case 2: family = Family::Tipslide; break;
        case 3: family = Family::FiveO; break;
        case 4: family = Family::Backslash; break;
        case 5: family = Family::Darkslide; break;
        //The reset template uses u32::MAX. No family is normal outside grind;
        //history and previous-category bookkeeping must still run this frame.
        default: family = std::nullopt; break;
    }
    if (out.grinding_316 != 0 && !family) {
        return rs::make_err(fmt::cat("Active grind has invalid native family ", out.words_136_140[0]));
    }
    //Board Fill82C02AD8/B14 copies the unadjusted part transform. Its effective
    //variant82C01BF8 supplies Basic96 and128; neither is the Skeleton forward.
    const auto deck = physics.board.part_transforms()[::s3::physics::board::index(BodyId::Deck)];
    auto axis = [](std::array<float, 3> a) { return std::array<float, 4>{a[0], a[1], a[2], 0.0f}; };
    const auto effective_forward = axis(physics.riding.motion.effective_basis.columns[2]);
    //Basic144 comes from the dynamics COM at82C02D0C, not part translation48.
    const auto com = physics.board.bodies()[::s3::physics::board::index(BodyId::Deck)].rates.position;
    grind_chromosome::Input input;
    input.category = skater.player_input.physical.state.category_12;
    input.grinding_316 = out.grinding_316 != 0;
    //Common82DB7480 copies Processed2468 bit21 to Air439 on every frame.
    input.air_event_439 = (skater.player_input.processed.flags_2468 & 0x00200000u) != 0;
    input.family = family;
    input.basic_right_0 = axis(deck.basis.columns[0]);
    input.basic_forward_32 = axis(deck.basis.columns[2]);
    input.basic_position_48 = {deck.translation.x, deck.translation.y, deck.translation.z, 0.0f};
    input.basic_location_axis_96 = effective_forward;
    input.basic_twist_axis_128 = effective_forward;
    input.basic_location_position_144 = {com.x, com.y, com.z, 0.0f};
    //82BE1D00..1E58: physical pose starts8016;9232=part19,8976=part15.
    //256/272 receive column0, not toe positions or IK/rendered bones.
    input.feet_256_272 = {skater.skeleton.record.pose[19][0], skater.skeleton.record.pose[15][0]};
    //Common82DB7A90 copies the completed animation packet's byte10372.
    input.fakie_155 = skater.animation.packet.riding_fakie;
    input.point = rs::from_bits(out.point_16);
    input.direction = rs::from_bits(out.direction_0);
    input.normal = rs::from_bits(out.normal_32);
    input.across = rs::from_bits(out.across_48);
    return skater.grind.condition_outputs(input, out);
}

}  // namespace s3::game::physics::grind::condition_
