// Port of skate-game/src/graph_host/motion_offboard/runout.rs (module `motion_runout`).
//! AddRunoutAttribs: Begin82BBA4A8, Update82BBA770, state82BBA830.
//! End is82B61BB8. Values are sampled once per activation, never on Update.
#pragma once
#include <array>
#include <optional>
#include <string>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_runout {

using s3::data::state_graph_attributes::Attributes;

/// Unit struct `Operation`.
struct Operation {
    static std::optional<Operation> parse(const Attributes& attributes) {
        const std::string* name = attributes.text("name");
        if (name != nullptr && *name == "AddRunoutAttribs") return Operation{};
        return std::nullopt;
    }
};
S3_DERIVE_EQ_EMPTY(Operation)

///Borrowed values from the actual published physical bundle and animation owner.
struct Observation {
    bool offboard_flag_331 = false;
    std::array<float, 4> offboard_velocity_128{};
    std::array<float, 4> reckoning_velocity_16{};
    std::array<float, 4> reckoning_up_96{};
    ///Exact first vector of physical bundle20, loaded at82BBA620.
    std::array<float, 4> skeleton_vector_0{};
    ///Live ISkaterAnim virtual28 result at Begin, not a retained pose mirror.
    bool animation_mirrored = false;
};

struct State {
    float angle_degrees_8 = 0.0f;
    ///Native constructor does not initialize12. None records that fact without
    ///inventing a zero speed if a Begin observation was unavailable.
    std::optional<float> speed_12;

    ///None is permitted only for an actually absent native physical/animation
    ///component. An unavailable implementation must remain an upstream error.
    void begin(std::optional<Observation> observation);

    ///Write to the canonical playback queue in the original order. This does
    ///not modify the captured state. A genuinely absent animation skips Update.
    /// `animation`: nullptr = None; any type with `set_attribute(SettableAttribute)`.
    template <class Sink>
    rs::Result<rs::Unit, const char*> update(Sink* animation) const {
        using s3::animation::playback_parameters::SettableAttribute;
        using s3::animation::skeleton_input::name::encode;
        if (animation == nullptr) return rs::Unit{};
        if (!speed_12) return rs::make_err("AddRunoutAttribs speed12 has no successful Begin observation");
        const float speed = *speed_12;
        const std::pair<const char*, float> values[] = {{"BipedStartAngle", angle_degrees_8}, {"BipedSpeed", speed}};
        for (const auto& [name, value] : values) {
            animation->set_attribute(SettableAttribute{encode(name), value, false, -1});
        }
        return rs::Unit{};
    }
};
S3_DERIVE_EQ(State, angle_degrees_8, speed_12)

}  // namespace s3::game::graph_host::motion_runout
