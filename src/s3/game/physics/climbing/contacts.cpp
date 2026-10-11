// Port of skate-game/src/physics/climbing/contacts.rs
//! Two-bone IK over the authored pose. Preserve limb lengths and
//! align both palms to fixed surface contacts with independent wrist rotation.
#include "s3/game/physics/climbing/contacts.hpp"

#include <string>

#include "s3/game/glam_ext.hpp"

namespace s3::game::physics::climbing::contacts {

Vec3 clearance(const Clip& clip, const std::vector<Mat4>& globals) {
    const Vec3 hands_ = clip.hands(globals);
    const Vec3 hips = glam::truncate(globals[clip.index("HIPS")].w_axis);
    // The Mixamo freehang arches its hips past its hands. Bring the torso
    // outside a vertical wall and raise it enough to keep the edge in reach.
    const float back = rs::max(hips.z - hands_.z + 0.24f, 0.0f);
    return Vec3::new_(0.0f, back * 0.53f, -back);
}

std::pair<Vec3, Quat> wrist(ledge::Ledge ledge, std::size_t i) {
    const Vec3 normal = ledge.normals[i];
    const Vec3 forward = glam::normalize(ledge.forward - normal * ledge.forward.dot(normal));
    const Quat rotation =
        glam::quat_from_mat3(glam::Mat3::from_cols(forward, -normal, glam::cross(forward, -normal)));
    return {ledge.palms[i] - rotation * PALM, rotation};
}

void hands(const Clip& clip, std::vector<Transform>& locals, Mat4 root, ledge::Ledge ledge, float weight) {
    const auto globals = clip.globals(locals);
    const Mat4 inverse = root.inverse();
    const Quat root_rotation = Transform::from_matrix(root).rotation;
    const std::array<const char*, 2> sides{"LEFT", "RIGHT"};
    for (std::size_t i = 0; i < 2; ++i) {
        const std::string side = sides[i];
        const std::size_t upper = clip.index(side + "ARM");
        const std::size_t lower = clip.index(side + "FOREARM");
        const std::size_t hand = clip.index(side + "HAND");
        const auto [position, rotation0] = wrist(ledge, i);
        const Vec3 target =
            glam::lerp(glam::truncate(globals[hand].w_axis), glam::transform_point3(inverse, position), weight);
        const Quat rotation =
            glam::slerp(Transform::from_matrix(globals[hand]).rotation, root_rotation.inverse() * rotation0, weight);
        solve(clip, locals, upper, lower, hand, target, rotation);
    }
}

void solve(const Clip& clip, std::vector<Transform>& locals, std::size_t upper, std::size_t lower, std::size_t hand,
           Vec3 target, Quat wrist_) {
    auto g = clip.globals(locals);
    const Vec3 a = glam::truncate(g[upper].w_axis);
    Vec3 b = glam::truncate(g[lower].w_axis);
    const Vec3 c = glam::truncate(g[hand].w_axis);
    const float l1 = glam::distance(a, b);
    const float l2 = glam::distance(b, c);
    const auto dir_opt = glam::try_normalize(target - a);
    if (!dir_opt) return;
    const Vec3 dir = *dir_opt;
    const float distance = rs::clamp(glam::distance(a, target), rs::abs(l1 - l2) + 0.0001f, l1 + l2 - 0.0001f);
    const float along = (l1 * l1 - l2 * l2 + distance * distance) / (2.0f * distance);
    const Vec3 bend = (b - a) - dir * (b - a).dot(dir);
    const auto pole_opt = glam::try_normalize(bend);
    const Vec3 pole = pole_opt ? *pole_opt : glam::any_orthonormal_vector(dir);
    const Vec3 elbow = a + dir * along + pole * rs::sqrt(rs::max(l1 * l1 - along * along, 0.0f));
    rotate(clip, locals, g, upper, b - a, elbow - a);
    g = clip.globals(locals);
    b = glam::truncate(g[lower].w_axis);
    rotate(clip, locals, g, lower, glam::truncate(g[hand].w_axis) - b, a + dir * distance - b);
    g = clip.globals(locals);
    const Quat relative = Transform::from_matrix(g[lower]).rotation.inverse() * wrist_;
    // Pronation belongs in the forearm, not a 100+ degree wrist twist.
    // Roll about the actual elbow-to-wrist axis so the contact cannot move.
    const Vec3 axis = glam::normalize(locals[hand].translation);
    const Vec3 projected = axis * Vec3::new_(relative.x, relative.y, relative.z).dot(axis);
    const Quat raw_twist = Quat::from_xyzw(projected.x, projected.y, projected.z, relative.w);
    const Quat twist = glam::length_squared(raw_twist) > 1e-8f ? raw_twist.normalize() : Quat::IDENTITY;
    locals[lower].rotation = (locals[lower].rotation * twist).normalize();
    locals[hand].rotation = (twist.inverse() * relative).normalize();
}

void rotate(const Clip& clip, std::vector<Transform>& locals, const std::vector<Mat4>& globals, std::size_t bone,
            Vec3 from, Vec3 to) {
    const Quat delta = glam::quat_from_rotation_arc(glam::normalize(from), glam::normalize(to));
    const std::size_t parent = std::size_t(clip.parents[bone]);
    locals[bone].rotation = (Transform::from_matrix(globals[parent]).rotation.inverse() * delta *
                             Transform::from_matrix(globals[bone]).rotation)
                                .normalize();
}

}  // namespace s3::game::physics::climbing::contacts
