// Port of skate-game/src/physics/offboard/board_manager/tests.rs
//! Adapter tests use synthetic physical parameters, not stock parity fixtures.
//! They check writes to actual BoardRuntime bodies/drives rather than a call log.
#include "harness.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/game/physics/offboard/board_manager.hpp"

using namespace s3::game::physics::offboard::board_manager;
using s3::game::physics::offboard::board_possession::Materials;
using s3::game::physics::offboard::board_possession::VolumeFlags;
using s3::math::Vector3;
using s3::physics::board_runtime::BoardMotion;
using s3::physics::board_runtime::BoardRuntime;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::physics::rigid_body::RetailBodyMassProperties;
using s3::physics::rigid_body::RetailInertiaDynamics;
using s3::physics::rigid_body::RetailLocalMassFrame;
using s3::physics::rigid_body::RetailReactionCorrections;
using s3::physics::rigid_body::RetailSimulationStep;
using native::IDENTITY;
using native::lifecycle::Alignment;

namespace {
Owner owner() {
    auto graph = [](float value) {
        s3::point_graph::PointGraph<8> g{};
        g.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
        g.y.fill(value);
        return g;
    };
    Owner o{};
    o.state = native::State{};
    o.settings.hide_distance = 30.0f;
    o.settings.hide_offset = 1000.0f;
    o.settings.return_distance = 30.0f;
    o.settings.mounted_return_distance = 5.0f;
    o.settings.mounting_time = 0.1f;
    o.settings.retrieval_time = graph(1.0f);
    o.settings.retrieval_weight = graph(0.5f);
    o.settings.throw_pitch = 18.0f;
    o.settings.throw_velocity = graph(2.0f);
    o.settings.throw_target_pitch = 20.0f;
    o.settings.throw_pitch_scalar = 0.1f;
    o.settings.throw_roll_scalar = 0.1f;
    o.settings.throw_yaw_scalar = 0.2f;
    return o;
}

Observation observation() {
    Observation o{};
    o.processed = native::Processed{IDENTITY,
                                    IDENTITY,
                                    {0.0f, 0.0f, 0.0f, 0.0f},
                                    {0.0f, 0.0f, 0.0f, 0.0f},
                                    {0.0f, 0.0f, 1.0f, 0.0f},
                                    {0.0f, 0.0f, 1.0f, 0.0f},
                                    0,
                                    0,
                                    0};
    o.board_collision_flags_872 = 0;
    o.board_state_840 = 0;
    o.hand_contacts = {false, false};
    o.physical_hand_positions = {};
    o.animation_board_frame_12624 = IDENTITY;
    o.animation_hand_frames = {IDENTITY, IDENTITY};
    o.attachment_frame_0 = IDENTITY;
    return o;
}

BoardRuntime board() {
    RetailBodyMassProperties mass{};
    mass.local_mass_frame = RetailLocalMassFrame::IDENTITY;
    mass.dynamics.inverse_tensor = Vector3::new_(1.0f, 1.0f, 1.0f);
    mass.dynamics.inverse_mass = 1.0f;
    mass.dynamics.spherical = 0.0f;
    mass.dynamics.maximum_linear_velocity = 100.0f;
    mass.dynamics.maximum_angular_velocity = 100.0f;
    mass.dynamics.linear_drag = 0.0f;
    mass.dynamics.angular_drag = 0.0f;
    std::array<RetailBodyMassProperties, 7> masses;
    masses.fill(mass);
    std::array<RetailAffineTransform, 7> authored;
    authored.fill(RetailAffineTransform::IDENTITY);
    return BoardRuntime::new_(masses, authored, RetailAffineTransform::IDENTITY,
                              RetailSimulationStep::fixed_60_hz(0, 0.0f, Vector3::ZERO), BoardMotion::Active);
}

bool dynamics_disabled(const native::HandDrive& h) {
    return h.dynamics == std::array<std::array<std::uint32_t, 4>, 2>{{{0, 0, 0, 2}, {0, 0, 0, 2}}};
}
}  // namespace

S3_TEST(hold_and_release_change_real_drives_without_repositioning_board) {
    Owner o = owner();
    BoardRuntime b = board();
    const auto original = b.part_transforms();
    for (auto& body : b.bodies_mut()) body.rates.linear_velocity = Vector3::new_(1.0f, 2.0f, 3.0f);
    SkateboardControllerFields fields{};
    fields.state_448 = 0;
    fields.word_444 = 0;
    fields.system_on_452 = true;
    const Observation obs = observation();
    const RetailContactMaterial standard{0.5f, 0.4f, 0.1f};
    const RetailContactMaterial released{0.2f, 0.1f, 0.3f};
    std::uint8_t animated = 0;
    bool wiping_out = false;
    bool aligned = false;
    Alignment alignment{{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, false};
    VolumeFlags volumes{true, true, true, {true, true}};
    RetailContactMaterial wheels = standard, trucks = standard, deck = standard;
    Effects effects;
    effects.board = &b;
    effects.animated_290 = &animated;
    effects.wiping_out = &wiping_out;
    effects.alignment_active = &aligned;
    effects.alignment_ = &alignment;
    effects.volumes = &volumes;
    effects.materials = Materials{&wheels, &trucks, &deck};
    effects.standard_materials = {standard, standard, standard};
    effects.released_material = released;
    effects.standard_angular_drag = 3.0f;
    effects.processed_dt_2604 = 1.0f / 60.0f;

    o.hold(fields, obs, effects);
    CHECK_EQ(o.state.selected_hand_424, 1u);
    CHECK(dynamics_disabled(o.state.hands[0]));
    CHECK((o.state.hands[1].dynamics ==
           std::array<std::array<std::uint32_t, 4>, 2>{{{0x4395ffff, 0, 0x468c9fff, 2}, {0x4395ffff, 0, 0x468c9fff, 2}}}));
    CHECK_EQ(*effects.animated_290, std::uint8_t(1));
    CHECK(effects.board->part_transforms() == original);

    // Real hand and deck body IDs must be preserved in the shared row builder.
    {
        std::vector<RetailDriveRows> rows;
        const auto bodies = effects.board->bodies();
        o.append_drives(bodies[6], {bodies[0], bodies[1]}, 6, {11, 15}, 1.0f / 60.0f, rows);
        CHECK_EQ(rows.size(), std::size_t(2));
        CHECK_EQ(rows[0].frame_a_body.reaction_index, std::size_t(11));
        CHECK_EQ(rows[1].frame_a_body.reaction_index, std::size_t(15));
        for (const auto& row : rows) CHECK_EQ(row.frame_b_body.reaction_index, std::size_t(6));
    }

    o.let_go(fields, obs, effects);
    CHECK_MSG(fields.state_448 == 1, "LetGo leaves the caller's state assignment alone");
    CHECK_EQ(o.state.selected_hand_424, 2u);
    for (const auto& hand : o.state.hands) CHECK(dynamics_disabled(hand));
    CHECK((effects.board->hook_mut().drive.dynamics == std::array<std::uint32_t, 8>{0, 0, 0, 2, 0, 0, 0, 2}));
    CHECK_EQ(*effects.animated_290, std::uint8_t(0));
    CHECK(*effects.wiping_out);
    CHECK(*effects.materials.wheels == released);
    CHECK(*effects.materials.trucks == released);
    CHECK(*effects.materials.deck == released);
    CHECK(effects.board->part_transforms() == original);
    for (const auto& body : effects.board->bodies()) {
        CHECK(body.rates.linear_velocity == Vector3::new_(1.0f, 2.0f, 3.0f));
    }

    // Returning is the explicit exception: release clears retrieval velocity.
    fields.state_448 = 4;
    o.let_go(fields, obs, effects);
    for (const auto& body : effects.board->bodies()) CHECK(body.rates.linear_velocity == Vector3::ZERO);

    // SetPhysicsState changes state448 itself. The callback must preserve
    // LetGo's throw countdown without restoring the old returning state.
    Observation throw_observation = obs;
    throw_observation.processed.flags_2476 = 0x1000;
    {
        Transition transition = Transition::new_(o, throw_observation, effects, fields);
        transition.let_go_of_skateboard();
        fields.state_448 = 0;
        fields.system_on_452 = false;
        fields.word_444 = 0;
        std::move(transition).finish(fields);
    }
    CHECK_EQ(fields.state_448, 0u);
    CHECK(!fields.system_on_452);
    CHECK_EQ(fields.word_444, 8u);
    for (const auto& body : effects.board->bodies()) CHECK(body.rates.linear_velocity.z > 0.0f);

    //Teleport74D30 resets retrieval, not the persistent drive allocation.
    fields.system_on_452 = true;
    o.hold(fields, throw_observation, effects);
    o.state.retrieval.elapsed_192 = 4.0f;
    o.reset_for_teleport(fields, throw_observation, effects);
    CHECK_EQ(fields.state_448, 0u);
    CHECK(!fields.system_on_452);
    CHECK_MSG(fields.word_444 == 8, "Stop retains LetGo's throw write");
    CHECK_EQ(o.state.selected_hand_424, 2u);
    CHECK_EQ(o.state.retrieval.elapsed_192, 0.0f);
    CHECK(o.state.retrieval.initial_0 == IDENTITY);
    o.reset_for_teleport(fields, throw_observation, effects);
    CHECK_MSG(fields.word_444 == 0, "second teleport reset clears it while off");
    static_cast<native::lifecycle::Effects&>(effects).standard_board();
    CHECK(!*effects.wiping_out);
    CHECK(*effects.materials.deck == standard);
    CHECK(effects.volumes->deck && effects.volumes->trucks && effects.volumes->wheels);

    //75F00 ->75440 ->764F0: release must disable physical coupling BEFORE
    //the throw velocity and subsequent eight free-board torque updates.
    fields.system_on_452 = true;
    Observation throwing = throw_observation;
    throwing.processed.flags_2476 = 0x3000;
    o.hold(fields, throwing, effects);
    o.update(fields, throwing, effects);
    CHECK_EQ(fields.state_448, 2u);
    CHECK_EQ(fields.word_444, 7u);
    CHECK_EQ(o.state.selected_hand_424, 2u);
    {
        std::vector<RetailDriveRows> rows;
        const auto bodies = effects.board->bodies();
        o.append_drives(bodies[6], {bodies[0], bodies[1]}, 6, {11, 15}, 1.0f / 60.0f, rows);
        CHECK_MSG(rows.size() == 2, "native drives remain allocated when disabled");
        for (const auto& row : rows) {
            CHECK((row.linear_maximum_impulse == std::array<float, 3>{0.0f, 0.0f, 0.0f}));
            CHECK((row.angular_maximum_impulse == std::array<float, 3>{0.0f, 0.0f, 0.0f}));
        }
        std::array<RetailReactionCorrections, 16> reactions{};
        s3::physics::drive_solver::solve_drive_rows(rows, reactions, 8);
        std::array<RetailReactionCorrections, 16> zero{};
        CHECK_MSG(reactions == zero, "released hands cannot pull either assembly");
    }
    for (int i = 0; i < 7; ++i) o.update(fields, throwing, effects);
    CHECK_EQ(fields.word_444, 0u);
    std::array<Vector3, 7> velocities{};
    for (std::size_t i = 0; i < 7; ++i) velocities[i] = effects.board->bodies()[i].rates.linear_velocity;
    o.update(fields, throwing, effects);
    CHECK_EQ(fields.state_448, 2u);
    CHECK_MSG(fields.word_444 == 0, "throw input in FREE does not restart the throw");
    for (std::size_t i = 0; i < 7; ++i) CHECK(effects.board->bodies()[i].rates.linear_velocity == velocities[i]);
    for (const auto& h : o.state.hands) CHECK(dynamics_disabled(h));
}
