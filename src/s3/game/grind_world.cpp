// Port of skate-game/src/grind_world.rs.
//! Authored test-course grind paths. Collision/visuals share these dimensions.
//! This asset owner does not claim a nearby spline is an acquired grind.
// SIMREF: bevy removed; GrindGeometryPlugin removed.
#include "s3/game/grind_world.hpp"

#include "s3/game/physics/ground.hpp"

namespace s3::game::grind_world {

GrindGeometry GrindGeometry::for_world(bool test_world) {
    GrindGeometry out;
    if (test_world) {
        const auto r = ::s3::game::grind_world::rails();
        out.rails.assign(r.begin(), r.end());
        out.native_blob = spline_blob();
    }
    return out;
}

std::array<Rail, 7> rails() {
    const float floor = ::s3::game::physics::ground::FLOOR_HEIGHT;
    const float top = ::s3::game::physics::ground::HEIGHT;
    auto v = [](float x, float y, float z) { return Vector3::new_(x, y, z); };
    return {
        Rail{1, "low_flat_rail", v(-7.0f, floor + 0.45f, 7.0f), v(-7.0f, floor + 0.45f, 15.0f), 0.10f, 0.10f},
        Rail{2, "high_flat_rail", v(4.5f, floor + 0.70f, 8.0f), v(4.5f, floor + 0.70f, 16.0f), 0.10f, 0.10f},
        Rail{3, "halfpipe_left_coping", v(7.5f, floor + 2.5f, -6.0f), v(7.5f, floor + 2.5f, 6.0f), 0.08f, 0.08f},
        Rail{4, "halfpipe_right_coping", v(16.5f, floor + 2.5f, -6.0f), v(16.5f, floor + 2.5f, 6.0f), 0.08f,
             0.08f},
        Rail{5, "platform_back_edge", v(-3.0f, top, -4.0f), v(3.0f, top, -4.0f), 0.0f, 0.0f},
        Rail{6, "platform_right_edge", v(3.0f, top, -4.0f), v(3.0f, top, 4.0f), 0.0f, 0.0f},
        Rail{7, "platform_left_edge", v(-3.0f, top, 4.0f), v(-3.0f, top, -4.0f), 0.0f, 0.0f},
    };
}

std::vector<std::array<Vector3, 4>> surfaces() {
    std::vector<std::array<Vector3, 4>> out;
    const float floor = ::s3::game::physics::ground::FLOOR_HEIGHT;
    for (const Rail& rail : rails()) {
        // Ledge paths use the already authored platform collision.
        if (rail.width == 0.0f) {
            continue;
        }
        const float x = rail.start.x;
        const float y = rail.start.y;
        box_faces(out, Vector3::new_(x - rail.width * 0.5f, y - rail.thickness, rail.start.z),
                  Vector3::new_(x + rail.width * 0.5f, y, rail.end.z));
        if (rail.id <= 2) {
            for (const float z : {rail.start.z + 0.6f, rail.end.z - 0.6f}) {
                box_faces(out, Vector3::new_(x - 0.04f, floor, z - 0.04f),
                          Vector3::new_(x + 0.04f, y - rail.thickness, z + 0.04f));
            }
        }
    }
    return out;
}

void box_faces(std::vector<std::array<Vector3, 4>>& out, Vector3 min, Vector3 max) {
    auto v = [](float x, float y, float z) { return Vector3::new_(x, y, z); };
    const std::array<Vector3, 4> top = {
        v(min.x, max.y, min.z),
        v(max.x, max.y, min.z),
        v(max.x, max.y, max.z),
        v(min.x, max.y, max.z),
    };
    out.push_back(top);
    for (std::size_t i = 0; i < 4; ++i) {
        const Vector3 a = top[i];
        const Vector3 b = top[(i + 1) % 4];
        out.push_back({b, a, v(a.x, min.y, a.z), v(b.x, min.y, b.z)});
    }
    out.push_back({
        v(min.x, min.y, max.z),
        v(max.x, min.y, max.z),
        v(max.x, min.y, min.z),
        v(min.x, min.y, min.z),
    });
}

rs::Result<std::vector<std::uint8_t>, std::string> native_blob(const ::s3::data::SkateMap* map) {
    return spline::build(map);
}

std::vector<std::uint8_t> spline_blob() {
    auto r = spline::build_rails(test_rails());
    if (r.is_err()) S3_PANIC("authored course splines");
    return std::move(r.unwrap());
}

std::vector<::s3::data::Rail> test_rails() {
    std::vector<::s3::data::Rail> out;
    for (const Rail& r : rails()) {
        ::s3::data::Rail rail;
        rail.name = r.name;
        rail.points = {{r.start.x, r.start.y, r.start.z}, {r.end.x, r.end.y, r.end.z}};
        rail.closed = false;
        rail.native = std::nullopt;
        out.push_back(std::move(rail));
    }
    return out;
}

}  // namespace s3::game::grind_world
