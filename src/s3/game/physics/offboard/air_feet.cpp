// Port of skate-game/src/physics/offboard/air_feet.rs
#include "s3/game/physics/offboard/air_feet.hpp"

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/offboard/biped_air/feet.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::offboard::air_feet {

namespace feet = ::s3::player::offboard::biped_air::recovered::feet;
using sc::physics::skeleton_animation_record::compose_affine;
using ManagerState = sc::player::offboard::board_possession::manager::State;

void update_air(SkaterRuntime& skater, ManagerState& manager) {
    const auto& p = skater.player_input.processed;
    const auto root = skater.animated_skeleton.roots.animation_to_world;
    auto effective = root;
    if ((p.flags_2476 & 4u) != 0) {
        for (std::size_t axis : {std::size_t(0), std::size_t(2)}) {
            for (float& v : effective[axis]) v = -v;
        }
    }
    const auto& pose = skater.animated_skeleton.record.pose;
    const std::array<std::size_t, 2> indices{15, 19};
    std::array<std::array<std::array<float, 4>, 2>, 2> local_pairs{};
    for (std::size_t k = 0; k < 2; ++k) local_pairs[k] = {pose[indices[k]][3], pose[indices[k] + 1][3]};
    std::array<std::array<std::array<float, 4>, 2>, 2> world_pairs{};
    for (std::size_t k = 0; k < 2; ++k) {
        world_pairs[k] = {compose_affine(root, pose[indices[k]])[3], compose_affine(root, pose[indices[k] + 1])[3]};
    }
    for (auto& foot : manager.hands) foot.scalars_96_100[1] = 0.0f;
    manager.words_308_to_316 = {0, 0, 0};
    feet::Input input{};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto line = p.line_tests_960_1008_1056[i];
        input.lines[i] = feet::Line{rs::from_bits(line.position), rs::from_bits(line.normal), line.surface,
                                    line.valid != 0};
    }
    input.root = root;
    input.inverse_root = skater.animated_skeleton.roots.world_to_animation;
    input.effective_root = effective;
    input.local_foot_pairs = local_pairs;
    input.world_foot_pairs = world_pairs;
    input.position = rs::from_bits(p.vectors_544_560_592_608[2]);
    input.velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    input.flags_2476 = p.flags_2476;
    input.flags_2480 = p.flags_2480;
    input.flags_2484 = p.flags_2484;
    input.state = p.state_2508;
    const auto targets = feet::update(manager, input);
    if ((p.flags_2484 & 0x80000000u) == 0) {
        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto& target = targets[index];
            auto& status = skater.foot_ik.state.limbs[index];
            auto& external = skater.foot_ik.state.external_targets[index];
            if (target.world) {
                external.world_position = target.position;
                status.external_target_set = true;
            } else {
                external.animation_position = target.position;
                status.local_target_set = true;
            }
            status.target_blend = target.blend;
            if (target.normal.has_value()) feet::set_normal(external, *target.normal);
        }
    }
}

void enter(SkaterRuntime& skater, ManagerState& manager) {
    const auto& p = skater.player_input.processed;
    if (!(p.state_2504 == 500 || p.state_2504 == 501 || p.state_2504 == 502)) {
        manager.reset();
        skater.foot_ik.state.enable_feet(false);
        for (auto& limb : skater.foot_ik.state.limbs) {
            limb.board_blend = 0.0f;
            limb.external_blend = 0.0f;
            limb.mode = sc::animation::foot_ik::status::Mode::Disabled;
        }
    }
    manager.flags_304_to_307[0] = p.state_2508 == 502;
}

void publish(const ManagerState& manager, sc::player::input_phase::OffBoardOutputFields& output) {
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& foot = manager.hands[i];
        output.flags_306_307[i] = static_cast<std::uint8_t>(
            (foot.flags_104_to_107[1] && !foot.flags_104_to_107[2]) || foot.flags_104_to_107[3]);
    }
}

}  // namespace s3::game::physics::offboard::air_feet
