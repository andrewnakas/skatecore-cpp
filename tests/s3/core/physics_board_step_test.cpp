// Port of skate-core/src/physics/tests/board_step.rs and its submodules
// tests/contact_reports.rs and tests/hook_step.rs (they share the helpers below).
#include <cmath>
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/hook_drive.hpp"
#include "s3/core/physics/mass.hpp"
#include "s3/core/riding/braking.hpp"
#include "s3/core/riding/push.hpp"

namespace {
using namespace s3::physics::board_step;
using s3::physics::assembly::BoardConstraints;
using s3::physics::assembly::prepare_drive_frames;
using s3::physics::board::BodyId_ORDER;
using s3::physics::board::index;
using s3::physics::drive_frames::default_live_body_orientations;
using s3::physics::drive_frames::default_live_body_transforms;
using s3::physics::drive_frames::default_truck_transforms;
using s3::physics::drive_parameters::retail_truck_drive_dynamics;
using s3::physics::drive_parameters::RetailTruckDriveSettings;
using s3::physics::force_queue::QueuedPointForce;
using s3::physics::mass::default_skateboard_mass_properties;
using s3::physics::rigid_body::basis_from_quaternion;
using s3::physics::rigid_body::RetailBodyRates;
using s3::physics::rigid_body::world_inverse_inertia;
using s3::riding::braking::BrakeInput;
using s3::riding::braking::BrakeSettings;
using s3::riding::braking::calculate_braking;
using s3::riding::push::enqueue_push;
using s3::riding::push::PushInput;
using s3::riding::push::PushLimits;
namespace f32 = s3::rs::f32;
using Bodies = std::array<BodySnapshot, BODY_COUNT>;

Bodies bodies() {
    const auto poses = default_live_body_transforms();
    const auto orientations = default_live_body_orientations();
    const auto masses = default_skateboard_mass_properties();
    Bodies out{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const auto basis = basis_from_quaternion(orientations[i]);
        out[i].state_flags = 4;
        out[i].inertia = masses[i].dynamics;
        out[i].rates = RetailBodyRates{
            orientations[i],
            basis,
            world_inverse_inertia(basis, masses[i].dynamics.inverse_tensor),
            poses[i].translation,
            Vector3::ZERO,
            Vector3::ZERO,
            Vector3::ZERO,
            Vector3::ZERO,
            0.0f,
            0,
        };
    }
    return out;
}

// Deliberately inactive synthetic hook isolates the existing board-only cases.
// Its body values are test inputs, not a gameplay initialization contract.
BoardHook inactive_hook() {
    BodySnapshot body = bodies()[6];
    body.state_flags = 0;
    return BoardHook{body, s3::physics::hook_drive::HookDriveState::initial()};
}

BoardStepSettings settings(std::uint32_t iterations) {
    return BoardStepSettings{
        RetailSimulationStep::fixed_60_hz(30, 0.001f, Vector3::ZERO),
        iterations,
        default_truck_transforms(),
        retail_truck_drive_dynamics(RetailTruckDriveSettings::STOCK),
        0.0f,
    };
}

float momentum_z(const Bodies& bodies) {
    float s = -0.0f;
    for (const auto& b : bodies) s += b.rates.linear_velocity.z / b.inertia.inverse_mass;
    return s;
}

std::vector<RetailReactionCorrections> zero_reactions() {
    return std::vector<RetailReactionCorrections>(REACTION_COUNT, RetailReactionCorrections{});
}

const std::vector<BoardCollision> NONE{};

S3_TEST(push_is_delivered_through_constraints_into_all_seven_integrated_bodies) {
    Bodies body_state = bodies();
    const Bodies before = body_state;
    Bodies unforced = before;
    BoardForceQueue queue{};
    const BoardStepSettings config = settings(25);
    std::array<float, BODY_COUNT> inverse_masses{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) inverse_masses[i] = body_state[i].inertia.inverse_mass;
    const auto push = enqueue_push(
        PushInput{0x02000000u, 0, 3.0f, 0.0f, 0.0f, f32::NAN_, config.simulation.time_step, Vector3{0.0f, 0.0f, 1.0f}},
        PushLimits{10.0f, 0.1f, 0.05f}, inverse_masses, queue);
    BoardStep tick{};
    // The preserved research explicitly characterizes noncoincident anchors
    // in the constructor pose (TRANSITION_PHYSICS_EVIDENCE.md). Compare the
    // push against that same unforced pose, not an invented settled board.
    {
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(unforced, hook, BoardForceQueue{}, NONE, {0.0f, 0.0f}, config);
    }
    {
        BoardHook hook = inactive_hook();
        tick.advance(body_state, hook, queue, NONE, {0.0f, 0.0f}, config);
    }
    for (BodyId id : BodyId_ORDER) {
        const auto after = body_state[index(id)].rates;
        CHECK_MSG(after.linear_velocity.z > unforced[index(id)].rates.linear_velocity.z, int(index(id)));
        CHECK_NE(after.position, before[index(id)].rates.position);
        CHECK_EQ(after.force_acceleration, Vector3::ZERO);
        CHECK_EQ(after.torque_acceleration, Vector3::ZERO);
    }
    const float expected_momentum = push.vector.z * config.simulation.time_step;
    CHECK(s3::rs::abs(momentum_z(body_state) - expected_momentum) < 1e-4f);
    CHECK_EQ(queue.entries().size(), std::size_t(1));
    CHECK(tick.reactions == zero_reactions());
    // Clearing is owned by GeneralUpdate. A subsequent coast tick must not
    // repeat either the push or last frame's accumulated constraint impulses.
    queue.clear();
    {
        BoardHook hook = inactive_hook();
        tick.advance(body_state, hook, queue, NONE, {0.0f, 0.0f}, config);
    }
    CHECK(s3::rs::abs(momentum_z(body_state) - expected_momentum) < 1e-4f);
}

S3_TEST(simulation_uses_the_supplied_iteration_count_and_one_timestep) {
    Bodies zero_pass = bodies();
    Bodies solved = zero_pass;
    BoardForceQueue queue{};
    queue.append(QueuedPointForce{3, Vector3{0.0f, 0.0f, 60.0f}, Vector3::ZERO});
    BoardStepSettings config = settings(0);
    config.simulation.time_step = 1.0f / 120.0f;
    config.simulation.frequency = 1.0f;  // StartFrame must replace a stale reciprocal.
    {
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(zero_pass, hook, queue, NONE, {0.0f, 0.0f}, config);
    }
    config.iterations = 25;
    {
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(solved, hook, queue, NONE, {0.0f, 0.0f}, config);
    }
    CHECK_EQ(zero_pass[0].rates.linear_velocity, Vector3::ZERO);
    CHECK(solved[0].rates.linear_velocity.z > 0.0f);
    CHECK(s3::rs::abs(momentum_z(solved) - 0.5f) < 1e-4f);
    CHECK(s3::rs::abs(momentum_z(zero_pass) - 0.5f) < 1e-4f);
}

S3_TEST(steering_changes_integrated_truck_wheel_and_deck_orientations) {
    Bodies straight = bodies();
    Bodies turning = straight;
    const BoardForceQueue queue{};
    {
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(straight, hook, queue, NONE, {0.0f, 0.0f}, settings(25));
    }
    {
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(turning, hook, queue, NONE, {0.3f, 0.3f}, settings(25));
    }
    for (BodyId id : BodyId_ORDER) {
        CHECK_NE(straight[index(id)].rates.orientation, turning[index(id)].rates.orientation);
    }
}

S3_TEST(native_braking_force_reaches_whole_assembly_momentum) {
    for (float speed : {-4.0f, 4.0f}) {
        Bodies body_state = bodies();
        for (auto& body : body_state) body.rates.linear_velocity.z = speed;
        const float before = momentum_z(body_state);
        const QueuedPointForce brake =
            calculate_braking(BrakeInput{0x40000000u, 0.5f, speed, s3::rs::abs(speed), 0.7f, Vector3{0.0f, 0.0f, 1.0f}},
                              BrakeSettings{200.0f, 100.0f, 0.8f});
        BoardForceQueue queue{};
        queue.append(brake);
        const BoardStepSettings config = settings(25);
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(body_state, hook, queue, NONE, {0.0f, 0.0f}, config);
        const float after = momentum_z(body_state);
        CHECK(s3::rs::abs(after) < s3::rs::abs(before));
        CHECK(s3::rs::abs(after - before - brake.force_world.z * config.simulation.time_step) < 1e-4f);
    }
}

S3_TEST(no_active_bodies_skips_compile_solve_and_integration) {
    Bodies body_state = bodies();
    for (auto& b : body_state) b.state_flags = 0;
    std::array<RetailBodyRates, BODY_COUNT> before{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) before[i] = body_state[i].rates;
    BoardHook hook = inactive_hook();
    BoardStep{}.advance(body_state, hook, BoardForceQueue{}, NONE, {0.3f, 0.3f}, settings(25));
    std::array<RetailBodyRates, BODY_COUNT> after{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) after[i] = body_state[i].rates;
    CHECK(before == after);
}

S3_TEST(contact_workspace_contains_this_ticks_queued_force) {
    Bodies body_state = bodies();
    const Vector3 point = body_state[6].rates.position;
    const BoardCollision collision{
        CollisionBody::StaticWorld(),
        CollisionBody::Board(BodyId::Deck),
        RetailContactInput{point, point, Vector3{0.0f, -1.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 0},
    };
    BoardForceQueue queue{};
    queue.append(QueuedPointForce{2, Vector3{0.0f, -60.0f, 0.0f}, Vector3::ZERO});
    BoardStep tick{};
    const BoardStepSettings config = settings(25);
    Bodies free = body_state;
    {
        BoardHook hook = inactive_hook();
        BoardStep{}.advance(free, hook, queue, NONE, {0.0f, 0.0f}, config);
    }
    {
        BoardHook hook = inactive_hook();
        const std::vector<BoardCollision> collisions{collision};
        tick.advance(body_state, hook, queue, collisions, {0.0f, 0.0f}, config);
    }
    const RetailContactJacobian row = tick.contacts[0];
    CHECK(row.accumulated_impulse()[0] > 0.0f);
    CHECK(s3::rs::abs(body_state[6].rates.linear_velocity.y) < s3::rs::abs(free[6].rates.linear_velocity.y));
    const float expected_target = s3::rs::powi(config.simulation.time_step, 2) * 60.0f;
    CHECK(s3::rs::abs(row.target_impulse()[0] - expected_target) < 1e-9f);
    CHECK_EQ(tick.reactions[WORLD_REACTION], RetailReactionCorrections{});
}

// ---- tests/contact_reports.rs ----

S3_TEST(reports_use_solved_forces_and_post_integration_pose_and_velocity) {
    for (bool is_a : {true, false}) {
        Bodies body_state = bodies();
        const Vector3 point = body_state[6].rates.position;
        const CollisionBody board = CollisionBody::Board(BodyId::Deck);
        const CollisionBody world = CollisionBody::StaticWorld();
        const BoardCollision collision{
            is_a ? board : world,
            is_a ? world : board,
            RetailContactInput{point, point, Vector3{0.0f, is_a ? 1.0f : -1.0f, 0.0f}, 0.0f, 0.0f, 0.0f,
                               is_a ? (5u << 16 | 7u) : (7u << 16 | 5u)},
        };
        BoardForceQueue queue{};
        queue.append(QueuedPointForce{2, Vector3{0.0f, -60.0f, 0.0f}, Vector3::ZERO});
        BoardStep tick{};
        const BoardStepSettings config = settings(25);
        {
            BoardHook hook = inactive_hook();
            const std::vector<BoardCollision> collisions{collision};
            tick.advance(body_state, hook, queue, collisions, {0.0f, 0.0f}, config);
        }
        const BoardContactReport report = tick.contact_reports()[0];
        CHECK_EQ(report.part, BodyId::Deck);
        CHECK_EQ(report.other_surface, std::uint16_t(7));
        CHECK_EQ(report.is_body_a, is_a);
        CHECK_EQ(report.normal.y, 1.0f);
        const Vector3 expected = body_state[6].rates.position;
        CHECK(s3::rs::abs(report.position.x - expected.x) < 1e-10f);
        CHECK(s3::rs::abs(report.position.y - expected.y) < 1e-10f);
        CHECK(s3::rs::abs(report.position.z - expected.z) < 1e-10f);
        CHECK_EQ(report.relative_linear_velocity, body_state[6].rates.linear_velocity);
        CHECK(report.normal_force_on_a.y * (is_a ? 1.0f : -1.0f) > 0.0f);

        // A later empty/inactive tick must not expose the previous contact.
        for (auto& body : body_state) body.state_flags = 0;
        {
            BoardHook hook = inactive_hook();
            tick.advance(body_state, hook, BoardForceQueue{}, NONE, {0.0f, 0.0f}, config);
        }
        CHECK(tick.contact_reports().empty());
    }
}

S3_TEST(report_budget_is_shared_by_parts_and_keeps_native_contact_order) {
    std::vector<RetailContactJacobian> rows;
    for (std::uint32_t i = 0; i < 20; ++i) {
        std::array<std::uint32_t, 64> words{};
        words[11] = 8;
        words[20] = s3::rs::to_bits(1.0f);
        words[29] = s3::rs::to_bits(1.0f);
        words[31] = i % 7;
        words[35] = s3::rs::to_bits(1.0f);
        words[43] = 0xFFFFFFFFu;
        words[55] = i + 1;
        rows.push_back(RetailContactJacobian{words, static_cast<std::size_t>(i % 7), WORLD_REACTION});
    }
    std::vector<BoardContactReport> output;
    auto surfaces = [&]() {
        std::vector<std::uint16_t> s;
        for (const auto& r : output) s.push_back(r.other_surface);
        return s;
    };
    auto range = [](std::uint16_t lo, std::uint16_t hi) {
        std::vector<std::uint16_t> s;
        for (std::uint16_t v = lo; v <= hi; ++v) s.push_back(v);
        return s;
    };
    s3::physics::contact_feedback::board_reports::collect(output, rows, bodies(), 60.0f);
    CHECK(surfaces() == range(1, 16));
    rows[0].words[20] = 0;
    rows[1].words[11] = 0;
    rows[2].words[43] = 6;  // Different part, same board: excluded from reports.
    s3::physics::contact_feedback::board_reports::collect(output, rows, bodies(), 60.0f);
    CHECK(surfaces() == range(4, 19));
}

// ---- tests/hook_step.rs ----

S3_TEST(an_attached_body_exchanges_reactions_with_the_board_before_both_integrate) {
    Bodies expected = bodies();
    Bodies actual = expected;
    BoardHook target = inactive_hook();
    target.body.state_flags = 4;
    target.body.rates.position.x += 0.2f;
    std::uint8_t scratch = 0;
    target.drive.enable_animation_soft(scratch);
    BodySnapshot attached_body = target.body;
    const BoardStepSettings config = settings(25);
    const auto frames = prepare_drive_frames(config.base_truck_transforms, {0.0f, 0.0f}, target);
    std::vector<RetailDriveRows> rows =
        BoardConstraints::build(actual, target, frames, config.truck_dynamics, config.simulation.time_step).drives;
    S3_ASSERT(!rows.empty());
    RetailDriveRows row = rows.back();
    rows.pop_back();
    row.frame_a_body.reaction_index = ATTACHED_REACTION_BASE;
    std::array<RetailDriveRows, 1> attached_drives{row};
    const BoardForceQueue queue{};
    BoardStep{}.advance(expected, target, queue, NONE, {0.0f, 0.0f}, config);
    {
        BoardHook hook = inactive_hook();
        AttachedStep attached;
        attached.bodies = {&attached_body};
        attached.drives = attached_drives;
        BoardStep{}.advance_attached(actual, hook, queue, NONE, {0.0f, 0.0f}, config, attached);
    }
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        CHECK_EQ(actual[i].rates.position, expected[i].rates.position);
        CHECK_EQ(actual[i].rates.linear_velocity, expected[i].rates.linear_velocity);
    }
    CHECK_EQ(attached_body.rates.position, target.body.rates.position);
    CHECK_EQ(attached_body.rates.linear_velocity, target.body.rates.linear_velocity);
    bool any = false;
    for (float v : attached_drives[0].accumulated_linear_impulse) any = any || v != 0.0f;
    CHECK(any);
}

S3_TEST(hook_drive_exchanges_momentum_and_integrates_without_teleporting_deck) {
    Bodies parts = bodies();
    BoardHook hook = inactive_hook();
    hook.body.state_flags = 4;
    hook.body.rates.position.x += 0.2f;
    std::uint8_t animated = 0;
    hook.drive.enable_animation_soft(animated);
    BoardHook disabled = hook;
    disabled.drive.disable_animation(animated);
    Bodies free = parts;
    const Vector3 original_deck = parts[6].rates.position;
    const BoardForceQueue queue{};
    BoardStep tick{};
    const BoardStepSettings config = settings(25);
    tick.advance(parts, hook, queue, NONE, {0.0f, 0.0f}, config);
    BoardStep{}.advance(free, disabled, queue, NONE, {0.0f, 0.0f}, config);
    CHECK_NE(parts[6].rates.position.x, free[6].rates.position.x);
    CHECK_NE(hook.body.rates.position.x, disabled.body.rates.position.x);
    CHECK_NE(parts[6].rates.position.x, hook.body.rates.position.x);
    CHECK_NE(parts[6].rates.position, original_deck);
    float momentum = -0.0f;
    for (const auto& b : parts) momentum += b.rates.linear_velocity.x / b.inertia.inverse_mass;
    momentum += hook.body.rates.linear_velocity.x / hook.body.inertia.inverse_mass;
    CHECK_MSG(s3::rs::abs(momentum) < 1e-4f, "internal hook drive changed momentum: " << momentum);
    CHECK(tick.reactions == zero_reactions());
}
}  // namespace
