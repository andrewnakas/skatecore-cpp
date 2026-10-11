// Port of skate-core/src/air/trajectory/grind_surface.rs.
#include "s3/core/air/trajectory/grind_surface.hpp"

#include "s3/core/air/trajectory/grind_surface/classify.hpp"
#include "s3/core/air/trajectory/grind_surface/orientation.hpp"

namespace s3::air::trajectory::grind_surface {

using namespace trajectory::math;

GrindSurface GrindSurface::not_submitted(InvestigationInput input) {
    const float distance = input.deck_center_to_truck;
    const V x{1.0f, 0.0f, 0.0f, 0.0f};
    const V z{0.0f, 0.0f, 1.0f, 0.0f};
    GrindSurface s;
    s.center = input.reference;
    s.far_points = {madd(x, distance, input.reference), sub(input.reference, scale(x, distance))};
    s.upmost_normal = z;
    s.direction = z;
    s.high_side = z;
    s.normal_limits = {0.0f, 0.0f};
    s.kind = GeometryType::Impossible;
    s.audio_surface = 3;
    s.physics_surface = 1;
    s.flags = INVALID;
    s.tilted_upmost_normal = orientation::tilted_normal(z, z, {0.0f, 0.0f});
    return s;
}

std::optional<Investigation> prepare(InvestigationInput input) {
    const V delta = sub(input.end, input.start);
    const V raw_up = cross(delta, cross(UP, delta));
    const float up_length = length(raw_up);
    auto retain_normalized = [](V v) {
        const float square = dot(v, v);
        const float inverse = inverse_length(square);
        const float magnitude = square == 0.0f ? 0.0f : square * inverse;
        if (magnitude > rs::from_bits(0x358637bdu)) {
            return scale(v, inverse);
        }
        return v;
    };
    const V up = retain_normalized(raw_up);
    const V direction = retain_normalized(delta);
    if (up_length < rs::from_bits(0x3727c5acu)) {
        return std::nullopt;
    }
    const V projected = madd(direction, dot(sub(input.reference, input.start), direction), input.start);
    const V center = sub(input.reference, sub(input.reference, projected));
    const V side = cross(up, direction);
    const V short_up = scale(up, rs::from_bits(0x3d23d70au));
    const V near_side = scale(side, rs::from_bits(0x3db851ecu));
    const V far_side = scale(side, input.deck_center_to_truck);
    const V far_up = scale(up, input.deck_center_to_truck * rs::from_bits(0x3f87ae14u));
    const V raised = scale(up, rs::from_bits(0x3db851ecu));
    auto line = [](V at, V half, float radius) { return Probe{add(at, half), sub(at, half), radius}; };
    std::vector<Probe> probes = {
        line(add(center, near_side), short_up, 0.0f),
        line(sub(center, near_side), short_up, 0.0f),
        line(add(center, far_side), far_up, 0.0f),
        line(sub(center, far_side), far_up, 0.0f),
        line(center, short_up, 0.0f),
        line(add(center, raised), near_side, rs::from_bits(0x3a83126fu)),
    };
    if (input.optional_probe) {
        const V start = *input.optional_probe;
        probes.push_back(Probe{start, add(start, V{0.0f, -6.0f, 0.0f, 0.0f}), 0.0f});
    }
    return Investigation{center, up, direction, probes};
}

GrindSurface resolve(const Investigation& plan, const std::array<std::optional<ProbeHit>, 7>& hits,
                     std::uint32_t previous_flags) {
    return classify::resolve(plan, hits, previous_flags);
}

}  // namespace s3::air::trajectory::grind_surface
