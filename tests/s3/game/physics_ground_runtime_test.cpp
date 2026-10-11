// Port of skate-game/src/physics/ground_runtime/{entry_restoration_tests.rs, launch.rs, surface.rs,
// settings.rs} `#[test]`s.
#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/physics/ground_runtime.hpp"

using namespace s3::game::physics::ground_runtime;
using s3::math::Vector3;
namespace rb = s3::physics::rigid_body;
namespace br = s3::physics::board_runtime;

// ---- entry_restoration_tests.rs ----
//! Independent field-ownership regression, not a stock gameplay/parity fixture.
S3_TEST(ground_reentry_restores_collision_group_without_reactivating_bodies) {
    rb::RetailBodyMassProperties mass{};
    mass.local_mass_frame = rb::RetailLocalMassFrame::IDENTITY;
    mass.dynamics.inverse_tensor = Vector3::new_(1.0f, 2.0f, 3.0f);
    mass.dynamics.inverse_mass = 1.0f;
    mass.dynamics.spherical = 0.0f;
    mass.dynamics.maximum_linear_velocity = 100.0f;
    mass.dynamics.maximum_angular_velocity = 100.0f;
    mass.dynamics.linear_drag = 0.25f;
    mass.dynamics.angular_drag = 0.5f;
    std::array<rb::RetailBodyMassProperties, 7> masses;
    masses.fill(mass);
    std::array<s3::physics::drive_frames::RetailAffineTransform, 7> authored;
    authored.fill(s3::physics::drive_frames::RetailAffineTransform::IDENTITY);
    auto board = br::BoardRuntime::new_(masses, authored, s3::physics::drive_frames::RetailAffineTransform::IDENTITY,
                                        rb::RetailSimulationStep::fixed_60_hz(30, 0.001f, Vector3::ZERO),
                                        br::BoardMotion::Active);
    board.set_collision_group(7);
    const std::uint32_t flags_table[7] = {4, 12, 2, 10, 1, 9, 4};
    {
        std::size_t i = 0;
        for (auto& body : board.bodies_mut()) {
            body.state_flags = flags_table[i];
            body.rates.cool_down = 10 + std::uint32_t(i);
            body.rates.linear_velocity = Vector3::new_(float(i), 3.0f, 2.0f);
            body.rates.angular_velocity = Vector3::new_(1.0f, float(i), 2.0f);
            ++i;
        }
    }
    const auto before = board.bodies();
    const auto target_before = board.hook().body;
    std::uint8_t flags = 0xd5;
    const float supplied_stock_drag = 2.75f;

    entry::restore_standard_board_fields(board, flags, supplied_stock_drag);

    CHECK_EQ(board.collision_group(), 4u);
    CHECK_EQ(flags, 0x55);
    for (std::size_t i = 0; i < 7; ++i) {
        const auto& body = board.bodies()[i];
        const auto& old = before[i];
        CHECK_EQ(body.state_flags, old.state_flags);
        CHECK(body.rates == old.rates);
        auto expected_inertia = old.inertia;
        if (i == 6) {
            expected_inertia.angular_drag = supplied_stock_drag;
        }
        CHECK(body.inertia == expected_inertia);
    }
    CHECK_EQ(board.hook().body.state_flags, target_before.state_flags);
    CHECK(board.hook().body.rates == target_before.rates);
    CHECK(board.hook().body.inertia == target_before.inertia);
}

// ---- launch.rs ----
S3_TEST(wall_jump_selector_receives_launch_velocity_and_physical_anchors) {
    GroundLaunchInfo info{};
    info.board_position = {1.0f, 2.0f, 3.0f, 0.0f};
    info.physical_center_of_mass = {1.2f, 3.0f, 3.1f, 0.0f};
    info.skeleton_vector_16208 = {0.2f, 0.8f, 0.1f, 0.0f};
    info.flags_270 = 7;
    info.time_step = 1.0f / 60.0f;
    const std::array<float, 4> velocity{2.0f, 4.0f, -3.0f, 0.0f};
    info.wall_jump(velocity);
    const auto launch = info.selector_launch();
    CHECK(launch.player_jumped);
    CHECK(!launch.use_position_override);
    CHECK(launch.start_velocity == velocity);
    CHECK(launch.com_velocity == velocity);
    CHECK(launch.board_position == info.board_position);
    CHECK(launch.animation_com_position == info.physical_center_of_mass);
    CHECK(launch.skeleton_vector_160 == info.skeleton_vector_16208);
    CHECK_EQ(launch.trajectory_count, 7);
}

// ---- surface.rs ----
S3_TEST(audio_vote_prefers_contact_weight_then_lower_id) {
    CHECK_EQ(surface::choose_audio_surface({7, 7, 3, 3}, {true, false, true, false}), 3u);
    CHECK_EQ(surface::choose_audio_surface({7, 7, 3, 3}, {true, true, true, false}), 7u);
    CHECK_EQ(surface::choose_audio_surface({0, 0, 0, 0}, {true, true, true, true}), 1u);
}

// ---- settings.rs (Rust `#[ignore]`: private stock collections; here pack-backed, skip without pack) ----
S3_TEST(stock_ground_settings_closure_loads_without_defaults) {
    const auto* vfs = s3test::game::assets();
    if (!vfs) return;
    auto loaded = s3::data::Collections::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK(loaded.ok);
    if (!loaded.ok) return;
    const auto& data = loaded.value;
    const auto settings = GroundSettings::load(data, "default", "default");
    CHECK_MSG(settings.is_ok(), (settings.is_err() ? settings.unwrap_err() : std::string()));
    const auto runtime = GroundRuntime::load(data);
    CHECK_MSG(runtime.is_ok(), (runtime.is_err() ? runtime.unwrap_err() : std::string()));
    const auto pumping = GroundPumping::load(data);
    CHECK_MSG(pumping.is_ok(), (pumping.is_err() ? pumping.unwrap_err() : std::string()));
    const auto state = GroundState::load(data, "default", true);
    CHECK_MSG(state.is_ok(), (state.is_err() ? state.unwrap_err() : std::string()));
}

S3_TEST(customiser_truck_tightness_changes_authored_steering) {
    namespace steering = s3::riding::steering;
    const auto* vfs = s3test::game::assets();
    if (!vfs) return;
    auto loaded = s3::data::Collections::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK(loaded.ok);
    if (!loaded.ok) return;
    const auto settings_r = GroundSettings::load(loaded.value, "default", "default");
    CHECK_MSG(settings_r.is_ok(), (settings_r.is_err() ? settings_r.unwrap_err() : std::string()));
    if (settings_r.is_err()) return;
    const auto& settings = settings_r.unwrap();
    std::vector<float> samples;
    for (float tightness : {0.0f, 0.7f, 1.0f}) {
        steering::SteeringInput input{};
        input.turn = 0.6f;
        input.absolute_body_speed = 5.0f;
        input.flipped_controls_scalar = 1.0f;
        input.truck_tightness = tightness;
        samples.push_back(steering::calculate_tilt(settings.steering, input, nullptr, nullptr));
    }
    CHECK(s3::rs::abs(samples[0]) > s3::rs::abs(samples[2]));
    CHECK(s3::rs::abs(samples[2] / samples[0] - settings.steering.tight_trucks_scalar) < 0.00001f);
    std::fprintf(stderr, "Authored truck tightness tilt samples: [%g, %g, %g]; tight scalar %g\n",
                 double(samples[0]), double(samples[1]), double(samples[2]),
                 double(settings.steering.tight_trucks_scalar));
}
