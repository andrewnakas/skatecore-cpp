// Port of the #[cfg(test)] modules of skate-game/src/physics/player_input/mod.rs,
// player_input/ground_position.rs and player_input/reset.rs.
#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/physics/player_input.hpp"

using namespace s3::game::physics::player_input;
using s3::player::input_phase::ProcessedPhysicsInput;

// ---- mod.rs (Rust `#[ignore]`: private stock collections; here pack-backed, skip without pack) ----
S3_TEST(original_player_input_settings_and_reset_load) {
    const auto* vfs = s3test::game::assets();
    if (!vfs) return;
    auto loaded = s3::data::Collections::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK(loaded.ok);
    if (!loaded.ok) return;
    const auto r = PlayerInputRuntime::load(loaded.value);
    CHECK_MSG(r.is_ok(), (r.is_err() ? r.unwrap_err() : std::string()));
    if (r.is_err()) return;
    const auto& state = r.unwrap();
    CHECK_EQ(state.player.flags_1296, 0xe00c0000u);
    CHECK(!state.pending_teleport().has_value());
    CHECK_EQ(state.physical.ground.scalar_276, -1.0f);
    CHECK(state.dynamic_normal.normal == s3::math::Vector3::new_(0.0f, 1.0f, 0.0f));
}

// ---- ground_position.rs ----
S3_TEST(minimum_height_is_measured_in_the_ground_frame) {
    const AnimationPartTransform ground = {{
        {0.0f, 1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {99.0f, 99.0f, 99.0f, 99.0f},
    }};
    const std::array<std::array<float, 4>, 4> wheels = {{
        {1.0f, 2.0f, 3.0f, 0.0f},
        {2.0f, 4.0f, 5.0f, 0.0f},
        {3.0f, 6.0f, 7.0f, 0.0f},
        {4.0f, 8.0f, 9.0f, 0.0f},
    }};
    CHECK((ground_position::wheel_ground_position(wheels, ground) == std::array<float, 4>{4.0f, 5.0f, 6.0f, 0.0f}));
}

// ---- reset.rs ----
S3_TEST(reset_retains_only_native_packet_lanes_and_masks) {
    ProcessedPhysicsInput input{};
    input.flags_2468 = UINT32_MAX;
    input.flags_2488 = UINT32_MAX;
    input.external_physics_1616.flags = UINT32_MAX;
    input.external_physics_1616.vectors[0] = {7, 7, 7, 7};
    for (auto& v : input.vectors_720_784_800_816_832_864) v = {9, 9, 9, 9};
    input.vector_1520 = {11, 11, 11, 11};
    input.scalar_2652 = 123.0f;
    reset::reset_processed(input);
    using W = std::array<std::uint32_t, 4>;
    CHECK_EQ(input.flags_2468, 0x2008u);
    CHECK_EQ(input.flags_2488, 0x001fffffu);
    CHECK_EQ(input.external_physics_1616.flags, 0x01ffffffu);
    CHECK((input.external_physics_1616.vectors[0] == W{7, 7, 7, 7}));
    CHECK((input.vectors_720_784_800_816_832_864[3] == W{0, 0, 0, 0}));
    CHECK((input.vectors_720_784_800_816_832_864[4] == W{9, 9, 9, 9}));
    CHECK((input.vector_1520 == W{11, 11, 11, 11}));
    CHECK_EQ(input.scalar_2652, 0.0f);
    CHECK_EQ(input.state_variant_index_2528, 1u);
}
