// Port of skate-core/tests/animation_packet_reset.rs (crate integration test).
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/output/packet_reset.hpp"

using namespace s3::animation::output;
using commands_BufferError = s3::animation::commands::buffers::BufferError;
using packet_reset::AdditionalResetFields;
using physics_packet::PhysicsPosePacket;

namespace {
NativeMatrix filled(float v) { return NativeMatrix{{{v, v, v, v}, {v, v, v, v}, {v, v, v, v}, {v, v, v, v}}}; }
PhysicsPosePacket packet(std::uint32_t count) {
    PhysicsPosePacket p;
    p.bone_count = count;
    p.hierarchy = std::vector<NativeMatrix>(3, filled(7.0f));
    p.local = std::vector<NativeMatrix>(3, filled(8.0f));
    p.timestep = 99.0f;
    p.foot_surface_ids = {123, 456};
    p.flags = UINT32_MAX;
    p.board_flipped = true;
    p.mirrored = true;
    p.riding_switch = true;
    p.riding_fakie = true;
    p.weight_forwards = true;
    p.regular_stance = true;
    p.air_dismount_revert_frames = 100;
    return p;
}
AdditionalResetFields fields() {
    AdditionalResetFields f;
    f.compression = 9.0f;
    f.foot_ik_influence = {4.0f, 5.0f};
    f.next_step_position_valid = true;
    f.actor_flag_1904_bit23 = true;
    f.actor_flag_1908_bit2 = true;
    f.external_impulse_active = true;
    f.external_physics_input_active = true;
    f.externally_controlled = true;
    f.prevent_manual_respawn = true;
    f.ignore_respawn_reset_button = 0xff;
    f.force_braking = true;
    f.truck_tightness = 4.0f;
    f.wheel_hardness = 5.0f;
    const float n = s3::rs::f32::NAN_;
    for (auto& v : f.auxiliary_vectors) v = {n, n, n, n};
    f.requested_physics_mode = 4;
    return f;
}
}  // namespace

S3_TEST(reset_clears_all_owned_fields_and_uses_packed_basis_with_zero_translation_w) {
    PhysicsPosePacket p = packet(2);
    AdditionalResetFields f = fields();
    const NativeMatrix* hierarchy_allocation = p.hierarchy.data();
    const NativeMatrix* local_allocation = p.local.data();
    packet_reset::reset(p, f).unwrap();
    const NativeMatrix expected{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
    CHECK(p.local[0] == expected && p.local[1] == expected);
    CHECK(p.hierarchy[0] == expected && p.hierarchy[1] == expected);
    CHECK(p.local[2] == filled(8.0f));
    CHECK(p.hierarchy[2] == filled(7.0f));
    CHECK(p.hierarchy.data() == hierarchy_allocation);
    CHECK(p.local.data() == local_allocation);
    CHECK_EQ(p.bone_count, 2u);
    CHECK_EQ(s3::rs::to_bits(p.timestep), 0x3c888889u);
    CHECK_EQ(p.flags, 0x013fffffu);
    CHECK((p.foot_surface_ids == std::array<std::uint32_t, 2>{0, 0}));
    CHECK_EQ(p.air_dismount_revert_frames, 0);
    CHECK(!p.board_flipped && !p.mirrored && !p.riding_switch && !p.riding_fakie && !p.weight_forwards &&
          !p.regular_stance);
    CHECK_EQ(f.compression, 0.5f);
    CHECK((f.foot_ik_influence == std::array<float, 2>{0.0f, 0.0f}));
    CHECK_EQ(f.truck_tightness, 0.0f);
    CHECK_EQ(f.wheel_hardness, 0.0f);
    CHECK_EQ(f.ignore_respawn_reset_button, std::uint8_t(0));
    CHECK_EQ(f.requested_physics_mode, 1u);
    CHECK(!f.next_step_position_valid && !f.actor_flag_1904_bit23 && !f.actor_flag_1908_bit2 &&
          !f.external_impulse_active && !f.external_physics_input_active && !f.externally_controlled &&
          !f.prevent_manual_respawn && !f.force_braking);
    for (const auto& vector : f.auxiliary_vectors) {
        for (float v : vector) CHECK_EQ(s3::rs::to_bits(v), 0u);
    }
}

S3_TEST(reset_preserves_every_flag_outside_the_recovered_clear_mask) {
    for (std::uint32_t bit = 0; bit < 32; ++bit) {
        PhysicsPosePacket p = packet(0);
        AdditionalResetFields f = fields();
        p.flags = 1u << bit;
        packet_reset::reset(p, f).unwrap();
        const bool preserved = bit <= 21 || bit == 24;
        CHECK_MSG(p.flags == (preserved ? 1u << bit : 0u), "bit " << bit);
    }
}

S3_TEST(zero_and_signed_negative_counts_skip_matrices_but_still_reset_scalars) {
    for (std::uint32_t count : {0u, 0x80000000u, UINT32_MAX}) {
        PhysicsPosePacket p = packet(count);
        AdditionalResetFields f = fields();
        packet_reset::reset(p, f).unwrap();
        CHECK_EQ(p.bone_count, count);
        CHECK(p.local == std::vector<NativeMatrix>(3, filled(8.0f)));
        CHECK_EQ(f.compression, 0.5f);
        CHECK_EQ(p.flags, 0x013fffffu);
    }
}

S3_TEST(short_allocation_is_rejected_without_partial_reset) {
    PhysicsPosePacket p = packet(4);
    AdditionalResetFields f = fields();
    const auto r = packet_reset::reset(p, f);
    CHECK(r.is_err() && r.unwrap_err() == commands_BufferError(commands_BufferError::RangeOutsideAllocation{}));
    CHECK_EQ(p.flags, UINT32_MAX);
    CHECK_EQ(f.requested_physics_mode, 4u);
    CHECK(p.local == std::vector<NativeMatrix>(3, filled(8.0f)));
}
