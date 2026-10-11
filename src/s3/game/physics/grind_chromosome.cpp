// Port of skate-game/src/physics/grind_chromosome.rs.
#include "s3/game/physics/grind_chromosome.hpp"

#include <cstring>

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::physics::grind_chromosome {

using pose::add;
using pose::dot;
using pose::signed_;
using pose::sub;

names::Name Components::name() const {
    // Constructed only from booleans, orientation0..3 and the Family enum.
    const auto name = names::lookup(_0);
    if (!name) S3_PANIC("validated grind chromosome dimensions");
    return *name;
}

///82DEE508's actual PhysOut publication, after update and only while grinding.
/// FastString encoding uses CURRENT's existing original-name encoder.
void publish(const Publication& p, ::s3::player::input_phase::grind_output::GrindOutputFields& out) {
    using ::s3::animation::skeleton_input::name::encode;
    out.volatile_chromosome_244 = p.volatile_._0;
    if (p.animation) {
        const auto name = p.animation->name();
        out.animation_chromosome_268 = p.animation->_0;
        out.animation_id_144 = static_cast<std::uint32_t>(name.skating_id);
        out.animation_name_156 = encode(std::string_view(name.attribute, std::strlen(name.attribute)));
    }
    if (p.scoring) {
        const auto name = p.scoring->name();
        out.scoring_chromosome_292 = p.scoring->_0;
        out.scoring_id_148 = static_cast<std::uint32_t>(name.skating_id);
        out.scorable_id_152 = static_cast<std::uint32_t>(name.scorable_id);
        out.scoring_name_176 = encode(std::string_view(name.attribute, std::strlen(name.attribute)));
    }
}

///82DEE358 leaves byte144 unwritten. The false storage below is opaque
///until the host observes the first completed animation packet.
Chromosome Chromosome::uninitialized() {
    Chromosome state = new_(false);
    state.saved_fakie_initialized = false;
    return state;
}

///Explicit host initialization of a native unwritten byte, not a claim
///about retail allocator contents. Seed once from completed Anim10372;
///do not replace constructor axes/position or any established history.
void Chromosome::initialize_host_fakie(bool observed_fakie) {
    if (!saved_fakie_initialized) {
        saved.fakie = observed_fakie;
        saved_fakie_initialized = true;
    }
}

Chromosome Chromosome::new_(bool constructor_fakie_144) {
    Chromosome c;
    c.history.clear();
    c.saved = ApproachPose{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {{{1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}}},
        constructor_fakie_144,
    };
    c.saved_fakie_initialized = true;
    c.approach = 0;
    c.previous_category = 0;
    c.previous_kind = std::nullopt;
    c.away_frames = 31;
    c.reversed = false;
    c.orientation = std::nullopt;  // Native sentinel4, distinct from forward0.
    c.pending = std::nullopt;
    c.pending_frames = 0;
    c.animation = std::nullopt;
    c.scoring = std::nullopt;
    return c;
}

ApproachPose Chromosome::reference() const {
    //82DEED28: fewer than30 samples uses saved, NOT newest ground pose.
    if (history.size() == 30) {
        return history[0];
    }
    return saved;
}

std::optional<Publication> Chromosome::update(const Input& input) {
    const ApproachPose snapshot{input.basic_right_0, input.basic_position_48, input.feet_256_272, input.fakie_155};
    //82DEEB90/82DEEAF8, in this order even if both conditions hold.
    if (input.air_event_439) {
        saved = snapshot;
        saved_fakie_initialized = true;
        history.clear();
    }
    if (input.category == 100) {
        if (history.size() == 30) {
            history.pop_front();
        }
        history.push_back(snapshot);
    }
    const bool new_approach = input.category == 400 && previous_category != 400 && away_frames > 30;
    if (new_approach || (input.category == 400 && previous_kind != input.family)) {
        saved = reference();
        saved_fakie_initialized |= history.size() == 30;
    }
    if (new_approach) {
        const auto across = signed_(input.across, dot(input.across, sub(saved.position, input.point)) > 0.0f);
        const auto right = signed_(saved.right, dot(add(saved.feet[0], saved.feet[1]), saved.right) > 0.0f);
        approach = dot(right, across) > 0.0f ? 1u : 0u;
    }
    previous_kind = input.family;
    away_frames = input.category == 400 ? 0 : static_cast<std::int32_t>(static_cast<std::uint32_t>(away_frames) + 1u);
    std::optional<Publication> output;
    if (input.family && input.grinding_316) {
        const Family family = *input.family;
        //82DEF518: orientation, tilt, then twist, using actual Basic output
        //lanes rather than substituting the live board's current transform.
        const std::uint32_t orientation_value = travel(input, family);
        const bool low_ = pose::low(input);
        const bool straight_ = pose::straight(input);
        const bool location =
            (family == Family::Tipslide || family == Family::FiveO || family == Family::Backslash) &&
            dot(sub(input.point, input.basic_location_position_144), input.basic_location_axis_96) <= 0.0f;
        const Components components{{
            approach,
            location ? 1u : 0u,
            straight_ ? 1u : 0u,
            low_ ? 1u : 0u,
            orientation_value,
            static_cast<std::uint32_t>(family),
        }};
        if (previous_category != 400) {
            pending = components;
            pending_frames = 13;
        } else if (pending == components) {
            pending_frames = static_cast<std::int32_t>(static_cast<std::uint32_t>(pending_frames) + 1u);
        } else {
            pending = components;
            pending_frames = 0;
        }
        if (pending_frames > 0) {
            animation = pending;
        }
        if (pending_frames > 12) {
            scoring = pending;
        }
        output = Publication{components, animation, scoring};
    } else {
        //82DEE508 tail, independent of the category.
        orientation = std::nullopt;
        reversed = false;
    }
    previous_category = input.category;
    return output;
}

std::uint32_t Chromosome::travel(const Input& input, Family family) {
    const auto feet = add(input.feet_256_272[0], input.feet_256_272[1]);
    const auto right = signed_(input.basic_right_0, dot(feet, input.basic_right_0) > 0.0f);
    const bool forward = dot(right, input.direction) > 0.0f;
    std::uint32_t result;
    if (family == Family::FiftyFifty || family == Family::FiveO) {
        if (!orientation) {
            if (!(history.size() == 30 || saved_fakie_initialized)) {
                S3_PANIC("saved fakie144 consumed before an actual history write");
            }
            const ApproachPose ref = reference();
            reversed = dot(feet, add(ref.feet[0], ref.feet[1])) < 0.0f;
            if (ref.fakie) {
                reversed = !reversed;
            }
        }
        if (forward && !reversed) result = 0;
        else if (!forward && !reversed) result = 1;
        else if (forward && reversed) result = 2;
        else result = 3;
    } else {
        result = forward ? 0u : 1u;
    }
    orientation = result;
    return result;
}

}  // namespace s3::game::physics::grind_chromosome
