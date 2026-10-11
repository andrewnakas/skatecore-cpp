// Port of skate-core/src/physics/board_step.rs.
#include "s3/core/physics/board_step.hpp"

#include <sstream>

#include "s3/core/physics/point_force.hpp"

namespace s3::physics::board_step {

namespace {

void apply_deck_forces(std::array<BodySnapshot, BODY_COUNT>& bodies, const BoardForceQueue& forces,
                       float y_offset) {
    BodySnapshot& deck = bodies[board::index(BodyId::Deck)];
    const point_force::RetailForceAccumulator result = forces.apply_to_deck(
        point_force::RetailForceAccumulator{deck.rates.force_acceleration, deck.rates.torque_acceleration,
                                            deck.rates.cool_down},
        deck.rates.basis, deck.inertia.inverse_mass, deck.rates.world_inverse_inertia, y_offset);
    deck.rates.force_acceleration = result.force_acceleration;
    deck.rates.torque_acceleration = result.torque_acceleration;
    deck.rates.cool_down = result.cool_down;
}

contact::RetailContactBodyState contact_body(const CollisionBody& body,
                                             const std::array<BodySnapshot, BODY_COUNT>& bodies,
                                             const std::vector<BodySnapshot*>& attached,
                                             std::size_t world_reaction) {
    if (body == CollisionBody::StaticWorld()) {
        contact::RetailContactBodyState world{};
        world.contact_body_id = 0xFFFFFFFFu;
        world.reaction_id = static_cast<std::uint32_t>(world_reaction);
        world.center_of_mass = Vector3::ZERO;
        world.inverse_inertia_full = Vector3::ZERO;
        world.inverse_inertia_split = Vector3::ZERO;
        world.inverse_mass = 0.0f;
        world.state = 0;
        world.force_acceleration = Vector3::ZERO;
        world.torque_acceleration = Vector3::ZERO;
        world.linear_velocity = Vector3::ZERO;
        world.angular_velocity = Vector3::ZERO;
        world.kinetic_energy = 0.0f;
        world.cool_down = 0;
        return world;
    }
    const std::uint32_t contact_id = body.contact_id();
    const BodySnapshot* source = nullptr;
    switch (body.kind) {
        case CollisionBody::Kind::Board:
            source = &bodies[board::index(body.board)];
            break;
        case CollisionBody::Kind::Attached:
            source = attached.at(body.attached);
            break;
        case CollisionBody::Kind::StaticWorld:
            S3_PANIC("internal error: entered unreachable code");
    }
    const rigid_body::RetailBodyRates rates = source->rates;
    const rigid_body::RetailPackedWorldInverseInertia inertia =
        rigid_body::pack_world_inverse_inertia(rates.world_inverse_inertia);
    contact::RetailContactBodyState out{};
    out.contact_body_id = contact_id;
    out.reaction_id = contact_id;
    out.center_of_mass = rates.position;
    out.inverse_inertia_full = inertia.full;
    out.inverse_inertia_split = inertia.split;
    out.inverse_mass = source->inertia.inverse_mass;
    // The physical assemblies subscribe to post-solver feedback. Bit 8
    // requests contact observations; it does not alter dynamic eligibility.
    out.state = source->state_flags | 8u;
    out.force_acceleration = rates.force_acceleration;
    out.torque_acceleration = rates.torque_acceleration;
    out.linear_velocity = rates.linear_velocity;
    out.angular_velocity = rates.angular_velocity;
    out.kinetic_energy = rates.kinetic_energy;
    out.cool_down = rates.cool_down;
    return out;
}

void show(std::ostream& os, const Vector3& v) { os << "(" << v.x << ", " << v.y << ", " << v.z << ")"; }
void show(std::ostream& os, const BodySnapshot& b) {
    os << "BodySnapshot { state_flags: " << b.state_flags << ", position: ";
    show(os, b.rates.position);
    os << ", linear_velocity: ";
    show(os, b.rates.linear_velocity);
    os << ", angular_velocity: ";
    show(os, b.rates.angular_velocity);
    os << " }";
}

}  // namespace

void BoardStep::advance(std::array<BodySnapshot, BODY_COUNT>& bodies, BoardHook& hook,
                        const BoardForceQueue& forces, Slice<const BoardCollision> collisions,
                        std::array<float, 2> truck_targets, BoardStepSettings settings) {
    advance_attached(bodies, hook, forces, collisions, truck_targets, settings, AttachedStep{});
}

void BoardStep::advance_attached(std::array<BodySnapshot, BODY_COUNT>& bodies, BoardHook& hook,
                                 const BoardForceQueue& forces, Slice<const BoardCollision> collisions,
                                 std::array<float, 2> truck_targets, BoardStepSettings settings,
                                 AttachedStep attached) {
    // StartFrame writes both timing fields (82DC2FB8). Use one dt for row
    // construction and integration, and derive the reciprocal from it.
    RetailSimulationStep simulation = settings.simulation;
    S3_ASSERT(rs::is_finite(simulation.time_step) && simulation.time_step > 0.0f);
    simulation.frequency = 1.0f / simulation.time_step;
    for (const BoardCollision& collision : collisions) {
        S3_ASSERT(collision.body_a != collision.body_b);
    }

    apply_deck_forces(bodies, forces, settings.force_point_y_offset);
    const auto frames = assembly::prepare_drive_frames(settings.base_truck_transforms, truck_targets, hook);
    const std::size_t world_reaction = attached.world_reaction();
    reactions.resize(world_reaction + 1, RetailReactionCorrections{});
    std::fill(reactions.begin(), reactions.end(), RetailReactionCorrections{});
    contacts.clear();
    reports.clear();
    bool all_inactive = true;
    for (const BodySnapshot& body : bodies) all_inactive = all_inactive && (body.state_flags & 4u) == 0;
    all_inactive = all_inactive && (hook.body.state_flags & 4u) == 0;
    for (const BodySnapshot* body : attached.bodies) all_inactive = all_inactive && (body->state_flags & 4u) == 0;
    if (all_inactive) {
        return;
    }
    for (const BoardCollision& collision : collisions) {
        // Copy force/torque workspaces AFTER force accumulation. Otherwise
        // ContactBatchBuild cannot oppose this frame's applied forces.
        const contact::RetailContact contact =
            contact::generate_contact(collision.contact,
                                      contact_body(collision.body_a, bodies, attached.bodies, world_reaction),
                                      contact_body(collision.body_b, bodies, attached.bodies, world_reaction));
        contacts.push_back(contact_solver::build_contact_jacobian(contact, simulation.time_step));
    }
    assembly::BoardConstraints constraints =
        assembly::BoardConstraints::build(bodies, hook, frames, settings.truck_dynamics, simulation.time_step);
    const std::size_t board_contacts = contacts.size();
    const std::size_t board_joints = constraints.joints.size();
    const std::size_t board_drives = constraints.drives.size();
    contacts.insert(contacts.end(), attached.contacts.begin(), attached.contacts.end());
    constraints.joints.insert(constraints.joints.end(), attached.joints.begin(), attached.joints.end());
    constraints.drives.insert(constraints.drives.end(), attached.drives.begin(), attached.drives.end());
    solver::solve_constraints(contacts, constraints.joints, constraints.drives, reactions, settings.iterations);
    diagnostic_tick = diagnostic_tick + 1u;
    if (diagnostic_capture && diagnostic_tick % 6 == 0) {
        // C++: approximates the Rust `{:?}` dump (format is diagnostic-only).
        const std::size_t deck = board::index(BodyId::Deck);
        std::ostringstream os;
        os << "tick=" << diagnostic_tick << " body=";
        show(os, bodies[deck]);
        os << " hook=";
        show(os, hook.body);
        os << " reaction=";
        show(os, reactions[deck].linear_displacement);
        show(os, reactions[deck].position_displacement);
        show(os, reactions[deck].angular_displacement);
        show(os, reactions[deck].orientation_displacement);
        os << " joints=[";
        for (std::size_t i = 0; i < constraints.joints.size(); ++i) {
            const JointConstraint& j = constraints.joints[i];
            if (j.reaction_a == deck || j.reaction_b == deck) {
                os << "(" << i << ", " << j.reaction_a << ", " << j.reaction_b << ")";
            }
        }
        os << "] drives=[";
        for (std::size_t i = 0; i < constraints.drives.size(); ++i) {
            const RetailDriveRows& d = constraints.drives[i];
            if (d.frame_a_body.reaction_index == deck || d.frame_b_body.reaction_index == deck) {
                os << "(" << i << ")";
            }
        }
        os << "]";
        diagnostic_snapshot = os.str();
    }
    // Every solver family finishes before ANY body integrates. The native
    // job tree places BatchIntegrator after all island solver jobs.
    std::vector<BodySnapshot*> integrate;
    integrate.reserve(BODY_COUNT + 1 + attached.bodies.size());
    for (BodySnapshot& body : bodies) integrate.push_back(&body);
    integrate.push_back(&hook.body);
    for (BodySnapshot* body : attached.bodies) integrate.push_back(body);
    const std::size_t pairs = std::min(integrate.size(), reactions.size());
    for (std::size_t i = 0; i < pairs; ++i) {
        BodySnapshot& body = *integrate[i];
        RetailReactionCorrections& reaction = reactions[i];
        if ((body.state_flags & 4u) != 0) {
            body.rates = rigid_body::integrate_body_rates(body.rates, body.inertia, simulation, reaction).state;
        }
        // DynamicUpdate clears all four working vectors, not just the
        // velocity-producing pair. Accumulated rows are rebuilt next tick.
        reaction = RetailReactionCorrections{};
    }
    // ContactSpiesJob follows integration (82DC35E0). Step_Solver3 then
    // publishes reports before the physical Adjust/Output phases.
    contact_feedback::board_reports::collect(reports, contacts, bodies, simulation.frequency);
    // Physical output/spy consumers observe the rows from the shared solve.
    S3_ASSERT(attached.contacts.size() == contacts.size() - board_contacts);
    for (std::size_t i = 0; i < attached.contacts.size(); ++i) attached.contacts[i] = contacts[board_contacts + i];
    S3_ASSERT(attached.joints.size() == constraints.joints.size() - board_joints);
    for (std::size_t i = 0; i < attached.joints.size(); ++i) {
        attached.joints[i] = constraints.joints[board_joints + i];
    }
    S3_ASSERT(attached.drives.size() == constraints.drives.size() - board_drives);
    for (std::size_t i = 0; i < attached.drives.size(); ++i) {
        attached.drives[i] = constraints.drives[board_drives + i];
    }
}

}  // namespace s3::physics::board_step
