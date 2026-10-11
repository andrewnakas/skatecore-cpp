// Port of skate-core/src/physics/tests/contact.rs (tests/contact_solver.rs has no tests).
#include "harness.hpp"
#include "s3/core/physics/contact.hpp"

namespace {
using namespace s3::physics::contact;
using s3::rs::from_bits;
using s3::rs::to_bits;

RetailContactBodyState body(std::uint32_t contact_body_id, Vector3 center_of_mass) {
    return RetailContactBodyState{
        contact_body_id,
        center_of_mass,
        contact_body_id + 10,
        Vector3{1.0f, 2.0f, 3.0f},
        0.25f,
        Vector3{4.0f, 5.0f, 6.0f},
        7,
        Vector3{8.0f, 9.0f, 10.0f},
        11.0f,
        Vector3{12.0f, 13.0f, 14.0f},
        15,
        Vector3::ZERO,
        Vector3::ZERO,
    };
}

RetailContactInput input(Vector3 normal) {
    return RetailContactInput{
        Vector3{2.0f, 3.0f, 4.0f}, Vector3{2.0f, 3.0f, 4.0f}, normal, 0.1f, 0.8f, 0.7f, 0x12345678u,
    };
}

S3_TEST(material_combine_matches_tu3_branch_directions) {
    const auto combined = combine_contact_materials(
        RetailContactMaterial{from_bits(0x3F4CCCCDu), from_bits(0x3F333333u), from_bits(0x3E4CCCCDu)},
        RetailContactMaterial{from_bits(0x3F000000u), from_bits(0x3F666666u), from_bits(0x3DCCCCCDu)});
    CHECK_EQ(to_bits(combined.static_friction), 0x3F4CCCCDu);
    CHECK_EQ(to_bits(combined.dynamic_friction), 0x3F666666u);
    CHECK_EQ(to_bits(combined.restitution), 0x3DCCCCCDu);
}

S3_TEST(material_combine_is_commutative_for_finite_values) {
    const RetailContactMaterial a{0.8f, 0.7f, 0.0f};
    const RetailContactMaterial b{0.5f, 0.9f, 0.2f};
    CHECK_EQ(combine_contact_materials(a, b), combine_contact_materials(b, a));
}

void assert_close(float actual, float expected) {
    CHECK_MSG(s3::rs::abs(actual - expected) <= 1.0e-6f, "actual " << actual << ", expected " << expected);
}
void assert_vec_close(Vector3 actual, Vector3 expected) {
    assert_close(actual.x, expected.x);
    assert_close(actual.y, expected.y);
    assert_close(actual.z, expected.z);
}

S3_TEST(angular_contact_rates_use_omega_cross_arm) {
    auto body_a = body(1, Vector3{1.0f, 3.0f, 4.0f});
    body_a.angular_velocity = Vector3{0.0f, 0.0f, 2.0f};
    const auto contact = generate_contact(input(Vector3{0.0f, -1.0f, 0.0f}), body_a,
                                          body(2, Vector3{2.0f, 3.0f, 4.0f}));
    assert_vec_close(contact.relative_velocity, Vector3{0.0f, -2.0f, 0.0f});
}

S3_TEST(contact_frame_is_orthogonal) {
    const Vector3 normal{0.36f, 0.48f, 0.8f};
    auto body_b = body(2, Vector3::ZERO);
    body_b.linear_velocity = Vector3{2.0f, -1.0f, 0.5f};
    const auto contact = generate_contact(input(normal), body(1, Vector3::ZERO), body_b);
    assert_close(dot(contact.normal, contact.tangent_0), 0.0f);
    assert_close(dot(contact.normal, contact.tangent_1), 0.0f);
    assert_close(dot(contact.tangent_0, contact.tangent_1), 0.0f);
    assert_vec_close(cross(contact.normal, contact.tangent_0), contact.tangent_1);
}

S3_TEST(contact_copies_header_ids_materials_tag_and_body_workspaces) {
    const auto body_a = body(41, Vector3{1.0f, 2.0f, 3.0f});
    const auto body_b = body(52, Vector3{4.0f, 5.0f, 6.0f});
    const auto collision = input(Vector3{0.0f, 1.0f, 0.0f});
    const auto contact = generate_contact(collision, body_a, body_b);
    CHECK_EQ(contact.body_a_id, 41u);
    CHECK_EQ(contact.body_b_id, 52u);
    CHECK_EQ(contact.restitution, collision.restitution);
    CHECK_EQ(contact.static_friction, collision.static_friction);
    CHECK_EQ(contact.dynamic_friction, collision.dynamic_friction);
    CHECK_EQ(contact.tag, collision.tag);
    CHECK_EQ(contact.body_a_workspace, RetailContactWorkspace::from(body_a));
    CHECK_EQ(contact.body_b_workspace, RetailContactWorkspace::from(body_b));
    CHECK_EQ(contact.body_a_workspace.reaction_id, 51u);
    CHECK_EQ(contact.body_b_workspace.reaction_id, 62u);
}

S3_TEST(recovered_tangent_constants_keep_their_exact_binary32_bits) {
    CHECK_EQ(to_bits(CONTACT_TANGENT_MINIMUM_SQUARED), 0x00800000u);
    CHECK_EQ(to_bits(CONTACT_FALLBACK_HALF), 0x3F000000u);
}
}  // namespace
