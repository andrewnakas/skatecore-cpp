// Port of skate-core/src/physics/grind_air.rs.
#include "s3/core/physics/grind_air.hpp"

namespace s3::physics::grind_air {

void GrindAir::start(Target target_) {
    target = target_;
    offset_delta = ZERO;
    offset = ZERO;
    angle_delta = ZERO;
    angles = ZERO;
    selected_kind = std::nullopt;
}

rs::Result<std::optional<Adjustment>, std::string> GrindAir::update(Input input,
                                                                    const Settings& settings) {
    if (!input.active) {
        return std::optional<Adjustment>{};
    }
    if ((input.flags_2480 & 0x04000000u) != 0 || (input.flags_2472 & 0x8000u) != 0 ||
        (input.flags_2468 & 8u) != 0) {
        offset = ZERO;
        angles = ZERO;
        return std::optional<Adjustment>{};
    }
    if (!target) {
        return rs::make_err(std::string("Active GrindAirAdjust has no selected primitive"));
    }
    const Target target_ = *target;
    const auto validation = settings.validate();
    if (validation.is_err()) {
        return rs::make_err(validation.unwrap_err());
    }
    const V rail = unit(sub(target_.edge.end, target_.edge.start));
    const V normal = unit(cross(rail, cross(input.up, rail)));
    const auto samples = prediction::project(input, settings, rail, normal, headings);
    const auto contacts =
        selection::contacts(samples, settings.frames, target_.edge.start, rail, normal);
    const auto chosen = selection::choose(contacts, headings, settings,
                                          (input.flags_2484 & 0x00200000u) != 0, selected_kind);
    if (!chosen) {
        return std::optional<Adjustment>{};
    }
    const std::size_t kind = *chosen;
    selected_kind = kind;
    const std::size_t probe = targets::CONTACTS[kind];
    const float time = contacts.times[probe];
    // 82D72610, per-frame and total displacement are separately bounded.
    offset_delta =
        limited(add(offset_delta, scale(contacts.corrections[probe], reciprocal(time))), settings.max_delta);
    offset = limited(add(offset, offset_delta), settings.max_offset);
    angles::update(*this, input, settings, target_, samples, rail, normal, kind, time);
    return std::optional<Adjustment>{Adjustment{rail, offset, angles}};
}

}  // namespace s3::physics::grind_air
