// Port of skate-game/src/physics/solve/assembly_contacts_tests.rs.
#include "harness.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/world_contact.hpp"
#include "s3/game/physics/solve/assembly_contacts.hpp"

using namespace s3::game::physics::solve::assembly_contacts;
using s3::math::Vector3;
using s3::physics::board::BodyId;
using s3::physics::board_step::BoardCollision;
using s3::physics::board_step::CollisionBody;
using s3::physics::board_world::BoardWorldVolume;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::skeleton_body::SkeletonCollisionMode;
using s3::physics::skeleton_body::SkeletonCollisionSettings;

namespace {
RetailContactMaterial material() {
    RetailContactMaterial m{};
    m.static_friction = 0.5f;
    m.dynamic_friction = 0.3f;
    m.restitution = 0.0f;
    return m;
}
SkeletonCollisionMode mode() {
    SkeletonCollisionSettings settings{};
    settings.enabled = true;
    settings.normal_material = material();
    settings.compliant.fill(false);
    settings.priority.fill(0.0f);
    settings.effect_time = 0.0f;
    auto m = SkeletonCollisionMode::new_normal(settings, true);
    CHECK(m.select_driven(3).is_ok());
    return m;
}
BoardWorldVolume volume(CollisionBody body, float x) {
    BoardWorldVolume v{};
    v.body = body;
    s3::physics::collision::Sphere sphere{};
    sphere.center = Vector3::new_(x, 0.0f, 0.0f);
    sphere.radius = 0.5f;
    v.primitive = sphere;
    v.linear_velocity = Vector3::ZERO;
    v.material = material();
    return v;
}
}  // namespace

S3_TEST(board_pairs_use_actual_biped_part_groups) {
    const auto m = mode();
    CHECK_EQ(m.assembly_group, 6u);
    const std::vector<BoardWorldVolume> board = {volume(CollisionBody::Board(BodyId::Deck), 0.0f)};
    // Carried board4 excludes all these part groups.
    for (std::size_t part : {3, 7, 15, 19, 16, 20, 23, 24, 25}) {
        const std::vector<BoardWorldVolume> rider = {volume(CollisionBody::Attached(part), 0.25f)};
        std::vector<BoardCollision> contacts;
        CHECK(append(contacts, board, rider, 4, m).is_ok());
        CHECK_MSG(contacts.empty(), "carried board contacted part" + std::to_string(part));
    }
    // Released board7 can contact the original leg group17, but not torso18 or controller20.
    for (auto [part, expected] : {std::pair<std::size_t, bool>{15, true}, {19, true}, {23, false}, {24, false}}) {
        std::vector<BoardCollision> contacts;
        const std::vector<BoardWorldVolume> rider = {volume(CollisionBody::Attached(part), 0.25f)};
        CHECK(append(contacts, board, rider, 7, m).is_ok());
        CHECK_MSG(!contacts.empty() == expected, "released board part" + std::to_string(part));
    }
}

S3_TEST(assembly_gate_and_world_query_group_are_distinct) {
    auto m = mode();
    const std::vector<BoardWorldVolume> board = {volume(CollisionBody::Board(BodyId::Deck), 0.0f)};
    const std::vector<BoardWorldVolume> rider = {volume(CollisionBody::Attached(15), 0.25f)};
    m.parts[15].volume_group = 1;  // World-query group is not VolumeData212.
    std::vector<BoardCollision> contacts;
    CHECK(append(contacts, board, rider, 7, m).is_ok());
    CHECK(!contacts.empty());
    contacts.clear();
    m.assembly_group = 1;
    CHECK(append(contacts, board, rider, 7, m).is_ok());
    CHECK(contacts.empty());
}

S3_TEST(original_single_player_table_keeps_special_groups_and_symmetry) {
    for (std::uint32_t a = 0; a < 21; ++a) {
        for (std::uint32_t b = 0; b < 21; ++b) {
            CHECK_EQ(group_pair_allowed(a, b), group_pair_allowed(b, a));
        }
    }
    CHECK(!group_pair_allowed(4, 5));
    CHECK(!group_pair_allowed(4, 17));
    CHECK(group_pair_allowed(7, 17));
    CHECK(group_pair_allowed(5, 20));
    CHECK(!group_pair_allowed(7, 20));
    CHECK(group_pair_allowed(11, 11));
    CHECK(!group_pair_allowed(11, 12));
    CHECK(group_pair_allowed(12, 12));
    CHECK(group_pair_allowed(13, 13));
    CHECK(!group_pair_allowed(21, 0));
}
