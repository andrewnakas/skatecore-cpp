// Port of skate-game/src/physics/offboard/board_manager/runtime.rs
#include "s3/game/physics/offboard/board_manager/runtime.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::offboard::board_manager::runtime {

namespace native = ::s3::player::offboard::board_possession;
using board_possession::Materials;
using board_possession::vector;
using sc::physics::board::BodyId;
using sc::physics::skeleton_animation_record::compose_affine;

namespace {
template <std::size_t N>
std::array<float, N> f32_from_bits(const std::array<std::uint32_t, N>& a) {
    return rs::from_bits(a);
}
template <std::size_t N, std::size_t M>
std::array<std::array<float, N>, M> f32_from_bits(const std::array<std::array<std::uint32_t, N>, M>& a) {
    std::array<std::array<float, N>, M> out{};
    for (std::size_t i = 0; i < M; ++i) out[i] = rs::from_bits(a[i]);
    return out;
}
}  // namespace

rs::Result<LiveState, std::string> LiveState::load(const sd::Collections& data, const GamePhysics& physics) {
    const auto& s = physics.settings;
    S3G_TRY(float friction, data.float_("physics_wipeout", "default", "SkateboardFriction"));
    LiveState out{};
    out.volumes.deck = true;
    out.volumes.trucks = s.truck_collisions;
    out.volumes.wheels = true;
    for (const auto& c : s.deck_geometry.children) out.volumes.deck_children.push_back(c.collision_enabled);
    out.alignment = Alignment{{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, false};
    out.alignment_active = false;
    out.standard_materials = {s.standard_wheel_material, s.truck_material, s.deck_material};
    out.released_material.static_friction = friction;
    out.released_material.dynamic_friction = friction;
    S3G_TRY(out.released_material.restitution, data.float_("physics_wipeout", "default", "SkateboardRestitution"));
    S3G_TRY(out.standard_drag, board_possession::settings::standard_angular_drag(data));
    out.output = std::nullopt;
    return out;
}

Effects LiveState::effects(GamePhysics& physics, std::uint8_t& animated, float dt) {
    Effects e;
    e.board = &physics.board;
    e.animated_290 = &animated;
    e.wiping_out = &physics.board_wiping_out;
    e.alignment_ = &alignment;
    e.alignment_active = &alignment_active;
    e.volumes = &volumes;
    e.materials = Materials{&physics.settings.wheel_material, &physics.settings.truck_material,
                            &physics.settings.deck_material};
    e.standard_materials = standard_materials;
    e.released_material = released_material;
    e.standard_angular_drag = standard_drag;
    e.processed_dt_2604 = dt;
    return e;
}

void LiveState::publish_volumes(GamePhysics& physics) const {
    physics.settings.truck_collisions = volumes.trucks;
    auto& children = physics.settings.deck_geometry.children;
    for (std::size_t i = 0; i < children.size() && i < volumes.deck_children.size(); ++i) {
        children[i].collision_enabled = volumes.deck_children[i];
    }
}

bool LiveState::volume_enabled(CollisionBody body) const {
    if (body.kind != CollisionBody::Kind::Board) return true;
    if (body.board == BodyId::Deck) return volumes.deck;
    if (body.board == BodyId::FrontTruck || body.board == BodyId::BackTruck) return volumes.trucks;
    return volumes.wheels;
}

Observation observe(const GamePhysics& physics, const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const auto& frames = skater.skeleton_input.drive_frames;
    auto board = solve::deck_frame(physics.board);
    //82C01440..60: Processed64 is the unmodified deck part transform.
    //The effective axes are written separately at320/352.
    if (skater.player_input.toolkit.has_value()) board = skater.player_input.toolkit->deck;
    Observation o{};
    o.processed = native::Processed{
        board,
        f32_from_bits(p.effective_anim_transform_192),
        f32_from_bits(p.vectors_544_560_592_608[2]),
        f32_from_bits(p.vectors_880_896_912_928_944[2]),
        f32_from_bits(p.vectors_400_416[0]),
        f32_from_bits(p.vectors_464_480_496_512_528[0]),
        p.flags_2476,
        p.flags_2480,
        p.flags_2488,
    };
    o.board_collision_flags_872 = physics.riding.ground.collision_flags;
    o.board_state_840 = surface(physics);
    //82D761F0 reads Collision1232 + part*112 +99 (group byte1331).
    o.hand_contacts = {skater.collision_feedback.bones[3].groups[3], skater.collision_feedback.bones[7].groups[3]};
    o.physical_hand_positions = {skater.skeleton.record.pose[3][3], skater.skeleton.record.pose[7][3]};
    o.animation_board_frame_12624 = frames[0];
    o.animation_hand_frames = {frames[3], frames[7]};
    //82BE3170 composes Skeleton11920 with current12624; not physical pose.
    o.attachment_frame_0 = compose_affine(skater.animated_skeleton.roots.animation_to_world, frames[0]);
    return o;
}

std::uint32_t surface(const GamePhysics& physics) {
    if ((physics.riding.ground.collision_flags & 0x02000000u) != 0) return 12;
    std::array<std::uint32_t, 32> counts{};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t surface = static_cast<std::size_t>(physics.riding.wheel_lines.physics_surfaces[i]);
        if (surface != 0) counts[surface] += physics.riding.ground.parts[i].in_contact ? 4 : 1;
    }
    std::size_t best = 1;
    std::uint32_t count = 0;
    for (std::size_t i = 1; i <= 13; ++i) {
        if (counts[i] > count) {
            best = i;
            count = counts[i];
        }
    }
    return static_cast<std::uint32_t>(best);
}

void update(GamePhysics& physics, SkaterRuntime& skater) {
    if (!skater.skateboard_controller.fields.system_on_452) return;
    const Observation observation = observe(physics, skater);
    Effects effects = skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290,
                                                           skater.player_input.processed.timestep_2604);
    skater.board_possession.update(skater.skateboard_controller.fields, observation, effects);
    skater.board_possession_live.publish_volumes(physics);
}

void reset_for_teleport(GamePhysics& physics, SkaterRuntime& skater) {
    const Observation observation = observe(physics, skater);
    Effects effects = skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290,
                                                           skater.player_input.processed.timestep_2604);
    skater.board_possession.reset_for_teleport(skater.skateboard_controller.fields, observation, effects);
    skater.board_possession_live.output = std::nullopt;
    skater.board_possession_live.publish_volumes(physics);
}

void finish_teleport(GamePhysics& physics, SkaterRuntime& skater) {
    skater.board_possession_live
        .effects(physics, skater.ground_lifecycle.board_animated_290, skater.player_input.processed.timestep_2604)
        .standard_board();
    skater.board_possession_live.publish_volumes(physics);
}

Fill publish(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& live = skater.board_possession_live;
    if (live.alignment_active) {
        using native::math::dot;
        auto neg = [](const std::array<float, 4>& v) { return std::array<float, 4>{-v[0], -v[1], -v[2], -v[3]}; };
        for (const auto& part : physics.riding.ground.parts) {
            if (part.in_contact && (dot(vector(part.normal), neg(live.alignment.first_1008)) > live.alignment.factor_1040 ||
                                    dot(vector(part.normal), neg(live.alignment.second_1024)) >
                                        live.alignment.factor_1040)) {
                physics.riding.ground.collision_flags |= 0x04000000u;
            }
        }
    }
    const Observation observation = observe(physics, skater);
    const auto bone = compose_affine(skater.animated_skeleton.roots.animation_to_world, skater.skeleton_input.drive_frames[11]);
    const Fill out = skater.board_possession.fill(skater.skateboard_controller.fields, observation.processed, bone);
    auto& physical = skater.player_input.physical.off_board;
    physical.angle_36 = out.angle_36;
    physical.angle_40 = out.angle_40;
    physical.flag_311 = static_cast<std::uint8_t>(out.held_311);
    physical.free_board_312 = static_cast<std::uint8_t>(out.free_312);
    physical.returning_board_313 = static_cast<std::uint8_t>(out.returning_313);
    physical.hiding_board_321 = static_cast<std::uint8_t>(out.hiding_321);
    physical.dropping_board_322 = static_cast<std::uint8_t>(out.flag_322);
    physical.retrieving_board_323 = static_cast<std::uint8_t>(out.flag_323);
    physical.flag_324 = static_cast<std::uint8_t>(out.flag_324);
    skater.board_possession_live.output = out;
    return out;
}

}  // namespace s3::game::physics::offboard::board_manager::runtime
