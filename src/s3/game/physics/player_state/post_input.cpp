// Port of skate-game/src/physics/player_state/post_input.rs.
//! Original PostInput82DB5588 with the live ballistic trajectory selector.
//! The authored world has no grind edges or grab-spline descriptors.
#include "s3/game/physics/player_state/post_input.hpp"

#include "s3/core/player/post_input.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/grind_host.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::player_state::post_input {

using namespace sc::player::post_input;
using sc::player::input_phase::ProcessedPhysicsInput;

PostInputState PostInputState::new_() {
    PostInputState out{};
    // Original Player ctor82DB1AAC=1000,1ACC=0,3028 clears1264.
    out.jump_reference = {0, 0, 0, 0};
    out.jump_fix_frames = 1000;
    out.latch_frames = 0;
    out.state_frames = 100;
    out.heading_adjust = 0.0f;
    out.complete = false;
    out.trajectory_pending = false;
    out.trajectory_valid = false;
    out.trajectory_available = false;
    out.trajectory_new_candidate = false;
    return out;
}

EmptyEdgePost EmptyEdgePost::new_() { return EmptyEdgePost{}; }

void EmptyEdgePost::advance(ProcessedPhysicsInput& p) {
    // No candidate means82D8ACF0 skips wallassist,82D86318 skips geometry.
    expiry = rs::max(expiry - p.timestep_2604, 0.0f);
    free_frames = free_frames + 1u;
    if (free_frames > 20) {
        duration = 0;
        engage_state = 2;
    }
    pop_frames = decrement(pop_frames);
    for (float& value : lean) {
        value *= rs::from_bits(0x3f68f5c3u);
        if (rs::abs(value) < 0.0001f) value = 0.0f;
    }
    reset_frames = decrement(reset_frames);
    if (reset_frames > 0) p.flags_2476 |= 0x02000000u;
    if (p.grind_words_2532_2536[0] != UINT32_MAX) previous_grind = p.grind_words_2532_2536[0];
    blend = rs::min(blend + 0.0035f, 1.0f);
}

std::uint32_t decrement(std::uint32_t v) {
    const std::uint32_t n = v - 1u;
    return (n & 0x80000000u) != 0 ? 0u : n;
}

namespace {

struct Services final : PostInputServices {
    float heading;
    player_input::grind::GrindInputState& grind;
    std::optional<player_input::grind::Pending> pending;
    player_input::grind::PostContext grind_context;
    grind_host::LiveHost host;
    std::optional<player_input::grind::PostResult> result;
    ProcessedPhysicsInput& processed;
    air_trajectory::AirTrajectoryRuntime& trajectory;
    sc::air::trajectory::SelectorInput trajectory_input;
    std::array<float, 4> trajectory_board_position;
    const sc::physics::board_world::BoardWorld& world;
    std::array<std::optional<sc::player::offboard::grab_scene::Record>, 2> grab_records;
    std::optional<std::string> error;

    Services(float heading_, player_input::grind::GrindInputState& grind_,
             std::optional<player_input::grind::Pending> pending_, player_input::grind::PostContext grind_context_,
             grind_host::LiveHost host_, ProcessedPhysicsInput& processed_,
             air_trajectory::AirTrajectoryRuntime& trajectory_, sc::air::trajectory::SelectorInput trajectory_input_,
             std::array<float, 4> trajectory_board_position_, const sc::physics::board_world::BoardWorld& world_,
             std::array<std::optional<sc::player::offboard::grab_scene::Record>, 2> grab_records_)
        : heading(heading_),
          grind(grind_),
          pending(std::move(pending_)),
          grind_context(grind_context_),
          host(host_),
          processed(processed_),
          trajectory(trajectory_),
          trajectory_input(trajectory_input_),
          trajectory_board_position(trajectory_board_position_),
          world(world_),
          grab_records(std::move(grab_records_)) {}

    void update_grind_manager_82d8ab08() override {
        if (!pending.has_value()) {
            error = std::string("Grind post-input requires this tick's pre-input queries");
            return;
        }
        auto taken = std::move(*pending);
        pending = std::nullopt;
        auto r = grind.post_update(processed, world, std::move(taken), grind_context, host);
        if (r.is_ok()) {
            result = std::move(r.unwrap());
        } else {
            error = r.unwrap_err();
        }
    }
    std::uint8_t update_trajectory_selector_82d68800() override {
        // 82DB56B0 runs the real selector after GrindManager. That manager
        // can change2476, so use its current output rather than a prior snapshot.
        auto input = trajectory_input;
        input.flags_2476 = processed.flags_2476;
        const auto grind_ctx = air_trajectory::GrindContext::from_processed(processed, trajectory_board_position);
        auto r = trajectory.update(input, world, grind_ctx);
        if (r.is_ok()) return std::uint8_t(r.unwrap());
        error = r.unwrap_err();
        return 0;
    }
    float calculate_scalar_2740_82db5e10() override { return heading; }
    void register_candidate_82762ab0(CandidateRegistration registration) override {
        const std::size_t index = registration == CandidateRegistration::First1888 ? 0 : 1;
        if (grab_records[index].has_value()) {
            copy_grab_record_82762ab0(processed.grab_records_1888_2176[index], grab_records[index]->_0);
        } else {
            error = std::string("Grab publication requested without a completed record");
        }
    }
};

}  // namespace

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_TRY(const auto trajectory_input, air_phase::selector_input(physics, skater));
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Post-input trajectory requires this tick's board toolkit");
    const auto trajectory_board_position = toolkit->deck[3];
    auto& post = skater.player_state.post;
    auto& p = skater.player_input.processed;
    // 82DB573C ->82D740F8 consumes the one live owner's completed results.
    auto publication = skater.offboard_grab.publish();
    const std::optional<std::uint32_t> object_flat =
        publication.object.has_value() ? *publication.object : std::optional<std::uint32_t>{};
    CandidatePublicationFields candidates{};
    candidates.first_object_present_196 = publication.records[0].has_value();
    candidates.first_pending_288 = publication.records[0].has_value();
    candidates.second_object_present_500 = publication.records[1].has_value();
    candidates.second_pending_592 = publication.records[1].has_value();
    candidates.staged_word_12768 = object_flat.value_or(p.object_2464);
    candidates.staged_valid_12772 = std::uint32_t(object_flat.has_value());
    candidates.staged_latched_12776 = std::uint32_t(skater.offboard_grab.interactable_latched);
    candidates.staged_pending_12780 = publication.object.has_value();
    PostInputPlayerFields player{};
    player.jump_reference_1264 = post.jump_reference;
    player.flags_1296 = skater.player_input.player.flags_1296;
    player.state_frames_1304 = static_cast<std::uint32_t>(skater.player_input.player.ground_history_frames_1304);
    player.jump_fix_frames_1308 = post.jump_fix_frames;
    player.latch_frames_1320 = post.latch_frames;
    PostInputProcessedFields processed{};
    processed.jump_reference_848 = post.jump_reference;
    processed.word_2464 = p.object_2464;
    processed.flags_2468 = p.flags_2468;
    processed.flags_2472 = p.flags_2472;
    processed.flags_2480 = p.flags_2480;
    processed.flags_2484 = p.flags_2484;
    processed.current_state_2508 = p.state_2508;
    processed.state_frames_2572 = post.state_frames;
    processed.jump_fix_frames_2576 = post.jump_fix_frames;
    processed.scalar_2740 = post.heading_adjust;
    PostInputPhysOutFields output{};
    output.reset_state_frames_316 =
        skater.player_state.ground_output.has_value() && skater.player_state.ground_output->ground_32.wall_ride_exit;
    // 82DB5588 consumes the preceding state's actual jump publication.
    output.capture_jump_reference_442 = skater.player_input.physical.air.launched_442 != 0;
    output.jump_reference_128 = skater.player_input.physical.air.launch_velocity_128;
    output.complete_76 = post.complete;
    const float heading =
        physics.riding.update_input_heading(rs::from_bits(p.vectors_464_480_496_512_528[0][1]), p.scalar_2612);
    player_input::grind::PostContext grind_context{};
    grind_context.board = solve::deck_frame(physics.board);
    grind_context.balance_2720 = skater.animation_input.fields.balance;
    grind_context.translation_2796 = skater.animation_input.extra.grind_translation;
    grind_context.stability_nudge_2800 = skater.animation_input.extra.grind_stability_nudge;
    grind_context.up_down_2804 = skater.animation_input.extra.grind_up_down;
    grind_context.grab_min_height_2808 = skater.animation_input.extra.grind_grab_min_height;
    auto pending = std::move(skater.player_input.pending_grind);
    skater.player_input.pending_grind = std::nullopt;
    Services services(heading, skater.player_input.grind, std::move(pending), grind_context,
                      grind_host::LiveHost(physics.board, physics.settings, physics.grind_materials), p,
                      skater.trajectory, trajectory_input, trajectory_board_position, physics.world,
                      std::move(publication.records));
    run_post_input(PostInputContext{player, processed, output, candidates}, services);
    if (services.error.has_value()) {
        std::string error = std::move(*services.error);
        services.error = std::nullopt;
        return rs::make_err(std::move(error));
    }
    if (!services.result.has_value())
        return rs::make_err(std::string("Grind post-input did not publish its result"));
    const auto grind_result = std::move(*services.result);
    services.result = std::nullopt;
    processed.flags_2468 |= services.processed.flags_2468 & 0x0004'0000u;
    for (const std::size_t reason : grind_result.wipeout_reasons) skater.wipeout.state.request(reason, 0.0f);
    skater.grind.observe(grind_result.observation);
    skater.player_input.grind_observation = grind_result.observation;
    const auto g = p.grind;
    if ((g.flags_1516 & 0x0800'0000u) != 0) {
        auto xyz = [](std::array<std::uint32_t, 4> v) {
            return sc::math::Vector3::new_(rs::from_bits(v[0]), rs::from_bits(v[1]), rs::from_bits(v[2]));
        };
        skater.ground_lifecycle.edge =
            ground_phase::GroundEdge{g.flags_1516, rs::from_bits(g.vector_1232), xyz(g.second_start_1312),
                                     xyz(g.second_end_1328)};
    } else {
        skater.ground_lifecycle.edge = std::nullopt;
    }
    // These are observations of the actual owner, never inputs to selection.
    post.trajectory_pending = skater.trajectory.selector.pending();
    post.trajectory_valid = skater.trajectory.selector.valid();
    post.trajectory_available = skater.trajectory.selector.valid();
    post.trajectory_new_candidate = skater.trajectory.selector.just_changed();
    skater.player_input.player.flags_1296 = player.flags_1296;
    skater.player_input.player.ground_history_frames_1304 = static_cast<std::int32_t>(player.state_frames_1304);
    p.flags_2468 = processed.flags_2468;
    p.flags_2472 = processed.flags_2472;
    p.flags_2480 = processed.flags_2480;
    p.flags_2484 = processed.flags_2484;
    p.object_2464 = processed.word_2464;
    post.jump_reference = player.jump_reference_1264;
    post.jump_fix_frames = player.jump_fix_frames_1308;
    post.latch_frames = player.latch_frames_1320;
    post.state_frames = processed.state_frames_2572;
    post.heading_adjust = processed.scalar_2740;
    post.complete = output.complete_76;
    return ok();
}

}  // namespace s3::game::physics::player_state::post_input
