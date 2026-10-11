// Port of skate-game/src/physics/solve/diagnostics.rs.
#include "s3/game/physics/solve/diagnostics.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::solve::diagnostics {

using sc::physics::assembly::BodySnapshot;

std::vector<BodySnapshot> snapshot(const GamePhysics& physics, const SkaterRuntime& skater) {
    std::vector<BodySnapshot> out;
    for (const auto& b : physics.board.bodies()) out.push_back(b);
    out.push_back(physics.board.hook().body);
    for (const auto& b : skater.skeleton.bodies()) out.push_back(b);
    for (const auto& b : skater.skeleton_drives.targets.bodies) out.push_back(b);
    return out;
}

Status validate(const std::vector<BodySnapshot>& bodies, const char* stage) {
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const auto& r = bodies[index].rates;
        const std::pair<const char*, sc::math::Vector3> fields[] = {
            {"position", r.position},
            {"linear_velocity", r.linear_velocity},
            {"angular_velocity", r.angular_velocity},
            {"force_acceleration", r.force_acceleration},
            {"torque_acceleration", r.torque_acceleration},
        };
        for (const auto& [name, v] : fields) {
            if (!(rs::is_finite(v.x) && rs::is_finite(v.y) && rs::is_finite(v.z))) {
                return rs::make_err(fmt::cat("Non-finite ", name, " ", stage, "; reaction_body=", index));
            }
        }
        const auto q = r.orientation;
        bool finite = rs::is_finite(q.x) && rs::is_finite(q.y) && rs::is_finite(q.z) && rs::is_finite(q.w);
        for (const auto& c : r.basis.columns)
            for (float v : c) finite = finite && rs::is_finite(v);
        for (const auto& c : r.world_inverse_inertia.columns)
            for (float v : c) finite = finite && rs::is_finite(v);
        if (!finite) {
            return rs::make_err(fmt::cat("Non-finite orientation/inertia ", stage, "; reaction_body=", index));
        }
    }
    return ok();
}

}  // namespace s3::game::physics::solve::diagnostics
