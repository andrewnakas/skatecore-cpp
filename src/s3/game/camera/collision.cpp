// Port of simref sim/src/camera/collision.rs.
#include "s3/game/camera/collision.hpp"

#include "s3/game/camera/world_query.hpp"

namespace s3::game::camera::collision {

using s3::camera::FatLine;
using s3::camera::FatLineResult;

namespace {
// 82DF3290 initializes both vectors, fraction and surface to zero on a miss.
FatLineResult no_hit() { return FatLineResult{}; }
}  // namespace

CameraCollision CameraCollision::new_(const s3::physics::board_world::BoardWorld& world) {
    CameraCollision c;
    c.world = &world;
    c.results_.fill(no_hit());
    c.error = std::nullopt;
    return c;
}

rs::Result<std::array<FatLineResult, 10>, std::string> CameraCollision::query(std::array<FatLine, 10> lines,
                                                                              std::uint32_t /*context*/) {
    std::array<FatLineResult, 10> output;
    output.fill(no_hit());
    for (std::size_t i = 0; i < 10; ++i) {
        S3_CAM_TRY(output[i], world_query::line(*this->world, lines[i].start, lines[i].end, lines[i].radius));
    }
    return output;
}

void CameraCollision::submit(std::array<FatLine, 6> lines, std::uint32_t /*context*/) {
    // Context excludes the subject actor from native world queries. The
    // clone's BoardWorld contains world triangles only, never actor bodies.
    this->results_.fill(no_hit());
    for (std::size_t i = 0; i < 6; ++i) {
        auto hit = world_query::line(*this->world, lines[i].start, lines[i].end, lines[i].radius);
        if (hit.is_ok()) {
            this->results_[i] = hit.unwrap();
        } else {
            this->error = hit.unwrap_err();
        }
    }
}

std::array<FatLineResult, 6> CameraCollision::results() { return this->results_; }

}  // namespace s3::game::camera::collision
