// Port of skate-core/src/physics/tests/assembly.rs.
#include "harness.hpp"
#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/mass.hpp"

namespace {
using namespace s3::physics::assembly;
using s3::math::Vector3;
using s3::physics::board::BodyId_ORDER;
using s3::physics::board::index;
using s3::physics::rigid_body::RetailReactionCorrections;

std::array<BodySnapshot, BODY_COUNT> bodies() {
    const auto poses = s3::physics::drive_frames::default_live_body_transforms();
    const auto orientations = s3::physics::drive_frames::default_live_body_orientations();
    const auto masses = s3::physics::mass::default_skateboard_mass_properties();
    std::array<BodySnapshot, BODY_COUNT> out{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const auto basis = s3::physics::rigid_body::basis_from_quaternion(orientations[i]);
        out[i].state_flags = 4;
        out[i].inertia = masses[i].dynamics;
        out[i].rates = RetailBodyRates{
            orientations[i],
            basis,
            s3::physics::rigid_body::world_inverse_inertia(basis, masses[i].dynamics.inverse_tensor),
            poses[i].translation,
            Vector3::ZERO,
            Vector3::ZERO,
            Vector3::ZERO,
            Vector3::ZERO,
            0.0f,
            0,
        };
    }
    return out;
}

BoardHook inactive_hook() {
    BodySnapshot body = bodies()[6];
    body.state_flags = 0;
    return BoardHook{body, s3::physics::hook_drive::HookDriveState::initial()};
}

std::array<RetailReactionCorrections, BODY_COUNT> solve(std::array<float, 2> targets) {
    BoardHook hook = inactive_hook();
    const auto frames = prepare_drive_frames(s3::physics::drive_frames::default_truck_transforms(), targets, hook);
    auto constraints = BoardConstraints::build(
        bodies(), hook, frames,
        s3::physics::drive_parameters::retail_truck_drive_dynamics(
            s3::physics::drive_parameters::RetailTruckDriveSettings::STOCK),
        s3::rs::from_bits(0x3C888889u));
    std::array<RetailReactionCorrections, BODY_COUNT + 1> reactions{};
    s3::physics::solver::solve_constraints({}, constraints.joints, constraints.drives, reactions, 25);
    std::array<RetailReactionCorrections, BODY_COUNT> out{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) out[i] = reactions[i];
    return out;
}

S3_TEST(native_pair_order_and_stock_constraint_counts_are_preserved) {
    BoardHook hook = inactive_hook();
    const auto frames =
        prepare_drive_frames(s3::physics::drive_frames::default_truck_transforms(), {0.0f, 0.0f}, hook);
    const auto constraints = BoardConstraints::build(
        bodies(), hook, frames,
        s3::physics::drive_parameters::retail_truck_drive_dynamics(
            s3::physics::drive_parameters::RetailTruckDriveSettings::STOCK),
        s3::rs::from_bits(0x3C888889u));
    using P = std::pair<std::size_t, std::size_t>;
    std::vector<P> pairs;
    for (const auto& j : constraints.joints) pairs.push_back({j.reaction_a, j.reaction_b});
    CHECK(pairs == (std::vector<P>{{4, 6}, {5, 6}, {0, 4}, {1, 4}, {2, 5}, {3, 5}}));
    std::vector<P> drive_pairs;
    for (const auto& d : constraints.drives) {
        drive_pairs.push_back({d.frame_a_body.reaction_index, d.frame_b_body.reaction_index});
    }
    CHECK(drive_pairs == (std::vector<P>{{4, 6}, {5, 6}, {7, 6}}));
}

S3_TEST(steering_reaches_wheels_through_shared_joint_and_drive_reactions) {
    const auto rest = solve({0.0f, 0.0f});
    const auto turn = solve({0.3f, 0.3f});
    for (const auto body : BodyId_ORDER) {
        const auto r = turn[index(body)];
        CHECK_MSG(r.angular_displacement != rest[index(body)].angular_displacement, "body " << index(body));
        for (const Vector3 v : {r.linear_displacement, r.angular_displacement}) {
            CHECK_MSG(s3::rs::is_finite(v.x) && s3::rs::is_finite(v.y) && s3::rs::is_finite(v.z),
                      "body " << index(body));
        }
    }
    // Internal constraints exchange momentum; they do not propel the board.
    const auto snapshots = bodies();
    Vector3 momentum = Vector3::ZERO;
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        const auto& r = turn[i];
        const auto& b = snapshots[i];
        momentum = Vector3{
            momentum.x + r.linear_displacement.x / b.inertia.inverse_mass,
            momentum.y + r.linear_displacement.y / b.inertia.inverse_mass,
            momentum.z + r.linear_displacement.z / b.inertia.inverse_mass,
        };
    }
    CHECK_MSG(s3::rs::abs(momentum.x) < 1e-5f && s3::rs::abs(momentum.y) < 1e-5f && s3::rs::abs(momentum.z) < 1e-5f,
              momentum);
}
}  // namespace
