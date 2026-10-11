// Port of skate-core/src/physics/tests/drive_frames.rs.
#include "harness.hpp"
#include "s3/core/physics/drive_frames.hpp"

namespace {
using namespace s3::physics::drive_frames;
using s3::rs::to_bits;
using Bits3 = std::array<std::array<std::uint32_t, 3>, 3>;

Bits3 basis_bits(Basis3 basis) {
    Bits3 out{};
    for (std::size_t c = 0; c < 3; ++c)
        for (std::size_t l = 0; l < 3; ++l) out[c][l] = to_bits(basis.columns[c][l]);
    return out;
}
std::array<std::uint32_t, 3> vector_bits(Vector3 v) { return {to_bits(v.x), to_bits(v.y), to_bits(v.z)}; }
Vector3 vector_from_bits(std::array<std::uint32_t, 3> bits) {
    return Vector3{s3::rs::from_bits(bits[0]), s3::rs::from_bits(bits[1]), s3::rs::from_bits(bits[2])};
}

S3_TEST(identity_pair_has_bit_exact_identity_matrix_stage) {
    const auto parent_frame =
        relative_parent_transform(RetailAffineTransform::IDENTITY, RetailAffineTransform::IDENTITY);
    CHECK_EQ(basis_bits(parent_frame.basis),
             (Bits3{{{0x3F800000u, 0u, 0u}, {0u, 0x3F800000u, 0u}, {0u, 0u, 0x3F800000u}}}));
    CHECK_EQ(vector_bits(parent_frame.translation), (std::array<std::uint32_t, 3>{0u, 0u, 0u}));
}

S3_TEST(default_truck_transforms_match_tu3_stock_finite_path_bits) {
    const auto t = default_truck_transforms();
    const auto& slot_7712 = t[0];
    const auto& slot_7776 = t[1];
    CHECK_EQ(basis_bits(slot_7712.basis), (Bits3{{{0xB34BC7FAu, 0x3F03D987u, 0xBF5B6F50u},
                                                 {0u, 0x3F5B6F51u, 0x3F03D988u},
                                                 {0x3F7FFFFFu, 0x32D1E8FBu, 0xB32EACAFu}}}));
    CHECK_EQ(vector_bits(slot_7712.translation), (std::array<std::uint32_t, 3>{0u, 0xBD676C8Bu, 0xBE78D4FDu}));
    CHECK_EQ(basis_bits(slot_7776.basis), (Bits3{{{0xB34BC7FAu, 0x3F03D987u, 0x3F5B6F50u},
                                                 {0u, 0x3F5B6F51u, 0xBF03D988u},
                                                 {0xBF7FFFFFu, 0xB2D1E8FBu, 0xB32EACAFu}}}));
    CHECK_EQ(vector_bits(slot_7776.translation), (std::array<std::uint32_t, 3>{0u, 0xBD676C8Bu, 0x3E78D4FDu}));
}

S3_TEST(matrix_stage_preserves_tu3_vmx_fused_order) {
    const auto t = default_truck_transforms();
    const auto relative = relative_parent_transform(t[0], t[1]);
    CHECK_EQ(basis_bits(relative.basis), (Bits3{{{0xBEF05E95u, 0x3F6208D7u, 0xB395B9AEu},
                                                {0x3F6208D7u, 0x3EF05E96u, 0x3333ED95u},
                                                {0x3395B9AEu, 0xB333ED95u, 0xBF7FFFFEu}}}));
    // This finite vector distinguishes TU3's six-stage translation
    // accumulation from pre-subtracting the two translations before a dot.
    const Vector3 parent_translation = vector_from_bits({0x43D562EDu, 0xC356E21Fu, 0x4408E252u});
    const Vector3 child_translation = vector_from_bits({0x43FA50C2u, 0xC28B22BAu, 0xC3E60F58u});
    const Vector3 translation = transpose_relative_translation(t[0].basis, parent_translation, child_translation);
    CHECK_EQ(vector_bits(translation), (std::array<std::uint32_t, 3>{0x446AA4A8u, 0xC3C535F5u, 0x4293B75Au}));
}

S3_TEST(derived_level_spawn_places_all_four_wheel_spheres_on_y_zero) {
    const auto bodies = default_body_transforms();
    for (std::size_t i = 0; i < 4; ++i) {
        const float wheel_bottom =
            RETAIL_LEVEL_GROUND_DECK_CENTER_HEIGHT + bodies[i].translation.y - RETAIL_WHEEL_RADIUS;
        CHECK(s3::rs::abs(wheel_bottom) <= s3::rs::f32::EPSILON);
    }
}
}  // namespace
