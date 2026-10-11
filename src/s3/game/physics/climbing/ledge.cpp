// Port of skate-game/src/physics/climbing/ledge.rs
//! Static collision probes for the first authored jump/hang/mantle set.
#include "s3/game/physics/climbing/ledge.hpp"

#include "s3/game/glam_ext.hpp"

namespace s3::game::physics::climbing::ledge {

using glam::VEC3_Y;
using glam::VEC3_ZERO;

::s3::math::Vector3 v(Vec3 p) { return ::s3::math::Vector3::new_(p.x, p.y, p.z); }

std::optional<std::pair<Vec3, Vec3>> hit(const BoardWorld& world, Vec3 a, Vec3 b, float radius) {
    auto r = world.query_swept_line(v(a), v(b), radius);
    if (r.is_err()) return std::nullopt;
    const auto& opt = r.unwrap();
    if (!opt) return std::nullopt;
    const auto h = opt->geometry;
    return std::make_pair(Vec3::new_(h.position.x, h.position.y, h.position.z),
                          Vec3::new_(h.normal.x, h.normal.y, h.normal.z));
}

std::optional<Ledge> find(const BoardWorld& world, Vec3 feet, Vec3 facing) {
    return find_range(world, feet, facing, 1.90f, 0.95f);
}

std::optional<Ledge> find_air(const BoardWorld& world, Vec3 feet, Vec3 facing) {
    return find_range(world, feet, facing, 1.25f, 2.6f);
}

std::optional<Ledge> find_range(const BoardWorld& world, Vec3 feet, Vec3 facing_, float minimum, float distance) {
    const auto facing_opt = glam::try_normalize(Vec3::new_(facing_.x, 0.0f, facing_.z));
    if (!facing_opt) return std::nullopt;
    const Vec3 facing = *facing_opt;
    const Vec3 chest = feet + VEC3_Y * 1.15f;
    const auto wall_hit = hit(world, chest, chest + facing * distance, 0.0f);
    if (!wall_hit) return std::nullopt;
    const auto [wall, normal] = *wall_hit;
    if (rs::abs(normal.y) > 0.2f || normal.dot(facing) > -0.65f) return std::nullopt;
    const Vec3 forward = -glam::normalize(Vec3::new_(normal.x, 0.0f, normal.z));
    const Vec3 right = glam::cross(VEC3_Y, forward);
    const Vec3 probe = wall + forward * 0.12f;
    const auto top_hit = hit(world, Vec3::new_(probe.x, feet.y + 2.75f, probe.z),
                             Vec3::new_(probe.x, feet.y + minimum, probe.z), 0.0f);
    if (!top_hit) return std::nullopt;
    const auto [top, up] = *top_hit;
    if (up.y < 0.95f) return std::nullopt;
    const Vec3 anchor = Vec3::new_(wall.x, top.y + 0.025f, wall.z) + forward * 0.055f;
    // Both hands need the same ledge, not the tip of a thin pole or a corner.
    std::array<Vec3, 2> palms{VEC3_ZERO, VEC3_ZERO};
    std::array<Vec3, 2> normals{VEC3_Y, VEC3_Y};
    const std::array<float, 2> sides{0.30f, -0.30f};
    for (std::size_t i = 0; i < 2; ++i) {
        const float side = sides[i];
        const Vec3 p0 = wall + forward * 0.08f + right * side;
        const auto h = hit(world, Vec3::new_(p0.x, top.y + 0.15f, p0.z), Vec3::new_(p0.x, top.y - 0.15f, p0.z), 0.0f);
        if (!h) return std::nullopt;
        const auto [p, n] = *h;
        palms[i] = p + n * 0.005f;
        normals[i] = n;
        if (rs::abs(p.y - top.y) > 0.06f || n.y < 0.95f) return std::nullopt;
    }
    const Ledge ledge{anchor, Vec3::new_(wall.x, top.y + 0.015f, wall.z) + forward * 0.62f, forward, palms, normals};
    if (!clear(world, ledge)) return std::nullopt;
    // Space for the torso outside the face, and for the head as it rises.
    const Vec3 outside = anchor - forward * 0.32f;
    if (hit(world, outside - VEC3_Y * 1.25f, outside + VEC3_Y * 0.55f, 0.20f).has_value()) return std::nullopt;
    return ledge;
}

bool clear(const BoardWorld& world, Ledge ledge) {
    const Vec3 right = glam::cross(VEC3_Y, ledge.forward);
    const std::array<Vec3, 5> offsets{VEC3_ZERO, right * 0.25f, -right * 0.25f, ledge.forward * 0.25f,
                                      -ledge.forward * 0.25f};
    for (const Vec3 offset : offsets) {
        const Vec3 p = ledge.landing + offset;
        const auto g = hit(world, p + VEC3_Y * 0.15f, p - VEC3_Y * 0.15f, 0.0f);
        if (!g) return false;
        const auto [ground, n] = *g;
        if (rs::abs(ground.y - ledge.landing.y) > 0.06f || n.y < 0.95f) return false;
        if (hit(world, p + VEC3_Y * 0.35f, p + VEC3_Y * 1.55f, 0.28f).has_value()) return false;
    }
    return !hit(world, ledge.anchor + VEC3_Y * 0.65f - ledge.forward * 0.32f, ledge.landing + VEC3_Y * 0.65f, 0.22f)
                .has_value();
}

}  // namespace s3::game::physics::climbing::ledge
