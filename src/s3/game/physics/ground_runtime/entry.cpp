// Port of skate-game/src/physics/ground_runtime/entry.rs.
#include "s3/game/physics/ground_runtime/entry.hpp"

#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/core/riding/grounded/manual_entry.hpp"
#include "s3/core/riding/grounded/state/motion.hpp"
#include "s3/core/riding/pumping/geometry.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/foot_ik.hpp"
#include "s3/game/physics/ground_runtime/state.hpp"

namespace s3::game::physics::ground_runtime {

namespace entry {

rs::Result<EntrySettings, std::string> EntrySettings::load(const sd::Collections& data) {
    EntrySettings out;
    //82C091F8 restores the standard angular drag with literal822F860C.
    S3G_TRY(const float drag, data.float_("physicsdeck", "default", "DeckAngularDrag"));
    out.deck_angular_drag = drag * rs::from_bits(0x426fffffu);
    S3G_TRY(out.powerslide_exit, data.float_("physics_manual", "default", "PowerslideExitScalar"));
    S3G_TRY(out.landing_strength, data.float_("physics_feet", "default", "LandingOnDeckEffectScalar"));
    S3G_TRY(out.landing_offset, data.float_("physics_feet", "default", "LandingOnDeckOffset"));
    return out;
}

void restore_standard_board_fields(sc::physics::board_runtime::BoardRuntime& board, std::uint8_t& flags,
                                   float drag) {
    flags &= 0x7f;
    board.bodies_mut()[6].inertia.angular_drag = drag;
    board.set_collision_group(4);
}

namespace {
struct ManualBodies {
    sc::physics::board_runtime::BoardRuntime& board;
    std::array<float, 4> linear_velocity(std::size_t part) {
        const auto v = board.bodies()[part].rates.linear_velocity;
        return {v.x, v.y, v.z, 0.0f};
    }
    void set_linear_velocity(std::size_t part, std::array<float, 4> v) {
        board.bodies_mut()[part].rates.linear_velocity = sc::math::Vector3::new_(v[0], v[1], v[2]);
    }
};
struct Projection {
    using Error = sc::riding::pumping::geometry::Infallible;
    rs::Result<float, Error> normal_speed(std::array<float, 4> n, std::array<float, 4> v) {
        return sc::riding::ground_correction_math::dot_product(n, v);
    }
};
}  // namespace

}  // namespace entry

namespace state {

Status GroundState::enter(sc::physics::board_runtime::BoardRuntime& board,
                          const sc::player::input_phase::ProcessedPhysicsInput& processed,
                          const animation_input::AnimationInput& animation,
                          const sc::physics::board_toolkit::BoardToolkit& toolkit, entry::GroundEntryTargets targets) {
    namespace motion = sc::riding::grounded::state::motion;
    entry::restore_standard_board_fields(board, targets.board_flags_8384, entry_settings.deck_angular_drag);
    targets.foot_ik.enable_feet(true);
    targets.skeleton_elapsed_16505 = false;
    S3G_CHECK(targets.set_skeleton_collision_state(6));
    this->state.begin_entry();
    targets.reckoning_spin_angle = 0.0f;
    targets.reckoning_spin_speed = 0.0f;
    board.hook_mut().drive.disable_animation(targets.board_animated_290);
    const auto normal = rs::from_bits(processed.vectors_464_480_496_512_528[0]);
    const auto angular = rs::from_bits(processed.vectors_720_784_800_816_832_864[0]);
    board.bodies_mut()[6].rates.angular_velocity = motion::entry_angular_velocity(normal, angular);
    speed.target_speed =
        motion::entry_target_speed(board.bodies()[6].rates.linear_velocity, normal, toolkit.forward);
    wobble.reset();
    {
        entry::ManualBodies bodies{board};
        entry::Projection projection;
        sc::riding::grounded::manual_entry::ManualGroundInput input;
        input.balance = animation.fields.balance;
        input.ground_normal = normal;
        input.flags_2468 = processed.flags_2468;
        input.flags_2472 = processed.flags_2472;
        // Infallible: the result is always Ok.
        (void)sc::riding::grounded::manual_entry::enter_ground(manual, processed.category_2516,
                                                               entry_settings.powerslide_exit, input, bodies,
                                                               projection);
    }
    if (processed.category_2516 == 200) {
        const bool stance = (((processed.flags_2468 >> 20) ^ (processed.flags_2476 >> 2)) & 1u) != 0;
        S3G_CHECK(targets.enter_air_landing_modifier(stance));
    }
    if (processed.state_2504 == 503) {
        const auto force = motion::landing_on_deck_force(
            rs::from_bits(processed.vectors_400_416[0]), rs::from_bits(processed.vectors_544_560_592_608[3]),
            rs::from_bits(processed.vectors_544_560_592_608[0]), toolkit.total_mass, entry_settings.landing_strength,
            entry_settings.landing_offset);
        board.forces_mut().append(force);
    }
    //The scalar assignment precedes pumping reset in source; no called
    //operation between those writes reads it.
    this->state.straighten_scale_2672 = processed.state_2504 == 101 ? rs::from_bits(0x3e23d70au) : 1.0f;
    pumping.reset();
    if (targets.wipeout_mode != 1) {
        targets.wipeout_timer = 0.0f;
        targets.wipeout_mode = 1;
    }
    this->state.finish_entry(processed.state_2504);
    entered = true;
    return ok();
}

}  // namespace state

}  // namespace s3::game::physics::ground_runtime
