// Port of skate-core/tests/animation_actor_composition.rs (crate integration test).
#include <string_view>
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/commands.hpp"
#include "s3/core/animation/output/actor_packet.hpp"
#include "s3/core/animation/output/setup_physics.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"

using namespace s3::animation::output;
using s3::animation::commands::batch::CompletedSetData;
using s3::animation::commands::batch::SetDataQueue;
using s3::animation::commands::buffers::BufferError;
using s3::animation::commands::buffers::PoseBuffers;
using actor_packet::ActorPacketFields;
using actor_packet::ActorPublicationState;
using actor_packet::ExternalPhysicsInput;
using actor_packet::ExternalReset;
using attributes::AnimationAttribute;
using attributes::AttributeName;
using attributes::MotionGraphAttribute;
using attributes::PacketAttributes;
using intents::Intent;
using intents::IntentCapacityExceeded;
using intents::IntentMap;
using intents::IntentName;
using motion_graph_packet::MotionGraphPacket;
using motion_graph_packet::TrickIdentifier;
using packet_reset::AdditionalResetFields;
using packet_reset::RESET_POSE;
using physics_packet::ActorPoseBuffers;
using physics_packet::PhysicsPosePacket;
using physics_packet::SkaterPublicationState;
using setup_physics::AnimationPublication;
using setup_physics::IntentPublication;
using setup_physics::MotionGraphPublication;
using setup_physics::PhysicsInputPacket;
using Error = std::string_view;
using PubError = setup_physics::PublicationError<Error>;

namespace {
Intent intent(std::uint32_t id, float value) { return Intent{IntentName{{id, 0, 0, 0, 0, 0}}, value}; }
std::vector<std::uint32_t> first_words(const IntentMap& map) {
    std::vector<std::uint32_t> out;
    for (const auto& e : map.entries()) out.push_back(e.name._0[0]);
    return out;
}
NativeMatrix filled(float v) { return NativeMatrix{{{v, v, v, v}, {v, v, v, v}, {v, v, v, v}, {v, v, v, v}}}; }
s3::animation::Slice<const std::uint8_t> bytes(std::string_view text) {
    return s3::animation::Slice<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

MotionGraphPacket graph() {
    MotionGraphPacket g;
    g.trick.identifiers = {TrickIdentifier{{7, 7, 7, 7, 7, 7}}, TrickIdentifier{{7, 7, 7, 7, 7, 7}}};
    g.trick.word48 = 8;
    g.trick.scalars52_56 = {9.0f, 10.0f};
    g.gesture.gesture = 11;
    g.gesture.flag_bytes = {1, 2, 3, 255};
    g.wipeout_gesture = {12.0f, 13.0f};
    return g;
}
PhysicsInputPacket packet() {
    PhysicsInputPacket p;
    p.pose.bone_count = 1;
    p.pose.hierarchy = {filled(77.0f)};
    p.pose.local = {filled(88.0f)};
    p.pose.timestep = 0.0f;
    p.pose.foot_surface_ids = {42, 42};
    p.pose.flags = UINT32_MAX;
    p.reset.compression = 99.0f;
    p.reset.foot_ik_influence = {99.0f, 99.0f};
    p.reset.next_step_position_valid = true;
    p.reset.truck_tightness = 99.0f;
    p.reset.wheel_hardness = 99.0f;
    for (auto& v : p.reset.auxiliary_vectors) v = {99.0f, 99.0f, 99.0f, 99.0f};
    p.reset.requested_physics_mode = 99;
    p.motion_graph = graph();
    p.actor.external_physics.flags = 0x01abcdefu;
    return p;
}
ActorPublicationState actor() {
    ActorPublicationState a;
    a.flags1904 = (1u << 23) | (1u << 25) | (1u << 27) | (1u << 28) | (1u << 29) | (1u << 30) | 1u;
    a.flags1908 = 4;
    a.external_controller_present = false;
    a.external_impulse = {0x7fc12345u, 0x80000000u, 1u, 0xffu};
    for (std::uint32_t i = 0; i < 10; ++i) a.external_physics.vectors[i] = {i, i, i, i};
    a.external_physics.flags = 0xaa765432u;
    for (auto& row : a.external_reset.transform) row = {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u};
    a.external_reset.byte64 = 254;
    a.player_index = 3;
    return a;
}

struct Settings : actor_packet::PlayerPhysicsSettingsSource<Error> {
    bool fixed = false;
    bool missing_mode = false;
    Settings(bool fixed_, bool missing_mode_) : fixed(fixed_), missing_mode(missing_mode_) {}
    s3::rs::Result<float, Error> truck_tightness(std::uint32_t index) override {
        S3_ASSERT(!fixed);
        S3_ASSERT(index == 3);
        return 0.2f;
    }
    s3::rs::Result<float, Error> wheel_hardness(std::uint32_t index) override {
        S3_ASSERT(!fixed);
        S3_ASSERT(index == 3);
        return 0.8f;
    }
    s3::rs::Result<std::uint32_t, Error> requested_physics_mode(std::uint32_t index) override {
        S3_ASSERT(index == 3);
        if (missing_mode) return s3::rs::make_err(Error("mode unavailable"));
        return 4u;
    }
};

struct Environment : actor_packet::ActorPublicationEnvironment<Error, Settings> {
    std::vector<std::string_view> calls;
    std::optional<std::int32_t> scene = 19;
    std::uint8_t selector = 0;
    bool missing_scene = false;
    bool missing_mode = false;

    s3::rs::Result<std::optional<std::int32_t>, Error> current_scene_mode() override {
        calls.push_back("scene");
        if (missing_scene) return s3::rs::make_err(Error("scene service unavailable"));
        return scene;
    }
    s3::rs::Result<std::uint8_t, Error> ignore_respawn_reset_button() override {
        calls.push_back("global-byte");
        return std::uint8_t(0xfe);
    }
    s3::rs::Result<std::uint8_t, Error> actor_subobject56_slot16(ActorPublicationState& a) override {
        calls.push_back("actor56");
        S3_ASSERT((a.flags1904 & ((1u << 25) | (1u << 28))) == 0);
        return selector;
    }
    s3::rs::Result<Settings, Error> capture_physics_settings() override {
        calls.push_back("settings-base");
        return Settings(selector != 0, missing_mode);
    }
};
Environment environment() { return Environment{}; }
const std::vector<std::string_view> FULL_CALLS = {"scene", "global-byte", "settings-base", "actor56"};

struct Animation {
    CompletedSetData completed;
    ActorPoseBuffers poses;
    SkaterPublicationState state;
};
Animation animation(bool initialized) {
    PoseBuffers buffers{};
    const auto hierarchy = initialized ? buffers.matrices.initialized({filled(1.0f)}) : buffers.matrices.allocate(1);
    const auto local = buffers.matrices.initialized({filled(2.0f)});
    auto r = SetDataQueue::new_(std::move(buffers), 1).submit().complete();
    if (r.is_err()) S3_PANIC("set data failed");
    SkaterPublicationState s;
    s.orientation_bit31 = false;
    s.mirrored = false;
    s.riding_fakie = true;
    s.weight_on_nose = true;
    s.relative_stance = 1;
    s.natural_stance = 0;
    s.request_bit16 = true;
    s.request_bit15 = true;
    s.air_dismount_revert_requested = true;
    s.air_dismount_revert_frames = 12;
    s.signal = std::nullopt;
    return Animation{std::move(r.unwrap()), ActorPoseBuffers{hierarchy.at(0), local.at(0)}, s};
}
}  // namespace

S3_TEST(intent_assignment_reverses_collisions_and_retains_first_duplicate_value) {
    IntentMap source{};
    for (const Intent& entry : {intent(1, 1.0f), intent(62, 2.0f), intent(123, 3.0f), intent(2, 4.0f)}) {
        const auto r = source.insert(entry);
        CHECK(r.is_ok() && r.unwrap());
    }
    {
        const auto r = source.insert(intent(62, 99.0f));
        CHECK(r.is_ok() && !r.unwrap());
    }
    CHECK((first_words(source) == std::vector<std::uint32_t>{123, 62, 1, 2}));
    IntentMap destination{};
    destination.insert(intent(99, 99.0f)).unwrap();
    destination.replace_from(source);
    std::vector<std::pair<std::uint32_t, float>> pairs;
    for (const auto& e : destination.entries()) pairs.emplace_back(e.name._0[0], e.value);
    CHECK((pairs == std::vector<std::pair<std::uint32_t, float>>{{1, 1.0f}, {62, 2.0f}, {123, 3.0f}, {2, 4.0f}}));
    CHECK((first_words(source) == std::vector<std::uint32_t>{123, 62, 1, 2}));
}

S3_TEST(intent_hash_wraps_all_six_words_and_capacity_allows_existing_keys) {
    IntentMap map{};
    map.insert(intent(1, 7.0f)).unwrap();
    const Intent wrapped{IntentName{{UINT32_MAX, 0, 0, 0, 0, 2}}, -0.0f};
    map.insert(wrapped).unwrap();
    CHECK(map.entries().front().name == wrapped.name);
    CHECK_EQ(s3::rs::to_bits(map.entries().front().value), 0x80000000u);
    for (std::uint32_t id = 2; id < 64; ++id) map.insert(intent(id, static_cast<float>(id))).unwrap();
    CHECK_EQ(map.len(), std::size_t(64));
    {
        const auto r = map.insert(intent(1, 8.0f));
        CHECK(r.is_ok() && !r.unwrap());
    }
    {
        const auto r = map.insert(intent(1000, 8.0f));
        CHECK(r.is_err() && r.unwrap_err() == IntentCapacityExceeded{});
    }
    CHECK_EQ(map.len(), std::size_t(64));
    map.clear();
    CHECK(map.is_empty());
    {
        const auto r = map.insert(wrapped);
        CHECK(r.is_ok() && r.unwrap());
    }
}

S3_TEST(external_input_copy_transfers_each_high_flag_and_preserves_every_low_flag) {
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        ExternalPhysicsInput destination;
        for (auto& v : destination.vectors) v = {7, 7, 7, 7};
        destination.flags = 1u << bit;
        ExternalPhysicsInput source;
        for (auto& v : source.vectors) v = {0x7fc12345u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u};
        source.flags = 0;
        destination.copy_from(source);
        CHECK_EQ(destination.flags, bit < 25 ? 1u << bit : 0u);
        CHECK(destination.vectors == source.vectors);
        source.flags = 1u << bit;
        destination.flags = 0;
        destination.copy_from(source);
        CHECK_EQ(destination.flags, bit >= 25 ? 1u << bit : 0u);
    }
}

S3_TEST(actor_publication_copies_masked_payload_consumes_only_two_latches_and_repeats) {
    PhysicsInputPacket p = packet();
    ActorPublicationState a = actor();
    const std::uint32_t initial = a.flags1904;
    Environment e = environment();
    actor_packet::publish(a, p.pose, p.reset, p.actor, e).unwrap();
    CHECK(e.calls == FULL_CALLS);
    CHECK_EQ(p.actor.external_physics.flags, 0xababcdefu);
    CHECK(p.actor.external_physics.vectors == a.external_physics.vectors);
    CHECK(p.actor.external_reset == a.external_reset);
    CHECK(p.actor.external_impulse == a.external_impulse);
    CHECK(p.reset.actor_flag_1904_bit23 && p.reset.actor_flag_1908_bit2);
    CHECK(p.reset.external_impulse_active && p.reset.external_physics_input_active);
    CHECK(p.actor.actor_flag_1904_bit29 && p.reset.prevent_manual_respawn);
    CHECK(!p.reset.externally_controlled && !p.reset.force_braking);
    CHECK_EQ(p.pose.flags, UINT32_MAX);
    CHECK_EQ(a.flags1904, initial & ~((1u << 25) | (1u << 28)));
    CHECK_EQ(p.reset.ignore_respawn_reset_button, std::uint8_t(254));
    CHECK_EQ(p.reset.truck_tightness, 0.2f);
    CHECK_EQ(p.reset.wheel_hardness, 0.8f);
    CHECK_EQ(p.reset.requested_physics_mode, 4u);
    actor_packet::publish(a, p.pose, p.reset, p.actor, e).unwrap();
    CHECK_EQ(p.pose.flags & (1u << 24), 0u);
    CHECK(!p.reset.prevent_manual_respawn);
    CHECK(p.reset.external_impulse_active && p.actor.actor_flag_1904_bit29);
}

S3_TEST(scene_query_short_circuits_and_only_modes19_20_enable_flag) {
    const std::optional<std::int32_t> modes[] = {std::nullopt, 18, 19, 20, 21};
    for (bool controller : {false, true}) {
        for (bool braking : {false, true}) {
            for (const auto& mode : modes) {
                PhysicsInputPacket p = packet();
                ActorPublicationState a = actor();
                Environment e = environment();
                a.external_controller_present = controller;
                if (braking) a.flags1904 |= 1u << 26;
                e.scene = mode;
                e.selector = 128;
                actor_packet::publish(a, p.pose, p.reset, p.actor, e).unwrap();
                bool called = false;
                for (auto c : e.calls) called = called || c == "scene";
                CHECK_EQ(called, !controller && !braking);
                CHECK_EQ((p.pose.flags & (1u << 22)) != 0,
                         !controller && !braking && mode && (*mode == 19 || *mode == 20));
                CHECK_EQ(p.reset.externally_controlled, controller);
                CHECK_EQ(p.reset.force_braking, braking);
                CHECK_EQ(s3::rs::to_bits(p.reset.truck_tightness), 0x3f333333u);
                CHECK_EQ(s3::rs::to_bits(p.reset.wheel_hardness), 0x3f333333u);
                CHECK_EQ(p.reset.requested_physics_mode, 4u);
            }
        }
    }
}

S3_TEST(missing_mode_retains_both_branch_scalars_without_reading_unused_settings) {
    for (bool fixed : {false, true}) {
        PhysicsInputPacket p = packet();
        ActorPublicationState a = actor();
        Environment e = environment();
        e.selector = static_cast<std::uint8_t>(fixed);
        e.missing_mode = true;
        const auto r = actor_packet::publish(a, p.pose, p.reset, p.actor, e);
        CHECK(r.is_err() && r.unwrap_err() == "mode unavailable");
        CHECK_EQ(p.reset.truck_tightness, fixed ? 0.7f : 0.2f);
        CHECK_EQ(p.reset.wheel_hardness, fixed ? 0.7f : 0.8f);
        CHECK_EQ(p.reset.requested_physics_mode, 99u);
        CHECK(e.calls == FULL_CALLS);
    }
}

S3_TEST(composition_overwrites_reset_poses_then_merges_graph_lists_and_binds_owned_packet) {
    Animation anim = animation(true);
    PhysicsInputPacket p = packet();
    ActorPublicationState a = actor();
    Environment e = environment();
    MotionGraphPacket mg = graph();
    mg.gesture.gesture = 987;
    IntentMap intents{};
    intents.insert(intent(1, 3.0f)).unwrap();
    const std::array<MotionGraphAttribute, 1> attrs = {MotionGraphAttribute{AttributeName{{1, 1, 1, 1, 1}}, 4.0f}};
    const std::array<AnimationAttribute, 1> tree = {
        MotionGraphAttribute{AttributeName{{1, 1, 1, 1, 1}}, 5.0f}.to_animation()};
    const auto result = setup_physics::compose(
        p, AnimationPublication{anim.state, anim.completed, anim.poses, bytes("signup"), tree},
        MotionGraphPublication{mg, IntentPublication::MotionGraph{&intents}, attrs}, a, e);
    const auto& binding = result.unwrap();
    const PhysicsInputPacket& published = binding.packet();
    CHECK(published.pose.hierarchy == std::vector<NativeMatrix>{filled(1.0f)});
    CHECK(published.pose.local == std::vector<NativeMatrix>{filled(2.0f)});
    CHECK_EQ(published.pose.flags, 0x19ffffffu);
    CHECK_EQ(published.reset.compression, 0.5f);
    CHECK_EQ(s3::rs::to_bits(published.pose.timestep), 0x3c888889u);
    CHECK_EQ(published.motion_graph.gesture.gesture, 987u);
    const auto entries = published.attributes.entries();
    CHECK_EQ(entries.size(), std::size_t(2));
    CHECK(entries[0].payload._0[0] == std::optional<std::uint32_t>(s3::rs::to_bits(4.0f)));
    CHECK(entries[1].payload._0[0] == std::optional<std::uint32_t>(s3::rs::to_bits(5.0f)));
    CHECK_EQ(published.intents.entries().front().value, 3.0f);
    CHECK(!anim.state.request_bit16 && !anim.state.request_bit15 && !anim.state.air_dismount_revert_requested);
    CHECK_EQ(mg.gesture.gesture, 987u);
}

S3_TEST(composition_runtime_failure_retains_prior_phases_but_does_not_bind) {
    Animation anim = animation(true);
    PhysicsInputPacket p = packet();
    ActorPublicationState a = actor();
    Environment e = environment();
    const MotionGraphPacket mg = graph();
    p.intents.insert(intent(1, 3.0f)).unwrap();
    p.intents.insert(intent(62, 4.0f)).unwrap();
    e.missing_scene = true;
    const auto result =
        setup_physics::compose(p, AnimationPublication{anim.state, anim.completed, anim.poses, bytes(""), {}},
                               MotionGraphPublication{mg, IntentPublication::RetainPacket{}, {}}, a, e);
    CHECK(result.is_err());
    CHECK(result.unwrap_err() == PubError(PubError::RuntimeService{"scene service unavailable"}));
    CHECK(e.calls == std::vector<std::string_view>{"scene"});
    CHECK(p.pose.local == std::vector<NativeMatrix>{filled(2.0f)});
    CHECK_EQ(p.actor.external_physics.flags, 0xababcdefu);
    CHECK((first_words(p.intents) == std::vector<std::uint32_t>{62, 1}));
    CHECK_NE(a.flags1904 & ((1u << 25) | (1u << 28)), 0u);
    CHECK(!anim.state.request_bit16);
}

S3_TEST(missing_pose_stops_after_reset_before_graph_or_runtime_phases) {
    Animation anim = animation(false);
    PhysicsInputPacket p = packet();
    ActorPublicationState a = actor();
    Environment e = environment();
    MotionGraphPacket mg = graph();
    mg.gesture.gesture = 999;
    const auto result =
        setup_physics::compose(p, AnimationPublication{anim.state, anim.completed, anim.poses, bytes(""), {}},
                               MotionGraphPublication{mg, IntentPublication::RetainPacket{}, {}}, a, e);
    CHECK(result.is_err());
    const auto* pose_error = std::get_if<PubError::Pose>(&result.unwrap_err().value);
    CHECK(pose_error && std::get_if<BufferError::UninitializedBone>(&pose_error->_0.value));
    CHECK(p.pose.local == std::vector<NativeMatrix>{RESET_POSE});
    CHECK_EQ(p.motion_graph.gesture.gesture, 11u);
    CHECK(e.calls.empty());
    CHECK(anim.state.request_bit16);
}

S3_TEST(composed_mg_and_tree_attributes_reach_concrete_skeleton_steering_consumers) {
    using s3::animation::skeleton_input::name::encode;
    namespace scalar_attributes = s3::animation::skeleton_input::scalar_attributes;
    Animation anim = animation(true);
    PhysicsInputPacket p = packet();
    ActorPublicationState a = actor();
    Environment e = environment();
    const MotionGraphPacket mg = graph();
    const std::array<MotionGraphAttribute, 2> graph_attributes = {
        MotionGraphAttribute{encode("Turn"), 0.25f},
        MotionGraphAttribute{encode("PushContact"), 0.0f},
    };
    const std::array<AnimationAttribute, 2> tree_attributes = {
        MotionGraphAttribute{encode("Turn"), -0.75f}.to_animation(),
        MotionGraphAttribute{encode("Spin"), 0.5f}.to_animation(),
    };
    const auto result = setup_physics::compose(
        p, AnimationPublication{anim.state, anim.completed, anim.poses, bytes("signup"), tree_attributes},
        MotionGraphPublication{mg, IntentPublication::RetainPacket{}, graph_attributes}, a, e);
    const auto& binding = result.unwrap();
    scalar_attributes::ScalarAttributeInputs processed{};
    processed.turn_scale = 1.0f;
    processed.magnitude_scale = 1.0f;
    scalar_attributes::AnimationControlOutput animation_output{AttributeName{{0, 0, 0, 0, 0}}, 0};
    scalar_attributes::dispatch_supported_packet(binding, processed, animation_output).unwrap();
    CHECK_EQ(processed.turn, 0.75f);  // Later tree value wins, then native negation.
    CHECK_EQ(processed.spin, -0.5f);
    CHECK_EQ(processed.flags2488, 1u << 29);  // Scalar marker, no strength calculation.
    CHECK_EQ(binding.packet().attributes.entries().len(), std::size_t(4));
    CHECK(binding.packet().attributes.entries()[0].payload._0[0] ==
          std::optional<std::uint32_t>(s3::rs::to_bits(0.25f)));
}
