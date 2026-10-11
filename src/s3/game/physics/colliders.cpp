// Port of skate-game/src/physics/colliders.rs.
//! Bind stock child geometry to the live solver part transforms.
#include "s3/game/physics/colliders.hpp"

#include <cmath>
#include <variant>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/mass.hpp"
#include "s3/game/physics/settings.hpp"

namespace s3::game::physics::colliders {

using sc::math::Basis3;
using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::physics::board_step::CollisionBody;
using sc::physics::board_world::BoardWorldVolume;
using sc::physics::collision::Sphere;
using sc::physics::contact::RetailContactMaterial;
using sc::physics::drive_frames::RetailAffineTransform;
using sc::physics::mass::DeckShape;
using sc::physics::mass::MassShape;
using sc::physics::world_contact::ContactPrimitive;
using sc::physics::world_contact::transform_triangle_volume;

std::vector<BoardWorldVolume> world_volumes(const sc::physics::board_runtime::BoardRuntime& board,
                                            const settings::PhysicsSettings& settings) {
    const auto poses = board.part_transforms();
    std::vector<BoardWorldVolume> volumes;
    // SerializeVolumes preserves part order, then compound child order.
    for (const BodyId id : sc::physics::board::BodyId_ORDER) {
        const auto& body = board.bodies()[sc::physics::board::index(id)];
        if (body.state_flags == 1) continue;
        const auto pose = poses[sc::physics::board::index(id)];
        auto add = [&](ContactPrimitive primitive, RetailContactMaterial material) {
            BoardWorldVolume v;
            v.body = CollisionBody::Board(id);
            v.primitive = primitive;
            v.material = material;
            v.linear_velocity = body.rates.linear_velocity;
            volumes.push_back(v);
        };
        switch (id) {
            case BodyId::Deck: {
                for (const auto& child : settings.deck_geometry.children) {
                    if (!child.collision_enabled) continue;
                    const auto world = compose(pose, child.transform);
                    const ContactPrimitive primitive = std::visit(
                        [&](const auto& s) -> ContactPrimitive {
                            using T = std::decay_t<decltype(s)>;
                            if constexpr (std::is_same_v<T, DeckShape::Sphere>) {
                                return Sphere{world.translation, s.radius};
                            } else if constexpr (std::is_same_v<T, DeckShape::Capsule>) {
                                return capsule(world, s.radius, s.half_length);
                            } else if constexpr (std::is_same_v<T, DeckShape::RoundedBox>) {
                                return ContactPrimitive::RoundedBox{world.translation, world.basis, s.half_extents,
                                                                    s.radius};
                            } else {
                                return transform_triangle_volume(s.vertices, s.fatness, s.edge_cosines,
                                                                 s.volume_flags, world.basis, world.translation);
                            }
                        },
                        child.shape.value);
                    add(primitive, settings.deck_material);
                }
                break;
            }
            case BodyId::FrontTruck:
            case BodyId::BackTruck: {
                if (settings.truck_collisions) {
                    const auto* c = std::get_if<MassShape::Capsule>(&settings.truck_shape.value);
                    if (c == nullptr) S3_PANIC("TU382C0A6F8 constructs a capsule for each truck");
                    add(capsule(pose, c->radius, c->half_length), settings.truck_material);
                }
                break;
            }
            default:
                add(Sphere{pose.translation, settings.wheel_radius}, settings.wheel_material);
                break;
        }
    }
    return volumes;
}

ContactPrimitive capsule(RetailAffineTransform pose, float radius, float half_length) {
    const auto z = pose.basis.columns[2];
    return ContactPrimitive::Capsule{pose.translation, Vector3::new_(z[0], z[1], z[2]), half_length, radius};
}

// Same multiply/add order as the Volume-to-world vertex path82ADE468.
Vector3 transform(Basis3 basis, Vector3 point, Vector3 origin) {
    const std::array<float, 3> translation{origin.x, origin.y, origin.z};
    auto component = [&](std::size_t row) {
        return std::fma(point.z, basis.columns[2][row],
                        std::fma(point.y, basis.columns[1][row],
                                 std::fma(point.x, basis.columns[0][row], translation[row])));
    };
    const float x = component(0);
    const float y = component(1);
    const float z = component(2);
    return Vector3::new_(x, y, z);
}

RetailAffineTransform compose(RetailAffineTransform parent, RetailAffineTransform child) {
    RetailAffineTransform out = child;
    for (std::size_t col = 0; col < 3; ++col) {
        const auto c = child.basis.columns[col];
        for (std::size_t row = 0; row < 3; ++row) {
            // CreateGP82AD8758 starts basis composition with a product;
            // only position composition includes an initial translation.
            out.basis.columns[col][row] =
                std::fma(c[2], parent.basis.columns[2][row],
                         std::fma(c[1], parent.basis.columns[1][row], c[0] * parent.basis.columns[0][row]));
        }
    }
    out.translation = transform(parent.basis, child.translation, parent.translation);
    return out;
}

}  // namespace s3::game::physics::colliders
