// Port of skate-game/src/physics/respawn.rs `mod tests`.
#include "harness.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics/ground.hpp"
#include "s3/game/physics/respawn.hpp"

using namespace s3::game::physics::respawn;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::player::respawn::Candidate;
using s3::player::respawn::Observation;
using A4 = std::array<float, 4>;

S3_TEST(real_scene_records_a_checkpoint_and_recovers_it_from_an_unsupported_position) {
    const auto world = s3::game::physics::ground::world(RetailContactMaterial{0.f, 0.f, 0.f});
    const Settings settings{0.4f, 0.5f, 1.f, 0.75f, 15};
    Scene scene{world, settings};
    auto history = History::new_(Candidate{IDENTITY, 0, false, 0.f});
    // The first conditioning samples clear the construction cooldown.
    CHECK(!history.recording_due(119, {2.f, 0.f, 0.f, 0.f}));
    auto transform = IDENTITY;
    transform[3] = {2.f, 0.2f, 0.f, 0.f};
    Observation o{};
    o.measurements = 120;
    o.root_position = transform[3];
    o.com_position = {1000.f, 0.f, 0.f, 0.f};
    o.teleport_requested = false;
    o.physical_state = 100;
    o.state_frames = 16;
    o.ground_suppressed = false;
    o.offboard_correction = false;
    o.ground_category = 1;
    o.foot_categories = {1, 1};
    o.riding_transform = transform;
    o.offboard_transform = IDENTITY;
    o.stance = 1;
    o.alternate_world = false;
    CHECK(history.observe(o, 15, scene).is_ok());
    const auto selected = history.automatic(0, scene).unwrap();
    CHECK(selected.transform == transform);
    CHECK_EQ(selected.stance, 1u);
    // Successful historical entries are consumed: a second failure uses spawn.
    CHECK(history.automatic(0, scene).unwrap().transform == IDENTITY);
}

S3_TEST(native_heading_keeps_low_speed_and_vertical_axes) {
    auto source = IDENTITY;
    source[0] = {0.f, 0.f, -1.f, 0.f};
    source[2] = {1.f, 0.f, 0.f, 0.f};
    CHECK(heading(source, {0.f, 0.f, 0.5f, 0.f}) == source);
    CHECK(heading(source, {0.f, 5.f, 0.1f, 0.f}) == source);
    const auto result = heading(source, {0.f, 0.f, 2.f, 0.f});
    CHECK(result[2] == (A4{0.f, 0.f, 1.f, 0.f}));
    CHECK(result[3] == source[3]);
}
