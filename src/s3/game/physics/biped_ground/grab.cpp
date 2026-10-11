// Port of skate-game/src/physics/biped_ground/grab.rs
//! Concrete Ground grab decision82D324B0. Query readiness belongs to ONE owner.
#include "s3/game/physics/biped_ground/grab.hpp"

namespace s3::game::physics::biped_ground::grab {

namespace grab_scene = ::s3::player::offboard::grab_scene;
using ::s3::player::offboard::grab_scene::Descriptor;
using ::s3::player::offboard::grab_scene::Query;
using ::s3::player::offboard::ground_sync::board_bounds;

void sync(State& state, Owner& owner, BoardSettings settings, Input p) {
    if ((p.flags_2484 & 0x20000000u) != 0 || (p.flags_2480 & 0x40000u) != 0 || p.timer_2852 > 0.0f) {
        return;
    }
    if ((p.flags_2476 & 0x400000u) == 0) {
        owner.invalidate();
        return;
    }
    if (state.flags_144_to_150[4]) {
        if (!(p.elapsed <= 0.5f)) {
            state.flags_144_to_150[4] = false;
        }
        return;
    }
    const auto bounds = board_bounds(p.frame, settings.offset_32, settings.extent_16);
    if ((owner.flags_12836 & 0x40u) != 0) {
        if (auto candidate = owner.best(p.position)) {
            const float k = rs::from_bits(0x3f733333u);
            const Vector extent{settings.extent_0[0] * k, settings.extent_0[1] * k, settings.extent_0[2] * k,
                                settings.extent_0[3] * k};
            const auto inner = board_bounds(p.frame, settings.offset_32, extent);
            state.flags_144_to_150[0] = grab_scene::qualify(
                *candidate, p.bone, inner, limits(settings.margin_444, settings.angle_452, settings.angle_436));
            if (state.flags_144_to_150[0]) {
                const auto key = candidate->descriptor();
                state.counter_152 = key.kind;
                state.counter_156 = key.id;
            }
        }
    }
    //Query always submits, even when the previous result was rejected.
    Query query{};
    query.position = p.position;
    query.sort_position = p.position;
    query.bounds = bounds;
    query.limits = limits(settings.margin_444, settings.angle_456, settings.angle_440);
    query.mode = 4;
    query.capacity = 5;
    query.context = p.context;
    owner.query(query);
    if (state.flags_144_to_150[0]) {
        //82585DD8 descriptor initialization followed by827602E0 actual data request.
        owner.request_primary(Descriptor{state.counter_152, state.counter_156});
    } else {
        owner.request_interactable(p.frame, p.context);
    }
}

BoardLimits limits(float margin, float a, float b) {
    const float radians = rs::from_bits(0x3c8efa35u);
    BoardLimits out{};
    out.margin = margin;
    out.angle_a = a * radians;
    out.angle_b = b * radians;
    return out;
}

}  // namespace s3::game::physics::biped_ground::grab
