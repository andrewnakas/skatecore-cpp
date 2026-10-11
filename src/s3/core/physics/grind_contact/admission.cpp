// Port of skate-core/src/physics/grind_contact/admission.rs.
#include "s3/core/physics/grind_contact/admission.hpp"

#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/physics/grind_contact/arithmetic.hpp"

namespace s3::physics::grind_contact::admission {

namespace {
float slope_sine(V tangent, V velocity, float dead_zone) {
    const V transverse = sub(velocity, scale(tangent, dot3(velocity, tangent)));
    const float length = arithmetic::square_root(dot3(transverse, transverse));
    if (length <= dead_zone) {
        return 0.0f;
    }
    return arithmetic::reciprocal(length) * transverse[1];
}
}  // namespace

Decision Admission::test(std::uint32_t requested_state, V tangent, bool allows_drop_in) const {
    EntryKind kind;
    if (category == 400 || state == 701) {
        if (state == requested_state || (requested_state == 402 && state == 404)) {
            kind = EntryKind::StayInGrind;
        } else {
            kind = EntryKind::ChangeGrind;
        }
    } else if (category == 100) {
        if (allows_drop_in && speed < 1.45f) {
            kind = EntryKind::DropIn;
        } else {
            const float slope = approach_slope_sine(tangent, velocity);
            if (slope > 0.96f) {
                kind = EntryKind::RideIntoCoping;
            } else if (slope > 0.25f) {
                kind = EntryKind::RideFromBelow;
            } else {
                kind = EntryKind::RideFromAbove;
            }
        }
    } else {
        kind = EntryKind::AirToGrind;
    }
    bool allowed;
    switch (kind) {
        case EntryKind::RideFromAbove:
            allowed = within_approach_angle(tangent, velocity, 17.0f);
            break;
        case EntryKind::StayInGrind:
            allowed = within_approach_angle(tangent, velocity, 90.0f);
            break;
        case EntryKind::ChangeGrind:
            allowed = within_approach_angle(tangent, velocity, 24.0f);
            break;
        case EntryKind::AirToGrind:
        case EntryKind::DropIn:
            allowed = true;
            break;
        case EntryKind::RideFromBelow:
        case EntryKind::RideIntoCoping:
        default: {
            const float square_speed = dot3(velocity, velocity);
            const V transverse = sub(scale(tangent, dot3(velocity, tangent)), velocity);
            const float square_transverse = dot3(transverse, transverse);
            const float ratio =
                square_speed <= 0.002f ? 1.0f : arithmetic::square_root(square_transverse / square_speed);
            const float slope = approach_slope_sine(tangent, velocity);
            const float multiplier = threshold_vs_slope->evaluate(slope);
            const float limit = rs::mul_add(1.0f - ratio, 2.4f, 2.8f) * multiplier;
            allowed = square_transverse < limit * limit;
            break;
        }
    }
    return Decision{kind, allowed};
}

float approach_slope_sine(V tangent, V velocity) { return slope_sine(tangent, velocity, 0.01f); }

float engagement_slope_sine(V tangent, V velocity) { return slope_sine(tangent, velocity, 0.001f); }

}  // namespace s3::physics::grind_contact::admission
