// Port of skate-game/src/physics/skeleton_colliders.rs.
//! Bind original physical skater shapes to the bodies receiving solver reactions.
//! The collision owner selects enabled parts and materials; shapes and local-Z
//! capsule axes come from the same stock definition used to construct each part.
#include "s3/game/physics/skeleton_colliders.hpp"

#include <algorithm>
#include <variant>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/mass.hpp"
#include "s3/core/physics/world_contact.hpp"

namespace s3::game::physics::skeleton_colliders {

using sc::math::Basis3;
using sc::math::Vector3;
using sc::physics::board_step::CollisionBody;
using sc::physics::board_world::BoardWorldVolume;
using sc::physics::collision::Sphere;
using sc::physics::mass::MassShape;
using sc::physics::skeleton_body::SkeletonBody;
using sc::physics::skeleton_body::SkeletonCollisionMode;
using sc::physics::world_contact::ContactPrimitive;

namespace {
/// Rust `{:?}` of `MassShape`.
std::string debug_shape(const MassShape& shape) {
    return std::visit(
        [](const auto& s) -> std::string {
            using T = std::decay_t<decltype(s)>;
            using fmt::debug_f32;
            if constexpr (std::is_same_v<T, MassShape::Sphere>) {
                return "Sphere { radius: " + debug_f32(s.radius) + " }";
            } else if constexpr (std::is_same_v<T, MassShape::Capsule>) {
                return "Capsule { radius: " + debug_f32(s.radius) + ", half_length: " + debug_f32(s.half_length) +
                       " }";
            } else if constexpr (std::is_same_v<T, MassShape::RoundedBox>) {
                return "RoundedBox { half_extents: Vector3 { x: " + debug_f32(s.half_extents.x) +
                       ", y: " + debug_f32(s.half_extents.y) + ", z: " + debug_f32(s.half_extents.z) +
                       " }, radius: " + debug_f32(s.radius) + " }";
            } else if constexpr (std::is_same_v<T, MassShape::Cylinder>) {
                return "Cylinder { radius: " + debug_f32(s.radius) + ", half_length: " + debug_f32(s.half_length) +
                       ", padding: " + debug_f32(s.padding) + " }";
            } else {
                return "Unsupported";
            }
        },
        shape.value);
}
}  // namespace

rs::Result<std::vector<BoardWorldVolume>, std::string> world_volumes(const SkeletonBody& skeleton,
                                                                     const SkeletonCollisionMode& collision) {
    S3G_TRY(auto volumes, enabled_volumes(skeleton, collision));
    retain_world_volumes(volumes, collision);
    return volumes;
}

/// Volume+84 group4 disables world contacts only. Keep these shapes available
/// to the separate SkaterSkaterCollisionPipeline and its own pair filters.
void retain_world_volumes(std::vector<BoardWorldVolume>& volumes, const SkeletonCollisionMode& collision) {
    volumes.erase(std::remove_if(volumes.begin(), volumes.end(),
                                 [&](const BoardWorldVolume& volume) {
                                     if (!volume.body.is_attached())
                                         S3_PANIC("skeleton volumes must reference attached bodies");
                                     return !(collision.parts[volume.body.attached].volume_group != 4);
                                 }),
                  volumes.end());
}

rs::Result<std::vector<BoardWorldVolume>, std::string> enabled_volumes(const SkeletonBody& skeleton,
                                                                       const SkeletonCollisionMode& collision) {
    const auto transforms = skeleton.part_transforms();
    std::vector<BoardWorldVolume> volumes;
    const std::size_t n = std::min(skeleton.definition.parts.size(), collision.parts.size());
    for (std::size_t index = 0; index < n; ++index) {
        const auto& part = skeleton.definition.parts[index];
        const auto& state = collision.parts[index];
        if (!state.enabled) continue;
        if (!(state.volume_group == 0 || state.volume_group == 4)) {
            return rs::make_err(fmt::cat("Skater part", index, " requires volume group", state.volume_group,
                                         " world filtering"));
        }
        if (part.hat.has_value()) {
            return rs::make_err(std::string("The equipped hat requires its original cylinder collision query"));
        }
        const auto frame = transforms[index];
        auto xyz = [](const std::array<float, 4>& value) { return Vector3::new_(value[0], value[1], value[2]); };
        const Vector3 center = xyz(frame[3]);
        std::optional<ContactPrimitive> primitive;
        if (const auto* s = std::get_if<MassShape::Sphere>(&part.shape.value)) {
            primitive = ContactPrimitive(Sphere{center, s->radius});
        } else if (const auto* c = std::get_if<MassShape::Capsule>(&part.shape.value)) {
            primitive = ContactPrimitive(ContactPrimitive::Capsule{center, xyz(frame[2]), c->half_length, c->radius});
        } else if (const auto* b = std::get_if<MassShape::RoundedBox>(&part.shape.value)) {
            Basis3 basis{};
            for (std::size_t i = 0; i < 3; ++i) basis.columns[i] = {frame[i][0], frame[i][1], frame[i][2]};
            primitive = ContactPrimitive(ContactPrimitive::RoundedBox{center, basis, b->half_extents, b->radius});
        } else {
            return rs::make_err(fmt::cat("Skater part", index, " has no recovered collision query for ",
                                         debug_shape(part.shape)));
        }
        BoardWorldVolume v;
        v.body = CollisionBody::Attached(index);
        v.primitive = *primitive;
        v.linear_velocity = skeleton.bodies()[index].rates.linear_velocity;
        v.material = state.material;
        volumes.push_back(v);
    }
    return volumes;
}

}  // namespace s3::game::physics::skeleton_colliders
