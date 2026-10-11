// Port of skate-game/src/physics/network.rs
//! Physics/transport boundary. Remote dynamic proxies join the existing solve;
//! only the locally owned assembly retains its solved reactions.
#include "s3/game/physics/network.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <variant>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/game/animation.hpp"
#include "s3/game/glam_ext.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/colliders.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_colliders.hpp"

namespace s3::game::physics::network {

using ::s3::physics::board_step::CollisionBody;
using ::s3::physics::collision::Sphere;
using ::s3::physics::rigid_body::RetailQuaternion;
using ::s3::physics::rigid_body::world_inverse_inertia;
using ::s3::physics::world_contact::triangle_from_volume;
using glam::Mat3;
using glam::Quat;

Pose pose(const Mat4& m) {
    // bevy Transform::from_matrix: to_scale_rotation_translation.
    const auto [scale, rotation, translation] = m.to_scale_rotation_translation();
    (void)scale;
    return Pose{translation.to_array(), rotation.normalize().to_array()};
}

Mat4 matrix(const Pose& p) {
    return glam::mat4_from_rotation_translation(Quat::from_array(p.q).normalize(), Vec3::from_array(p.p));
}

std::array<float, 3> xyz(Vector3 v) { return {v.x, v.y, v.z}; }
Vector3 vector(std::array<float, 3> v) { return Vector3::new_(v[0], v[1], v[2]); }
Vector3 add(Vector3 a, Vector3 b) { return Vector3::new_(a.x + b.x, a.y + b.y, a.z + b.z); }

Pose body_pose(const BodySnapshot& body) {
    const auto q = body.rates.orientation;
    return Pose{xyz(body.rates.position), Quat::from_xyzw(q.x, q.y, q.z, q.w).normalize().to_array()};
}

std::vector<std::size_t> anchors(const SkaterRuntime& skater) {
    std::vector<std::size_t> indices(skater.animated_skeleton.bone_indices.begin(),
                                     skater.animated_skeleton.bone_indices.end());
    const auto b = skater.skeleton_output.pose.board_bones;
    indices.insert(indices.end(), {b.front_truck, b.back_truck, b.front_left_wheel, b.front_right_wheel,
                                   b.back_left_wheel, b.back_right_wheel});
    // The physical head body is NECK1; HEAD is a separate visual joint.
    // Replicate it explicitly rather than replacing its animated neck offset.
    const auto& names = skater.animation.evaluator->frames.bone_names;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto& n = names[i];
        const std::string head = "HEAD";
        bool eq = n.size() == head.size();
        for (std::size_t c = 0; eq && c < n.size(); ++c)
            eq = std::toupper(static_cast<unsigned char>(n[c])) == head[c];
        if (eq) {
            indices.push_back(i);
            break;
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

BodyState capture_body(const GamePhysics& physics, const SkaterRuntime& skater) {
    std::uint64_t enabled = 0;
    {
        const auto& bodies = physics.board.bodies();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].state_flags != 1 &&
                skater.board_possession_live.volume_enabled(
                    CollisionBody::Board(::s3::physics::board::BodyId_ORDER[i]))) {
                enabled |= std::uint64_t(1) << i;
            }
        }
    }
    for (std::size_t i = 0; i < skater.skeleton_collision.parts.size(); ++i) {
        const auto& part = skater.skeleton_collision.parts[i];
        if (part.enabled && part.volume_group == 0) enabled |= std::uint64_t(1) << (7 + i);
    }
    if (skater.skeleton_collision.is_ragdoll) enabled |= std::uint64_t(1) << 63;
    BodyState out;
    out.root = pose(animation::native_matrix(skater.animated_skeleton.roots.animation_to_world));
    out.enabled = enabled;
    auto push = [&](const BodySnapshot& b) {
        out.bodies.push_back(Body{body_pose(b), xyz(b.rates.linear_velocity), xyz(b.rates.angular_velocity)});
    };
    for (const auto& b : physics.board.bodies()) push(b);
    for (const auto& b : skater.skeleton.bodies()) push(b);
    return out;
}

PoseState capture_pose(const SkaterRuntime& skater, const std::vector<std::size_t>& anchors) {
    PoseState out;
    out.root = pose(animation::native_matrix(skater.animated_skeleton.roots.animation_to_world));
    for (const std::size_t index : anchors) {
        if (index < skater.render_pose.size()) {
            out.bones.push_back(
                Bone{static_cast<std::uint16_t>(index), pose(animation::native_matrix(skater.render_pose[index]))});
        }
    }
    return out;
}

namespace {
/// Stable textual fingerprint input. NOT the Rust `{:?}` text (see the port note below).
void put(std::ostringstream& os, float v) { os << fmt::debug_f32(v) << ','; }
void put(std::ostringstream& os, const Vector3& v) {
    put(os, v.x);
    put(os, v.y);
    put(os, v.z);
}
void put(std::ostringstream& os, const ::s3::math::Basis3& b) {
    for (const auto& c : b.columns)
        for (float v : c) put(os, v);
}
}  // namespace

rs::Result<Schema, std::string> Schema::new_(const GamePhysics& physics, const SkaterRuntime& skater) {
    auto mode = ::s3::physics::skeleton_body::SkeletonCollisionMode::new_normal(skater.skeleton_collision.settings,
                                                                                 false);
    for (auto& part : mode.parts) {
        part.enabled = true;
        part.volume_group = 0;
    }
    auto volumes = colliders::world_volumes(physics.board, physics.settings);
    S3G_TRY(auto skeleton_volumes, skeleton_colliders::world_volumes(skater.skeleton, mode));
    volumes.insert(volumes.end(), skeleton_volumes.begin(), skeleton_volumes.end());
    const auto bodies = capture_body(physics, skater);
    Schema out;
    for (auto v : volumes) {
        std::size_t i = 0;
        if (v.body.is_board()) {
            i = ::s3::physics::board::index(v.body.board);
        } else if (v.body.is_attached()) {
            i = v.body.attached + 7;
        } else {
            S3_PANIC("internal error: entered unreachable code");
        }
        v.primitive = transform(v.primitive, matrix(bodies.bodies.at(i).pose).inverse());
        out.volumes.emplace_back(i, v);
    }
    // PORT NOTE: Rust hashes `format!("v2{:?}...", settings.masses, settings.authored,
    // settings.deck_geometry, settings.truck_shape, (wheel_radius, truck_collisions),
    // skeleton parts, skeleton bones)`. The derived Debug text of those core structs is not
    // reproduced in C++; this inactive multiplayer path hashes an equivalent C++-only
    // serialization of the board masses/authored frames and scalars, so fingerprints only
    // agree between C++ peers.
    const auto& settings = physics.settings;
    std::ostringstream os;
    os << "v2";
    for (const auto& m : settings.masses) {
        put(os, m.dynamics.inverse_tensor);
        put(os, m.dynamics.inverse_mass);
        put(os, m.dynamics.spherical);
        put(os, m.dynamics.maximum_linear_velocity);
        put(os, m.dynamics.maximum_angular_velocity);
        put(os, m.dynamics.linear_drag);
        put(os, m.dynamics.angular_drag);
    }
    for (const auto& a : settings.authored) {
        put(os, a.basis);
        put(os, a.translation);
    }
    os << settings.deck_geometry.children.size() << ',';
    put(os, settings.wheel_radius);
    os << (settings.truck_collisions ? "true" : "false") << ',';
    for (const auto& part : skater.skeleton.definition.parts) {
        put(os, part.inverse_mass_animated);
        put(os, part.inverse_mass_ragdoll);
    }
    const std::string text = os.str();
    out.fingerprint = ::s3::net::hash(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    return out;
}

ContactPrimitive transform(const ContactPrimitive& shape, const Mat4& m) {
    auto point = [&](Vector3 p) { return vector(glam::transform_point3(m, Vec3::from_array(xyz(p))).to_array()); };
    auto direction = [&](Vector3 p) {
        return vector(glam::transform_vector3(m, Vec3::from_array(xyz(p))).to_array());
    };
    return std::visit(
        [&](const auto& s) -> ContactPrimitive {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, Sphere>) {
                Sphere out = s;
                out.center = point(s.center);
                return out;
            } else if constexpr (std::is_same_v<T, ContactPrimitive::Capsule>) {
                return ContactPrimitive::Capsule{point(s.center), direction(s.axis), s.half_length, s.radius};
            } else if constexpr (std::is_same_v<T, ContactPrimitive::RoundedBox>) {
                const auto cols = (Mat3::from_mat4(m) * Mat3::from_cols_array_2d(s.basis.columns)).to_cols_array_2d();
                ::s3::math::Basis3 basis{};
                basis.columns = cols;
                return ContactPrimitive::RoundedBox{point(s.center), basis, s.half_extents, s.radius};
            } else {
                const std::array<Vector3, 3> vertices{point(s.vertices[0]), point(s.vertices[1]),
                                                      point(s.vertices[2])};
                return triangle_from_volume(vertices, s.fatness, {-1.0f, -1.0f, -1.0f}, 0);
            }
        },
        shape.value);
}

std::pair<Vec3, float> bounds(const ContactPrimitive& p) {
    return std::visit(
        [&](const auto& s) -> std::pair<Vec3, float> {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, Sphere>) {
                return {Vec3::from_array(xyz(s.center)), s.radius};
            } else if constexpr (std::is_same_v<T, ContactPrimitive::Capsule>) {
                return {Vec3::from_array(xyz(s.center)), s.half_length + s.radius};
            } else if constexpr (std::is_same_v<T, ContactPrimitive::RoundedBox>) {
                return {Vec3::from_array(xyz(s.center)), glam::length(Vec3::from_array(xyz(s.half_extents))) + s.radius};
            } else {
                std::array<Vec3, 3> vertices{};
                for (std::size_t i = 0; i < 3; ++i) vertices[i] = Vec3::from_array(xyz(s.vertices[i]));
                const Vec3 center = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;
                float r = 0.0f;
                for (const auto& v : vertices) r = rs::max(r, glam::distance(v, center));
                return {center, r + s.fatness};
            }
        },
        p.value);
}

void Proxies::append(const BodyState& frame, const Schema& schema, const GamePhysics& physics,
                     const SkaterRuntime& skater, float age) {
    std::vector<const BodySnapshot*> locals;
    for (const auto& b : physics.board.bodies()) locals.push_back(&b);
    for (const auto& b : skater.skeleton.bodies()) locals.push_back(&b);
    // Include detached boards as well as the rider. Distant bodies never enter the solver.
    const bool nearby = std::any_of(frame.bodies.begin(), frame.bodies.end(), [&](const Body& b) {
        return std::any_of(locals.begin(), locals.end(), [&](const BodySnapshot* local) {
            return glam::distance_squared(Vec3::from_array(b.pose.p), Vec3::from_array(xyz(local->rates.position))) <
                   64.0f;
        });
    });
    if (!nearby) return;
    const std::size_t base =
        skater.skeleton.bodies().size() + skater.skeleton_drives.targets.bodies.size() + bodies.size();
    const std::size_t body_start = bodies.size();
    const std::size_t n = std::min(locals.size(), frame.bodies.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& wire = frame.bodies[i];
        BodySnapshot b = *locals[i];
        if (i < 7) {
            b.inertia = physics.settings.masses[i].dynamics;
        } else {
            const auto part = skater.skeleton.definition.parts[i - 7];
            const bool ragdoll = (frame.enabled & (std::uint64_t(1) << 63)) != 0 && (i - 7) >= 1 && (i - 7) < 24;
            auto inertia = ragdoll ? part.ragdoll.dynamics : part.animated.dynamics;
            inertia.inverse_mass = ragdoll ? part.inverse_mass_ragdoll : part.inverse_mass_animated;
            b.inertia = inertia;
        }
        const Quat q = Quat::from_array(wire.pose.q).normalize();
        b.rates.orientation = RetailQuaternion{q.x, q.y, q.z, q.w};
        b.rates.basis = ::s3::math::Basis3{Mat3::from_quat(q).to_cols_array_2d()};
        b.rates.world_inverse_inertia = world_inverse_inertia(b.rates.basis, b.inertia.inverse_tensor);
        b.rates.position = add(vector(wire.pose.p), vector(rs::map(wire.velocity, [&](float v) { return v * age; })));
        b.rates.linear_velocity = vector(wire.velocity);
        b.rates.angular_velocity = vector(wire.angular);
        b.rates.force_acceleration = Vector3::ZERO;
        b.rates.torque_acceleration = Vector3::ZERO;
        b.state_flags = 4;
        bodies.push_back(b);
    }
    for (const auto& [i, tmpl] : schema.volumes) {
        if ((frame.enabled & (std::uint64_t(1) << i)) == 0) continue;
        const BodySnapshot& b = bodies[body_start + i];
        BoardWorldVolume v;
        v.body = CollisionBody::Attached(base + i);
        v.primitive = transform(tmpl.primitive, matrix(body_pose(b)));
        v.linear_velocity = b.rates.linear_velocity;
        v.material = tmpl.material;
        volumes.push_back(v);
    }
}

void Proxies::append_vehicle(const modding::vehicles::network::CollisionShape& shape, const GamePhysics& physics,
                             const SkaterRuntime& skater) {
    const Vec3 p = Vec3::from_array(shape.position);
    bool near = false;
    for (const auto& b : physics.board.bodies())
        near = near || glam::distance_squared(Vec3::from_array(xyz(b.rates.position)), p) < 100.0f;
    for (const auto& b : skater.skeleton.bodies())
        near = near || glam::distance_squared(Vec3::from_array(xyz(b.rates.position)), p) < 100.0f;
    if (!near) return;
    const std::size_t base =
        skater.skeleton.bodies().size() + skater.skeleton_drives.targets.bodies.size() + bodies.size();
    BodySnapshot body = physics.board.bodies()[0];
    const Quat q = Quat::from_array(shape.rotation).normalize();
    body.rates.position = vector(shape.position);
    body.rates.orientation = RetailQuaternion{q.x, q.y, q.z, q.w};
    body.rates.basis = ::s3::math::Basis3{Mat3::from_quat(q).to_cols_array_2d()};
    body.inertia.inverse_mass = 1.0f / shape.mass;
    const auto [x, y, z] = shape.half;
    body.inertia.inverse_tensor = vector({
        3.0f / (shape.mass * (y * y + z * z)),
        3.0f / (shape.mass * (x * x + z * z)),
        3.0f / (shape.mass * (x * x + y * y)),
    });
    body.rates.world_inverse_inertia = world_inverse_inertia(body.rates.basis, body.inertia.inverse_tensor);
    body.rates.linear_velocity = vector(shape.velocity);
    body.rates.angular_velocity = vector(shape.angular);
    body.rates.force_acceleration = Vector3::ZERO;
    body.rates.torque_acceleration = Vector3::ZERO;
    body.state_flags = 4;
    bodies.push_back(body);
    BoardWorldVolume v;
    v.body = CollisionBody::Attached(base);
    v.primitive = ContactPrimitive::RoundedBox{
        vector((p + q * Vec3::from_array(shape.offset)).to_array()),
        body.rates.basis,
        vector(rs::map(shape.half, [&](float h) { return h - shape.rounding; })),
        shape.rounding,
    };
    v.linear_velocity = body.rates.linear_velocity;
    v.material = ::s3::physics::contact::RetailContactMaterial{0.4f, 0.3f, 0.1f};
    volumes.push_back(v);
}

}  // namespace s3::game::physics::network
