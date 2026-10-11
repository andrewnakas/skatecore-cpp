// Port of skate-game/src/physics/player_state.rs.
//! PhysicalPlayer startup, PostInput, state selection and state publication.
//! Native control decisions use original TU3 functions and stock collections;
//! host ownership replaces the original player component pointers.
#include "s3/game/physics/player_state.hpp"

#include "s3/core/player/offboard/contact_toolkit.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/foot_ik_queries.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::player_state {

using sc::player::selector::conditions::TwoStageThresholds;

rs::Result<PlayerState, std::string> PlayerState::load(const sd::Collections& data, std::string_view /*mode*/) {
    auto thresholds = [&](const char* cls) -> rs::Result<TwoStageThresholds, std::string> {
        TwoStageThresholds t{};
        S3G_TRY(t.field_856_primary, data.float_(cls, "default", "NaturalAirMaxDist"));
        S3G_TRY(t.field_856_secondary, data.float_(cls, "default", "NaturalAirMinDist"));
        S3G_TRY(t.field_7692, data.float_(cls, "default", "NaturalAirTime"));
        return t;
    };
    PlayerState out{};
    out.registry = registry::StateRegistry::new_();
    // 82DB3008 selects the owned Sleeping object before SetPhysicsState100.
    out.lifecycle = sc::player::lifecycle::PhysicalPlayerStateLifecycle::new_(PhysicalStateId::Sleeping);
    out.selector = sc::player::selector::StateSelector{};
    out.requested_state = PhysicalStateId::Sleeping;
    out.filtered = sc::physics::filtered_state::FilteredState{};
    out.filtered_output = std::nullopt;
    out.ground_output = std::nullopt;
    out.post = post_input::PostInputState::new_();
    out.state_flags.fill(false);
    out.state_count = 0;
    out.update_count = 0;
    // 82D8BD50 reads Globals260; its layout392/396/400 is physics_airstates.
    S3G_TRY(out.normal_off_ground, thresholds("physics_airstates"));
    S3G_TRY(out.skitching_off_ground, thresholds("physics_state_skitching"));
    // Skeleton+24 physics_animation, original82BDC6C8 layout800.
    S3G_TRY(out.animated_board_threshold,
            data.float_("physics_animation", "default", "MaxDeckZAxisYForAnimatedDeck"));
    out.initialized = false;
    return out;
}

PhysicalStateId PlayerState::current() const { return lifecycle.active().state; }

void PlayerState::reset_for_teleport() {
    filtered.reset();
    filtered_output = std::nullopt;
    ground_output = std::nullopt;
    // ResetSystems82DB92D0 clears20..44, sets48=10 and52=0.
    // Current pointer16 and flags56/57 survive until Calculate writes them.
    selector.nonspecific_collision_free_frames = 0;
    selector.nonspecific_collision_frames = 0;
    selector.something_colliding_frames = 0;
    selector.two_wheel_counter = 0;
    selector.three_wheel_counter = 0;
    selector.post_grind_jump_counter = 0;
    selector.air_frames = 0;
    selector.teleport_countdown = 10;
    selector.skitch_exit_countdown = 0;
    // TrajectorySelector::Reset82D67228 clears9658, preserving9657.
    post.trajectory_available = false;
}

Status initialize(GamePhysics& physics, SkaterRuntime& skater) {
    if (skater.player_state.initialized) return ok();
    S3G_CHECK(transition::set(physics, skater, PhysicalStateId::PhysicsGround));
    skater.player_state.initialized = true;
    return ok();
}

Status post_input_and_select(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_CHECK(complete_post_input(physics, skater));
    const auto processed = skater.player_input.processed_snapshot(physics.ticks);
    return selection::advance(physics, skater, processed);
}

Status complete_post_input(GamePhysics& physics, SkaterRuntime& skater) { return post_input::advance(physics, skater); }

Status publish(GamePhysics& physics, SkaterRuntime& skater) { return publication::publish(physics, skater); }

Status enter_after_teleport(GamePhysics& physics, SkaterRuntime& skater) {
    const auto manual = skater.teleport_state.take_manual_on_board();
    const PhysicalStateId target =
        (manual.has_value() && *manual == false) ? PhysicalStateId::BipedGround : PhysicalStateId::PhysicsGround;
    return transition::set(physics, skater, target);
}

Status pre_state(GamePhysics& physics, SkaterRuntime& skater) { return pre_state_::advance(physics, skater); }

Status resume_after_climb(GamePhysics& physics, SkaterRuntime& skater) {
    biped_ground::exit(skater);
    skater.offboard_air_selector.reset();
    skater.collision_extra_errors = {};
    skater.animated_skeleton.motion.velocity_world = {0.0f, 0.0f, 0.0f, 0.0f};
    skater.player_input.processed.vectors_544_560_592_608[3] = {0, 0, 0, 0};
    if (skater.player_state.current() == PhysicalStateId::BipedGround) {
        S3G_CHECK(biped_ground::enter(physics, skater));
    } else {
        S3G_CHECK(transition::set(physics, skater, PhysicalStateId::BipedGround));
    }
    S3G_TRY(const auto tests, foot_ik_queries::query(physics.world, skater.skeleton));
    tests.publish(skater.player_input.player);
    const auto frame = skater.biped_ground.ground.frame_80;
    sc::player::offboard::contact_toolkit::Input input{};
    input.position = frame[3];
    input.right = frame[0];
    input.up = frame[1];
    input.forward = frame[2];
    input.animation_right = frame[0];
    input.animation_up = frame[1];
    input.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    S3G_TRY(const auto scene, offboard::contact_toolkit::StaticScene::new_(physics.world));
    S3G_CHECK(skater.offboard_contact.submit(
        input, static_cast<std::int32_t>(skater.player_input.processed.actor_query_2952), scene));
    return ok();
}

rs::Result<bool, std::string> apply_vehicle_ejection(GamePhysics& physics, SkaterRuntime& skater) {
    const auto ejection = skater.teleport_state.take_vehicle_ejection();
    if (!ejection.has_value()) return false;
    const auto [v, a] = *ejection;
    S3G_CHECK(transition::set(physics, skater, PhysicalStateId::WipeoutGround));
    const auto velocity = sc::math::Vector3::new_(v[0], v[1], v[2]);
    const auto angular = sc::math::Vector3::new_(a[0], a[1], a[2]);
    for (auto& body : skater.skeleton.bodies_mut()) {
        body.rates.linear_velocity = velocity;
        body.rates.angular_velocity = angular;
    }
    for (auto& body : physics.board.bodies_mut()) {
        body.rates.linear_velocity = velocity;
        body.rates.angular_velocity = angular;
    }
    skater.animated_skeleton.motion.velocity_world = {velocity.x, velocity.y, velocity.z, 0.0f};
    skater.player_input.processed.vectors_544_560_592_608[3] =
        rs::to_bits(std::array<float, 4>{velocity.x, velocity.y, velocity.z, 0.0f});
    skater.wipeout_state.state.velocity = {velocity.x, velocity.y, velocity.z, 0.0f};
    return true;
}

}  // namespace s3::game::physics::player_state
