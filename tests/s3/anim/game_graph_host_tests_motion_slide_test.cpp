// Port of skate-game/src/graph_host/tests/motion_slide.rs (the `slide_tests` module of motion.rs).
//! Stock bindings plus production Host lifecycle: a split latch breaks this test.
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

#include "assets.hpp"
#include "harness.hpp"
#include "motion_host_fixture.hpp"
#include "s3/game/graph_host/motion_conditions.hpp"
#include "s3/game/graph_host/motion_hooks.hpp"
#include "s3/game/graph_host/motion_reset.hpp"

using namespace s3::game::graph_host::motion;
namespace fixture = s3test::motion_fixture;
using s3::animation::skeleton_input::name::encode;
using Operation = s3::game::graph_host::motion_sliding::Operation;

namespace {

MotionHost host(const std::string& root) {
    const auto graph = fixture::stock_graph(root);
    return fixture::host(root, graph, fixture::context(false, false, false));
}

std::size_t behavior(const MotionHost& host, const std::function<bool(const MotionOperation&)>& predicate) {
    for (std::size_t i = 0; i < host.remap.behaviors.size(); ++i) {
        if (predicate(host.operations[host.remap.behaviors[i]])) return i;
    }
    S3_PANIC("Required authored stock behavior is missing");
}

std::size_t slide(const MotionHost& host, const Operation& operation) {
    return behavior(host, [&](const MotionOperation& v) {
        const auto* other = std::get_if<MotionOperation::Slide>(&v.value);
        return other != nullptr && other->_0 == operation;
    });
}

s3::graph::controller::Frame frame() { return fixture::frame(0.1f); }

void physical(MotionHost& host) {
    host.animation.skater_animation_flags = 0u;
    s3::graph::conditions::PhysicalStateInputs state{};
    state.category = 1;
    state.grinding = false;
    state.grind_name = std::string();
    host.condition_inputs.physical_state = state;
    MotionPhysical p{};
    p.turning = s3::input::set_turning::Physical{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f};
    p.stance = {false, false};
    p.forward_speed = 2.0f;
    p.time_since_teleport = 1.0f;
    p.is_switch = false;
    PushFootFrame foot{};
    foot.left_foot = {0.0f, 0.0f, 0.0f, 0.0f};
    foot.right_foot = {0.0f, 0.0f, 0.0f, 0.0f};
    foot.deck_position = {0.0f, 0.0f, 0.0f, 0.0f};
    foot.deck_y = {0.0f, 1.0f, 0.0f, 0.0f};
    foot.deck_z = {0.0f, 0.0f, 1.0f, 0.0f};
    foot.skateboard_flipped = false;
    p.foot_frame = foot;
    host.physical = p;
    s3::animation::riding_fakie::Physical fakie{};
    fakie.category = 1;
    fakie.grind_state = 0;
    fakie.doing_trick = false;
    fakie.board_axis = {0.0f, 0.0f, 1.0f, 0.0f};
    fakie.deck_velocity = {2.0f, 0.0f, 0.0f, 0.0f};
    fakie.external_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    fakie.ground_projected_speed = 2.0f;
    host.fakie_physical = fakie;
    s3::game::graph_host::motion_native::Physical native{};
    native.centre_of_mass_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    native.system_up = {0.0f, 1.0f, 0.0f, 0.0f};
    native.board_reckoning_z = {0.0f, 0.0f, 1.0f, 0.0f};
    native.board_reckoning = {{{1.0f, 0.0f, 0.0f, 0.0f},
                               {0.0f, 1.0f, 0.0f, 0.0f},
                               {0.0f, 0.0f, 1.0f, 0.0f},
                               {0.0f, 0.0f, 0.0f, 1.0f}}};
    host.native_physical = native;
}

float packet(const MotionHost& host, const char* name) {
    const auto encoded = encode(name);
    const auto& attributes = host.animation.motion_attributes;
    for (auto it = attributes.rbegin(); it != attributes.rend(); ++it) {
        if (it->name == encoded) return it->value;
    }
    S3_PANIC("Missing physical packet attribute");
}

std::string errors(const MotionHost& host) { return host.errors.join(", "); }

}  // namespace

// #[ignore = "requires the user's original stock graph, banks and collections"]
S3_TEST(stock_slide_lifecycle_shares_turning_latch_and_captured_stance) {
    S3_REQUIRE_ASSETS(root);
    MotionHost host = ::host(root);
    physical(host);
    const auto frame = ::frame();
    const std::size_t power = slide(host, Operation::Update{});
    const std::size_t create = slide(host, Operation::Create{true});
    const std::size_t candidate = slide(host, Operation::Candidate{});
    const std::size_t active = slide(host, Operation::IsPowerSliding{});
    const std::size_t manual = slide(host, Operation::ManualAttribute{});
    const std::size_t turning =
        behavior(host, [](const MotionOperation& v) { return v.is<MotionOperation::SetTurning>(); });
    std::size_t grab = host.remap.hooks.size();
    for (std::size_t i = 0; i < host.remap.hooks.size(); ++i) {
        const auto* hook = std::get_if<MotionOperation::Hook>(&host.operations[host.remap.hooks[i]].value);
        if (hook == nullptr) continue;
        using s3::game::graph_host::motion_hooks::MotionHook;
        const auto* slide_hook = std::get_if<MotionHook::GrabSlide>(&hook->_0.value);
        if (slide_hook != nullptr && slide_hook->right) {
            grab = i;
            break;
        }
    }
    CHECK_MSG(grab < host.remap.hooks.size(), "Authored GrabSlide hook missing");
    const std::array<std::uint32_t, 6> context{};
    for (const bool fakie : {false, true}) {
        host.slide_latch.reset();
        host.animation.motion_intents.clear();
        //A pre-graph snapshot must not override the current animation flag.
        host.physical->stance.first = !fakie;
        host.animation.skater_animation_flags = static_cast<std::uint32_t>(fakie) << 29;
        for (const std::size_t id : {power, candidate, active, create, turning, manual}) host.allocate(id, frame);
        host.begin(candidate, context, frame);
        host.begin(turning, context, frame);
        host.update(power, context, frame);  // native ground-entry history capture
        const char* intent = fakie ? "LeftSlide" : "RightSlide";
        const char* start = fakie ? "LeftSlideStart" : "RightSlideStart";
        const bool right = !fakie;
        const float value = fakie ? -0.7f : 0.7f;
        host.animation.motion_intents.insert(intent, value);
        host.animation.motion_intents.insert(start, 1.0f);
        host.update(power, context, frame);
        CHECK(host.slide_latch.start(right));
        host.update(turning, context, frame);
        {
            const auto* s = host.instances[turning].get<Instance::Turning>();
            CHECK(s != nullptr && s->_0.mode == static_cast<std::uint32_t>(right));
        }
        CHECK_EQ(host.slide_latch.captured_fakie(), fakie);
        host.hook(grab, frame);
        host.begin(create, context, frame);
        host.begin(active, context, frame);
        CHECK(host.is_power_sliding);
        host.update(create, context, frame);
        const float expected_slide = fakie ? 1.0f - 0.7f : 0.7f;
        CHECK(std::fabs(packet(host, "slide") - expected_slide) < 1.0e-6f);
        CHECK_EQ(packet(host, "turn"), host.turning.override_turn);
        // Current stance changes after entry; the captured side remains authoritative.
        host.animation.skater_animation_flags = static_cast<std::uint32_t>(!fakie) << 29;
        host.update(turning, context, frame);
        CHECK_EQ(host.slide_latch.captured_fakie(), fakie);
        host.animation.motion_intents.insert("Manual", -0.25f);
        host.update(manual, context, frame);
        CHECK_EQ(packet(host, "balance"), -0.25f);
        host.animation.motion_intents.remove(intent);
        host.animation.motion_intents.remove(start);
        for (int i = 0; i < 4; ++i) host.update(power, context, frame);
        using s3::game::graph_host::motion_conditions::MotionCondition;
        CHECK(MotionCondition(MotionCondition::ShouldLeaveSlide{true}).evaluate(host, frame).unwrap());
        host.update(turning, context, frame);
        {
            const auto* s = host.instances[turning].get<Instance::Turning>();
            CHECK(s != nullptr && s->_0.mode == 2u);
        }
        host.end(active, context, frame);
        host.end(candidate, context, frame);
        CHECK(!host.is_power_sliding && !host.slide_latch.candidate_enabled());
        CHECK_MSG(host.errors.is_empty(), errors(host));
    }
}

// #[ignore = "requires the user's original stock graph, banks and collections"]
S3_TEST(stock_slide_deceleration_uses_ground_frame_and_resets_on_entry) {
    S3_REQUIRE_ASSETS(root);
    MotionHost host = ::host(root);
    physical(host);
    const auto frame = ::frame();
    const std::array<std::uint32_t, 6> context{};
    const std::size_t decel = slide(host, Operation::Deceleration{});
    const std::size_t spin = slide(host, Operation::Spin{});
    host.allocate(decel, frame);
    host.begin(decel, context, frame);
    host.fakie_physical->deck_velocity = {0.0f, 2.0f, 0.0f, 0.0f};
    host.update(decel, context, frame);
    auto value = [&](const MotionHost& h) {
        const auto* s = h.instances[decel].get<Instance::SlideDeceleration>();
        if (s == nullptr) S3_PANIC("Wrong deceleration instance");
        return s->_0;
    };
    CHECK_EQ(value(host), 0.0f);  // motion parallel to ground up is discarded
    host.native_physical->board_reckoning = {{{0.0f, 1.0f, 0.0f, 0.0f},
                                              {-1.0f, 0.0f, 0.0f, 0.0f},
                                              {0.0f, 0.0f, 1.0f, 0.0f},
                                              {100.0f, 200.0f, 300.0f, 1.0f}}};
    host.update(decel, context, frame);
    const float first = value(host);
    CHECK(first > 0.0f && std::isfinite(first));  // same motion is now across the ground
    host.update(decel, context, frame);
    CHECK(value(host) > first);
    host.end(decel, context, frame);
    host.begin(decel, context, frame);
    CHECK_EQ(value(host), 0.0f);
    host.update(decel, context, frame);
    CHECK_EQ(value(host), first);
    host.begin(spin, context, frame);
    host.update(spin, context, frame);
    host.end(spin, context, frame);
    // Both are animation parameters, so neither creates a physical packet scalar.
    CHECK(host.animation.motion_attributes.empty());
    CHECK_MSG(host.errors.is_empty(), errors(host));
}

S3_TEST(slide_direction_preserves_native_unordered_threshold_branch) {
    CHECK_EQ(s3::game::graph_host::motion_sliding::direction({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f},
                                                             {0.0f, 0.0f, 1.0f, 0.0f}, false),
             1.0f);
}

// #[ignore = "requires installed stock graph assets; compile-only during bail work"]
S3_TEST(bail_reset_restores_saved_stance_in_the_real_graph_dispatch) {
    using Reset = s3::game::graph_host::motion_reset::Operation;
    S3_REQUIRE_ASSETS(root);
    MotionHost host = ::host(root);
    const std::array<std::uint32_t, 6> context{};
    const std::size_t reset = behavior(host, [](const MotionOperation& op) {
        const auto* r = std::get_if<MotionOperation::ResetAnimation>(&op.value);
        return r != nullptr && r->_0 == Reset::SkaterAnimation;
    });
    const std::size_t stance = behavior(host, [](const MotionOperation& op) {
        const auto* r = std::get_if<MotionOperation::ResetAnimation>(&op.value);
        return r != nullptr && r->_0 == Reset::GivenStance;
    });
    for (const std::uint32_t natural : {0u, 1u}) {
        for (const std::uint32_t requested : {0u, 1u}) {
            host.animation.skater_animation_flags = 0xf80a0000u;
            host.animation.natural_stance = natural;
            host.animation.relative_stance = 1;
            host.animation.requested_stance = requested;
            host.begin(reset, context, frame());
            CHECK_EQ(host.animation.requested_stance, requested);
            CHECK_EQ(host.animation.skater_animation_flags, std::optional<std::uint32_t>(0x08020000u));
            host.begin(stance, context, frame());
            CHECK_EQ(host.animation.requested_stance, 0u);
            const bool backwards = natural == requested;
            CHECK_EQ(host.playback_context.is_mirrored, std::optional<bool>(backwards));
            CHECK_EQ(host.animation.relative_stance, requested);
            CHECK_MSG(host.errors.is_empty(), errors(host));
        }
    }
}
