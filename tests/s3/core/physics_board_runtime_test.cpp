// Port of skate-core/src/physics/tests/board_runtime.rs.
#include "harness.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"

namespace {
using namespace s3::physics::board_runtime;
using s3::physics::board::index;
using s3::physics::drive_frames::authored_body_transforms;
using s3::physics::drive_frames::AuthoredTransformInputs;
using s3::physics::drive_frames::default_truck_transforms;
using s3::physics::drive_parameters::retail_truck_drive_dynamics;
using s3::physics::drive_parameters::RetailTruckDriveSettings;
using s3::physics::force_queue::QueuedPointForce;
namespace f32 = s3::rs::f32;

std::array<RetailBodyMassProperties, BODY_COUNT> masses() {
    std::array<RetailBodyMassProperties, BODY_COUNT> out{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const float fi = static_cast<float>(i);
        out[i] = RetailBodyMassProperties{
            RetailLocalMassFrame::IDENTITY,
            RetailInertiaDynamics{Vector3{2.0f + fi, 3.0f + fi, 4.0f + fi}, 0.5f, 0.0f, 1000.0f, 1000.0f, 0.0f,
                                  0.0f},
        };
    }
    return out;
}

RetailSimulationStep simulation(Vector3 gravity) { return RetailSimulationStep::fixed_60_hz(30, 0.001f, gravity); }

BoardRuntime runtime(Vector3 gravity, BoardMotion mode) {
    return BoardRuntime::new_(masses(), authored_body_transforms(AuthoredTransformInputs::STOCK),
                              RetailAffineTransform::IDENTITY, simulation(gravity), mode);
}

BoardStepSettings settings(Vector3 gravity, std::uint32_t iterations) {
    return BoardStepSettings{
        simulation(gravity),
        iterations,
        default_truck_transforms(),
        retail_truck_drive_dynamics(RetailTruckDriveSettings::STOCK),
        0.0f,
    };
}

void close(Vector3 actual, Vector3 expected) {
    CHECK_MSG(s3::rs::abs(actual.x - expected.x) < 2e-5f, actual << " != " << expected);
    CHECK_MSG(s3::rs::abs(actual.y - expected.y) < 2e-5f, actual << " != " << expected);
    CHECK_MSG(s3::rs::abs(actual.z - expected.z) < 2e-5f, actual << " != " << expected);
}

const std::vector<BoardCollision> NONE{};

S3_TEST(moving_board_transform_preserves_rates_and_pending_forces) {
    BoardRuntime board = runtime(Vector3{0.0f, -9.8f, 0.0f}, BoardMotion::Active);
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        auto& body = board.bodies_mut()[i];
        body.rates.linear_velocity = Vector3{static_cast<float>(i), 2.0f, 3.0f};
        body.rates.angular_velocity = Vector3{1.0f, static_cast<float>(i), 2.0f};
        body.rates.force_acceleration = Vector3{5.0f, 6.0f, 7.0f};
        body.rates.torque_acceleration = Vector3{8.0f, 9.0f, 10.0f};
    }
    const auto before = board.bodies();
    const auto parts = board.part_transforms();
    const Vector3 delta{4.0f, 5.0f, 6.0f};
    RetailAffineTransform target = parts[index(BodyId::Deck)];
    target.translation = delta;
    board.set_transform(target);
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const auto& body = board.bodies()[i];
        close(body.rates.linear_velocity, before[i].rates.linear_velocity);
        close(body.rates.angular_velocity, before[i].rates.angular_velocity);
        close(body.rates.force_acceleration, before[i].rates.force_acceleration);
        close(body.rates.torque_acceleration, before[i].rates.torque_acceleration);
        const Vector3 p = parts[i].translation;
        close(board.part_transforms()[i].translation, Vector3{p.x + delta.x, p.y + delta.y, p.z + delta.z});
    }
}

S3_TEST(spawn_preserves_each_mass_frame_and_keeps_the_hook_separate) {
    auto data = masses();
    data[index(BodyId::Deck)].local_mass_frame = RetailLocalMassFrame{
        Basis3{{{{0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}},
        Vector3{0.2f, -0.03f, 0.01f},
    };
    const RetailAffineTransform spawn{
        Basis3{{{{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}}},
        Vector3{3.0f, 4.0f, 5.0f},
    };
    const Vector3 gravity{0.0f, -9.81f, 0.0f};
    const BoardRuntime board = BoardRuntime::new_(data, authored_body_transforms(AuthoredTransformInputs::STOCK),
                                                  spawn, simulation(gravity), BoardMotion::Active);
    close(board.body_transform(BodyId::Deck).translation, Vector3{2.99f, 4.2f, 4.97f});
    const auto deck_part = board.part_transforms()[index(BodyId::Deck)];
    close(deck_part.translation, spawn.translation);
    for (std::size_t i = 0; i < 3; ++i) {
        close(Vector3{deck_part.basis.columns[i][0], deck_part.basis.columns[i][1], deck_part.basis.columns[i][2]},
              Vector3{spawn.basis.columns[i][0], spawn.basis.columns[i][1], spawn.basis.columns[i][2]});
    }
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const auto& body = board.bodies()[i];
        CHECK_EQ(body.inertia, data[i].dynamics);
        CHECK_EQ(body.state_flags, 4u);
        CHECK_EQ(body.rates.force_acceleration, gravity);
        CHECK_EQ(body.rates.kinetic_energy, f32::MAX);
        CHECK_EQ(body.rates.cool_down, 0u);
    }
    close(board.hook_transform().translation, spawn.translation);
    CHECK_NE(board.hook_transform().translation, board.body_transform(BodyId::Deck).translation);
    CHECK_EQ(board.hook().body.state_flags, 1u);
    CHECK_EQ(board.hook().body.inertia.inverse_mass, 0.0f);
    CHECK(board.hook().body.rates.world_inverse_inertia.columns == (Basis3{}.columns));
    CHECK_EQ(board.hook().body.rates.force_acceleration, Vector3::ZERO);
}

S3_TEST(integration_updates_the_same_state_read_by_presentation_and_force_producers) {
    BoardRuntime board = runtime(Vector3::ZERO, BoardMotion::Active);
    board.forces_mut().append(QueuedPointForce{7, Vector3{0.0f, 0.0f, 14.0f}, Vector3::ZERO});
    const Vector3 initial = board.body_transform(BodyId::Deck).translation;
    const BoardStepSettings config = settings(Vector3::ZERO, 0);
    const float dt = config.simulation.time_step;
    board.advance(NONE, {0.0f, 0.0f}, config);
    const BodySnapshot deck = board.bodies()[index(BodyId::Deck)];
    close(deck.rates.linear_velocity, Vector3{0.0f, 0.0f, 7.0f * dt});
    close(board.part_transforms()[index(BodyId::Deck)].translation,
          Vector3{initial.x, initial.y, initial.z + 7.0f * dt * dt});
    CHECK_EQ(board.forces().entries().size(), std::size_t(1));
    board.clear_forces();
    board.advance(NONE, {0.0f, 0.0f}, config);
    close(board.bodies()[index(BodyId::Deck)].rates.linear_velocity, deck.rates.linear_velocity);
    close(board.part_transforms()[index(BodyId::Deck)].translation,
          Vector3{initial.x, initial.y, initial.z + 14.0f * dt * dt});
}

S3_TEST(moving_the_static_hook_changes_constraint_response_without_moving_its_body) {
    BoardRuntime target = runtime(Vector3::ZERO, BoardMotion::Active);
    BoardRuntime baseline = runtime(Vector3::ZERO, BoardMotion::Active);
    std::uint8_t animated = 0;
    target.hook_mut().drive.enable_animation_soft(animated);
    baseline.hook_mut().drive.enable_animation_soft(animated);
    RetailAffineTransform requested = RetailAffineTransform::IDENTITY;
    requested.translation = Vector3{0.0f, 0.2f, 0.0f};
    target.set_hook_transform(requested);
    const BoardStepSettings config = settings(Vector3::ZERO, 25);
    target.advance(NONE, {0.0f, 0.0f}, config);
    baseline.advance(NONE, {0.0f, 0.0f}, config);
    CHECK(target.bodies()[index(BodyId::Deck)].rates.linear_velocity.y >
          baseline.bodies()[index(BodyId::Deck)].rates.linear_velocity.y);
    CHECK_EQ(target.hook_transform().translation, requested.translation);
    CHECK_EQ(target.hook().body.rates.linear_velocity, Vector3::ZERO);
    CHECK_EQ(target.hook().body.rates.angular_velocity, Vector3::ZERO);
    CHECK_EQ(target.hook().body.inertia, ZERO_INERTIA);
}

S3_TEST(explicit_inactive_modes_do_not_integrate_under_gravity) {
    const Vector3 gravity{0.0f, -9.81f, 0.0f};
    for (BoardMotion mode : {BoardMotion::Frozen, BoardMotion::Static}) {
        BoardRuntime board = runtime(gravity, mode);
        const RetailAffineTransform before = board.body_transform(BodyId::Deck);
        board.advance(NONE, {0.0f, 0.0f}, settings(gravity, 25));
        CHECK_EQ(board.body_transform(BodyId::Deck), before);
        CHECK_EQ(board.bodies()[6].state_flags, flags(mode));
        CHECK_EQ(board.bodies()[6].rates.cool_down, 30u);
        if (mode == BoardMotion::Static) {
            CHECK_EQ(board.bodies()[6].inertia.inverse_mass, 0.0f);
        } else {
            const auto data = masses();  // keep the temporary alive for CHECK_EQ
            CHECK_EQ(board.bodies()[6].inertia, data[6].dynamics);
        }
    }
}
}  // namespace
