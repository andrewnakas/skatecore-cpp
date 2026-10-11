// Port of the tests in skate-core/src/player/input_phase/{tests.rs, types.rs (snapshot_tests),
// deck_angles_tests.rs}.
#include <cmath>
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/input_phase.hpp"

namespace {
using namespace s3::player::input_phase;
using s3::animation::output::actor_packet::ExternalPhysicsInput;
using s3::input::animation_packet::AnimationPacketFields;
namespace rs = s3::rs;

ExternalPhysicsInput external(std::uint32_t flags) {
    ExternalPhysicsInput e{};
    e.flags = flags;
    return e;
}

AnimationPacketFields animation_publication() {
    AnimationPacketFields f{};
    f.timestep = 0.25f;
    return f;
}

AnimationInputPacket packet(const AnimationPacketFields& publication, const ExternalPhysicsInput& external_physics) {
    return AnimationInputPacket{publication, external_physics};
}

using Error = rs::Unit;  // Rust `type Error = ();`

struct Services : InputPhaseServices<Error> {
    std::vector<std::string> calls;
    std::optional<std::pair<std::uint32_t, std::uint32_t>> manager_state_change;
    bool actor_input_available = false;

    R<rs::Unit> update_pre_input_manager_82d81610(PlayerInputState&, PhysicalPlayerInput& physical) override {
        calls.push_back("manager");
        if (manager_state_change) {
            physical.state.state_16 = manager_state_change->first;
            physical.state.category_12 = manager_state_change->second;
        }
        return rs::Unit{};
    }
    R<rs::Unit> reset_processed_input_82bf9ef0(ProcessedPhysicsInput& output) override {
        calls.push_back("reset");
        output = ProcessedPhysicsInput{};
        return rs::Unit{};
    }
    R<std::uint32_t> actor_query_slot_56() override {
        calls.push_back("actor56");
        return 56u;
    }
    R<std::uint32_t> actor_query_slot_44() override {
        calls.push_back("actor44");
        return 44u;
    }
    R<rs::Unit> reset_player_probe_82d7a330(PlayerInputState& player, PhysicalPlayerInput&) override {
        calls.push_back("probe_reset");
        player.probe.byte_104 = 0;
        return rs::Unit{};
    }
    R<rs::Unit> check_teleport_82db88c8(PlayerInputState&, PhysicalPlayerInput&, ProcessedPhysicsInput&) override {
        calls.push_back("teleport");
        return rs::Unit{};
    }
    R<bool> actor_input_available_slot_4() override {
        calls.push_back("actor_input");
        return actor_input_available;
    }
    R<float> transition_action_825903c8() override {
        calls.push_back("transition");
        return 2.0f;
    }
    R<RawVector> calculate_ground_position_82c02840(const PhysicalPlayerInput&) override {
        calls.push_back("ground_position");
        return rs::to_bits(std::array<float, 4>{2.0f, 4.0f, 6.0f, 0.0f});
    }
    R<rs::Unit> prepare_board_toolkit_82c013f0(PlayerInputState&, PhysicalPlayerInput&,
                                               ProcessedPhysicsInput&) override {
        calls.push_back("board_toolkit");
        return rs::Unit{};
    }
    R<rs::Unit> process_skeleton_82bd8918(const AnimationInputPacket&, PhysicalPlayerInput&,
                                          ProcessedPhysicsInput& output) override {
        calls.push_back("skeleton");
        output.flags_2472 |= (1u << 21) | (1u << 23);
        output.flags_2476 |= 1u << 22;
        output.spin_input_2672 = 2.0f;
        output.crouch_2776 = 3.0f;
        return rs::Unit{};
    }
    R<rs::Unit> update_grind_manager_82d8a828(PlayerInputState&, PhysicalPlayerInput&,
                                              ProcessedPhysicsInput&) override {
        calls.push_back("grind");
        return rs::Unit{};
    }
};

bool contains(const std::vector<std::string>& v, const char* s) {
    for (const auto& e : v) {
        if (e == s) return true;
    }
    return false;
}

S3_TEST(complete_phase_keeps_native_service_order_and_persistent_counters) {
    const auto publication = animation_publication();
    const auto external_physics = external(0);
    const auto pkt = packet(publication, external_physics);
    PlayerInputState player{};
    player.manager_1856_counter_320 = 2;
    player.manager_1852_flag_256 = 9;
    player.grounded_frames_1300 = 6;
    player.update_count_1316 = 42;
    player.frames_since_teleport_1328 = 2;
    player.ground_history_frames_1304 = 1;
    player.time_since_last_input_1348 = 1.0f;
    player.time_on_ground_1352 = 1.0f;
    player.previous_spin_input_1360 = 1.0f;
    player.previous_crouch_1364 = 1.0f;
    player.previous_ground_position_1200 = rs::to_bits(std::array<float, 4>{1.0f, 2.0f, 3.0f, 0.0f});
    player.damped_ground_delta_1248 = {0, 0, 0, 0};
    player.probe.byte_104 = 7;
    player.left_line_test_1536.surface = 123;
    player.left_line_test_1536.valid = 2;
    PhysicalPlayerInput physical{};
    physical.state.category_12 = 100;
    physical.state.state_16 = 104;
    physical.state.skitch_value_40 = 77;
    physical.state.signed_ground_step_84 = 1;
    physical.surface_default_mode = 1;
    physical.ground.scalar_276 = 0.0f;
    physical.ground.scalar_292 = 0.5f;
    physical.ground.scalar_296 = 0.75f;
    ProcessedPhysicsInput output{};
    Services services;
    services.actor_input_available = true;

    CHECK(process_input(player, physical, pkt, output, services).is_ok());

    const std::vector<std::string> expected{"manager",     "reset",     "actor56",         "actor44",
                                            "probe_reset", "teleport",  "actor_input",     "transition",
                                            "ground_position", "board_toolkit", "skeleton", "grind"};
    CHECK_EQ(services.calls, expected);
    CHECK_EQ(player.manager_1856_counter_320, 1u);
    CHECK_EQ(player.manager_1852_flag_256, 0);
    CHECK_EQ(output.actor_query_2948, 56u);
    CHECK_EQ(output.actor_query_2952, 44u);
    CHECK_EQ(output.probe_1792.byte_104, 7);
    CHECK_EQ(player.probe.byte_104, 0);
    CHECK_EQ(player.grounded_frames_1300, 7u);
    CHECK_EQ(player.update_count_1316, 42u);
    CHECK_EQ(output.update_count_2568, 42u);
    CHECK_EQ(output.time_on_ground_2752, 1.25f);
    CHECK_NE(output.flags_2468 & (1u << 13), 0u);
    CHECK_EQ(output.transition_2636, 1.0f);
    CHECK_EQ(player.skitch_timer_1376, 1.75f);
    CHECK_EQ(output.skitch_value_2592, 77u);
    CHECK_EQ(output.ground_timer_2848, 0.25f);
    CHECK_EQ(output.secondary_ground_timer_2852, 0.5f);
    CHECK_EQ(rs::from_bits(output.vectors_464_480_496_512_528[1]), (std::array<float, 4>{2.0f, 4.0f, 6.0f, 0.0f}));
    CHECK_EQ(rs::from_bits(output.vectors_464_480_496_512_528[2]), (std::array<float, 4>{2.0f, 4.0f, 6.0f, 0.0f}));
    const auto filtered = rs::from_bits(output.vectors_464_480_496_512_528[3]);
    const std::array<float, 4> expected_filtered{0.2f, 0.4f, 0.6f, 0.0f};
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(rs::abs(filtered[i] - expected_filtered[i]) < 1e-6f);
    }
    CHECK_EQ(rs::from_bits(player.ground_delta_1232), (std::array<float, 4>{1.0f, 2.0f, 3.0f, 0.0f}));
    CHECK_EQ(output.time_since_last_input_2748, 1.25f);
    CHECK_EQ(rs::from_bits(output.prepared_jump_704), (std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(output.spin_same_direction_frames_2580, 1u);
    CHECK_EQ(output.crouch_delta_2780, 2.0f);
    CHECK_EQ(output.left_surface_2596, 123u);
}

S3_TEST(manager_call_precedes_category_capture_but_state_number_is_already_captured) {
    const auto publication = animation_publication();
    const auto external_physics = external(0);
    const auto pkt = packet(publication, external_physics);
    PlayerInputState player{};
    PhysicalPlayerInput physical{};
    physical.state.state_16 = 11;
    physical.state.category_12 = 100;
    physical.surface_default_mode = 1;
    physical.skeleton.anim_to_world_11920[3] = {7, 8, 9, 10};
    ProcessedPhysicsInput output{};
    Services services;
    services.manager_state_change = std::make_pair(22u, 500u);

    CHECK(process_input(player, physical, pkt, output, services).is_ok());

    CHECK_EQ(output.state_2504, 11u);
    CHECK_EQ(output.state_2508, 11u);
    CHECK_EQ(output.category_2512, 500u);
    CHECK_EQ(output.category_2516, 500u);
    CHECK_EQ(player.current_ground_position_1216, (RawVector{7, 8, 9, 10}));
    CHECK(!contains(services.calls, "ground_position"));
    CHECK_EQ(rs::to_bits(player.time_off_board_1368), 0x3c888889u);
}

S3_TEST(physical_publication_uses_low_flag_bits_and_stance_wheel_permutation) {
    using publication::publish_physical_outputs;
    const auto pub = animation_publication();
    const auto external_physics = external(0);
    auto pkt = packet(pub, external_physics);
    pkt.flags_10932 = (1u << 31) | (1u << 29) | (1u << 22);
    pkt.flag_10369 = 2;
    PhysicalPlayerInput physical{};
    physical.ground.flag_317 = 3;
    physical.air.flag_446 = 2;
    physical.off_board.scalar_32 = rs::f32::NAN_;
    physical.collision.wheel_contact_3296_3299 = {1, 0, 0, 0};
    ProcessedPhysicsInput output{};
    output.flags_2468 &= ~(1u << 20);

    publish_physical_outputs(physical, pkt, output);
    CHECK_NE(output.flags_2476 & (1u << 6), 0u);
    CHECK_EQ(output.flags_2476 & (1u << 5), 0u);
    CHECK_NE(output.flags_2476 & (1u << 4), 0u);
    CHECK_NE(output.flags_2488 & (1u << 24), 0u);
    CHECK_EQ(output.flags_2480 & 1u, 0u);
    CHECK_NE(output.flags_2476 & (1u << 27), 0u);
    CHECK_EQ(output.flags_2476 & 1u, 0u);
    CHECK_EQ(output.flags_2476 & (1u << 17), 0u);
    CHECK_EQ(output.flags_2472 & (1u << 27), 0u);
    CHECK_NE(output.flags_2472 & (1u << 26), 0u);
    CHECK_NE(output.flags_2472 & (1u << 25), 0u);
    CHECK_EQ(output.flags_2472 & (1u << 24), 0u);

    output.flags_2468 |= 1u << 20;
    publish_physical_outputs(physical, pkt, output);
    CHECK_NE(output.flags_2472 & (1u << 27), 0u);
    CHECK_EQ(output.flags_2472 & (1u << 26), 0u);
    CHECK_EQ(output.flags_2472 & (1u << 25), 0u);
    CHECK_NE(output.flags_2472 & (1u << 24), 0u);
}

S3_TEST(external_physics_fallback_uses_cached_trajectory_and_preserves_packet_high_bits) {
    const auto pub = animation_publication();
    auto incoming = external((1u << 31) | (1u << 25));
    for (auto& v : incoming.vectors) v = {0x100, 0x100, 0x100, 0x100};
    auto pkt = packet(pub, incoming);
    pkt.use_external_physics_10688 = 1;
    pkt.external_physics_flag_10689 = 3;
    PlayerInputState player{};
    for (std::uint32_t index = 5; index < 10; ++index) {
        player.external_physics_cache_1008.vectors[index] = {index, index, index, index};
    }
    player.external_physics_cache_1008.flags = 1u << 27;
    ProcessedPhysicsInput output{};
    output.external_physics_1616.flags = 0x1234;

    publication::publish_external_physics(player, pkt, output);

    CHECK_NE(output.flags_2472 & (1u << 29), 0u);
    CHECK_NE(output.flags_2472 & (1u << 28), 0u);
    CHECK_EQ(output.external_physics_1616.vectors[4], (RawVector{0x100, 0x100, 0x100, 0x100}));
    CHECK_EQ(output.external_physics_1616.vectors[5], (RawVector{5, 5, 5, 5}));
    CHECK_EQ(output.external_physics_1616.vectors[9], (RawVector{9, 9, 9, 9}));
    CHECK_NE(output.external_physics_1616.flags & (1u << 31), 0u);
    CHECK_NE(output.external_physics_1616.flags & (1u << 27), 0u);
    CHECK_NE(output.external_physics_1616.flags & (1u << 26), 0u);
    CHECK_EQ(output.external_physics_1616.flags & 0x01ffffffu, 0x1234u);
}

S3_TEST(surface_selector_and_effective_transform_match_recovered_switches) {
    using publication::effective_animation_transform;
    using publication::select_surface;
    const auto pub = animation_publication();
    const auto external_physics = external(0);
    auto pkt = packet(pub, external_physics);
    PlayerInputState player{};
    player.state_variants_1408[2].surface_override_enabled_60 = 1;
    pkt.state_variant_10928 = 2;
    ProcessedPhysicsInput output{};

    CHECK(select_surface(player, pkt, 13, output).is_ok());
    CHECK_EQ(output.surface_mode_2540, 2u);
    CHECK_EQ(rs::unwrap(output.state_variant_ref_2548).offset, 0x5a0);
    CHECK_EQ(rs::unwrap(output.surface_primary_ref_2544).offset, 40);
    CHECK_EQ(rs::unwrap(output.surface_secondary_ref_2552).offset, 116);

    player.state_variants_1408[2].surface_override_enabled_60 = 0;
    CHECK(select_surface(player, pkt, 6, output).is_ok());
    CHECK_EQ(output.surface_mode_2540, 3u);
    pkt.state_variant_10928 = 5;
    const auto err = select_surface(player, pkt, 1, output);
    CHECK(err.is_err());
    CHECK_EQ(err.unwrap_err(), 5u);

    const RawMatrix matrix{{
        {rs::to_bits(1.0f), rs::to_bits(-2.0f), rs::to_bits(0.0f), rs::to_bits(4.0f)},
        {rs::to_bits(5.0f), rs::to_bits(5.0f), rs::to_bits(5.0f), rs::to_bits(5.0f)},
        {rs::to_bits(6.0f), rs::to_bits(7.0f), rs::to_bits(8.0f), rs::to_bits(9.0f)},
        {rs::to_bits(10.0f), rs::to_bits(10.0f), rs::to_bits(10.0f), rs::to_bits(10.0f)},
    }};
    const auto flipped = effective_animation_transform(matrix, 1u << 2);
    CHECK_EQ(flipped[0][0], rs::to_bits(-1.0f));
    CHECK_EQ(flipped[0][1], rs::to_bits(2.0f));
    CHECK_EQ(flipped[1], matrix[1]);
    CHECK_EQ(flipped[2][0], rs::to_bits(-6.0f));
    CHECK_EQ(flipped[3], matrix[3]);
}

S3_TEST(physical_vector_publication_uses_the_restored_native_offsets) {
    const auto pub = animation_publication();
    const auto ext = external(0);
    const auto pkt = packet(pub, ext);
    auto splat = [](std::uint32_t v) { return RawVector{v, v, v, v}; };
    PhysicalPlayerInput physical{};
    physical.board_reckoning_side_176 = splat(176);
    physical.reckoning.vector_16 = splat(16);
    physical.reckoning.vector_64 = splat(64);
    physical.reckoning.vector_96 = splat(96);
    physical.physics.vector_16 = splat(116);
    physical.off_board.vector_64 = splat(164);
    physical.air.vector_160 = splat(160);
    ProcessedPhysicsInput output{};
    publication::publish_physical_outputs(physical, pkt, output);
    CHECK_EQ(output.vectors_544_560_592_608, (std::array<RawVector, 4>{splat(96), splat(176), splat(64), splat(16)}));
    CHECK_EQ(output.vectors_720_784_800_816_832_864[3], splat(116));
    CHECK_EQ(output.vectors_880_896_912_928_944[2], splat(164));
    physical.air.use_air_reckoning_452 = 1;
    publication::publish_physical_outputs(physical, pkt, output);
    CHECK_EQ(output.vectors_544_560_592_608[3], splat(160));
}

// types.rs snapshot_tests
S3_TEST(processed_snapshot_is_immutable_and_tick_owned) {
    ProcessedPhysicsInput input{};
    input.flags_2468 = 0x10;
    const auto snapshot = ProcessedPhysicsSnapshot::new_(23, input);
    input.flags_2468 = 0x20;

    CHECK_EQ(snapshot.tick, 23u);
    CHECK_EQ(snapshot.input.flags_2468, 0x10u);
    CHECK_EQ(input.flags_2468, 0x20u);
}

// deck_angles_tests.rs
S3_TEST(deck_angles_preserve_native_quadrants_stance_and_pitch_projection) {
    SkeletonOutputFields output{};
    const float pi = rs::f32::consts::PI;
    const std::array<std::array<float, 3>, 6> cases{{
        {1.0f, 1.0f, pi / 4.0f},
        {-1.0f, 1.0f, -pi / 4.0f},
        {1.0f, -1.0f, 3.0f * pi / 4.0f},
        {-1.0f, -1.0f, -3.0f * pi / 4.0f},
        {0.0f, 0.0f, pi / 2.0f},
        {-0.0f, -0.0f, -pi / 2.0f},
    }};
    for (const auto& c : cases) {
        output.publish_deck_angles({c[0], 0.5f, c[1], 0.0f}, false);
        CHECK(rs::abs(output.deck_yaw_536 - c[2]) < 0.00001f);
        CHECK_EQ(output.deck_pitch_540, 0.5f);
    }
    output.publish_deck_angles({1.0f, 0.5f, 1.0f, 0.0f}, true);
    CHECK(rs::abs(output.deck_yaw_536 + 3.0f * pi / 4.0f) < 0.00001f);
    CHECK_EQ(output.deck_pitch_540, -0.5f);
}

}  // namespace
