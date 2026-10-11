// Port of skate-game/src/graph_host/motion_tricks.rs.
//! Original trick leaves. Registration strings, not community symbol guesses,
//! identify SetTrickAttr82BC7848 and ScoringTrick82BCC030.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"

namespace s3::game::graph_host::motion_tricks {

using s3::animation::output::attributes::AttributeName;
using s3::animation::playback_parameters::SettableAttribute;
using s3::data::state_graph_attributes::Attributes;

/// Rust `enum Operation`; each variant is a nested struct held in `value`.
struct Operation {
    struct FootPlantAbsorb {};
    struct HandplantAntic {};
    struct HandplantScore {
        std::string base;
        std::array<std::optional<std::string>, 4> directions;
        std::array<std::string, 2> intents;
    };
    struct Height {
        AttributeName from;
        AttributeName rename;
        std::optional<float> value;
        bool manual = false;
        bool grind = false;
    };
    struct Attribute {
        AttributeName _0;
    };
    struct Scoring {
        AttributeName _0;
    };
    struct MonitorUnderflip {};
    struct Dark {};
    struct WeightOnNose {};

    using Value = std::variant<FootPlantAbsorb, HandplantAntic, HandplantScore, Height, Attribute, Scoring,
                               MonitorUnderflip, Dark, WeightOnNose>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    Operation(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }

    static std::optional<Operation> parse(const Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::FootPlantAbsorb)
S3_DERIVE_EQ_EMPTY(Operation::HandplantAntic)
S3_DERIVE_EQ(Operation::HandplantScore, base, directions, intents)
S3_DERIVE_EQ(Operation::Height, from, rename, value, manual, grind)
S3_DERIVE_EQ(Operation::Attribute, _0)
S3_DERIVE_EQ(Operation::Scoring, _0)
S3_DERIVE_EQ_EMPTY(Operation::MonitorUnderflip)
S3_DERIVE_EQ_EMPTY(Operation::Dark)
S3_DERIVE_EQ_EMPTY(Operation::WeightOnNose)
S3_DERIVE_EQ(Operation, value)

struct Requests {
    bool underflip = false;
    bool dark_catch = false;
};

/// `execute(host: &mut MotionHost, operation, updates, phase)`. Generic over the MotionGraph host
/// (graph_host::motion, ported separately). It uses `host.animation` (MotionAnimation),
/// `host.gameplay_conditions`, `host.score_packet` (`handplant`, `trick_names.first/second`,
/// `flags`), `host.trick_height_settings` (Rust `(bool, bool)`, read with std::get<0/1>),
/// `host.trick_requests` and `host.riding.dark`.
template <class MotionHost>
rs::Result<rs::Unit, std::string> execute(MotionHost& host, Operation operation, std::int32_t& updates,
                                          std::uint8_t phase) {
    using s3::animation::skeleton_input::name::encode;
    if (operation.is<Operation::HandplantAntic>() && phase == 0) {
        //VT82321360 Begin=82BBE5D0; shared clip lengths from82B959E8.
        auto duration = [&](const char* name) -> rs::Result<float, std::string> {
            auto clip = host.animation.metadata().clip(name);
            if (!clip.ok) return rs::make_err(clip.error);
            return (rs::from_bits(clip.value->frames_bits) - 1.0f) /
                   (rs::from_bits(clip.value->base_speed_bits) * rs::from_bits(clip.value->fps_bits));
        };
        const rs::Result<float, std::string> normal = duration("INVERT_HANDPLANT_FS_0_ANTIC");
        if (normal.is_err()) return rs::make_err(normal.unwrap_err());
        const rs::Result<float, std::string> late = duration("INVERT_HANDPLANT_FS_0_ANTIC_LATE");
        if (late.is_err()) return rs::make_err(late.unwrap_err());
        if (!host.gameplay_conditions) return rs::make_err(std::string("Handplant antic requires physical output"));
        const auto& p = *host.gameplay_conditions;
        const float n = normal.unwrap();
        const float l = late.unwrap();
        const float value = rs::clamp((n - (p.handplant_thresholds[2] - p.handplant_time) - l) / (n - l), 0.0f, 1.0f);
        host.animation.set_attribute(SettableAttribute{encode("antic_length"), value, false, -1});
    } else if (const auto* score = std::get_if<Operation::HandplantScore>(&operation.value); score && phase == 1) {
        //82BBF758 uses the same angular windows as ScoringGrabs82BBEF60.
        std::array<float, 2> xy{};
        for (std::size_t i = 0; i < 2; ++i) {
            const std::optional<float> v = host.animation.filtered_intent(score->intents[i]);
            xy[i] = v ? *v : 0.0f;
        }
        std::array<std::optional<std::string_view>, 4> directions;
        for (std::size_t i = 0; i < 4; ++i) {
            if (score->directions[i]) directions[i] = std::string_view(*score->directions[i]);
        }
        const std::string_view name = motion_stock_gameplay::select_grab_score(score->base, directions, xy[0], xy[1]);
        host.score_packet.handplant = std::make_pair(encode(name), xy);
    } else if (operation.is<Operation::FootPlantAbsorb>() && phase == 1) {
        //82BBE4E0 reads the manager's curve duration, Air212.
        if (!host.gameplay_conditions) return rs::make_err(std::string("FootPlantAbsorb requires physical output"));
        const float value = host.gameplay_conditions->footplant_duration;
        host.animation.set_attribute(SettableAttribute{encode("absorblength"), value, false, -1});
    } else if (const auto* height_op = std::get_if<Operation::Height>(&operation.value); height_op && phase == 0) {
        // 82BAEE90: graph intents override animation only on the authored paths.
        const float* gesture_ptr = host.animation.motion_intents.get("GestureSpeed");
        const float gesture = gesture_ptr ? *gesture_ptr : 0.0f;
        float height = 0.0f;
        if (height_op->value) {
            height = *height_op->value;
        } else if (const float* trick_height = host.animation.motion_intents.get("TrickHeight")) {
            height = *trick_height;
        } else {
            auto last = host.animation.last_attribute(height_op->from);
            if (last.is_err()) return rs::make_err(last.unwrap_err());
            const auto& attribute = last.unwrap();
            height = attribute && attribute->payload._0[0] ? rs::from_bits(*attribute->payload._0[0]) : 0.0f;
            if (std::get<0>(host.trick_height_settings)) {
                height = std::get<1>(host.trick_height_settings) ? rs::min(height, rs::max(gesture, 0.0f)) : gesture;
            }
        }
        // Independent packet output carries gesture speed, not selected height.
        host.animation.emit_packet(encode("JumpHeightOverride"),
                                   gesture * (height_op->manual ? 0.27f : height_op->grind ? 1.0f : 0.2f));
        host.animation.set_attribute(SettableAttribute{height_op->rename, height, true, -1});
    } else if (const auto* attribute_op = std::get_if<Operation::Attribute>(&operation.value)) {
        const AttributeName name = encode("Trick");
        auto& values = host.animation.construction_values;
        if (phase == 0) {
            auto entry = std::find_if(values.begin(), values.end(), [&](const auto& e) { return e.first == name; });
            if (entry != values.end()) {
                entry->second = attribute_op->_0;
            } else {
                values.emplace_back(name, attribute_op->_0);
            }
        } else if (phase == 2) {
            auto last = host.animation.last_attribute(encode("defaultcyc"));
            if (last.is_err()) return rs::make_err(last.unwrap_err());
            if (last.unwrap().has_value()) {
                values.erase(std::remove_if(values.begin(), values.end(),
                                            [&](const auto& e) { return e.first == name; }),
                             values.end());
            }
        }
    } else if (const auto* scoring = std::get_if<Operation::Scoring>(&operation.value); scoring && phase == 1) {
        // ISkaterMotionGraph236 ->8258FA20 stores both equal names and bit24.
        host.score_packet.trick_names.first = scoring->_0;
        host.score_packet.trick_names.second = scoring->_0;
        host.score_packet.flags |= 0x01000000u;
    } else if (operation.is<Operation::MonitorUnderflip>()) {
        if (phase == 0) {
            updates = 0;
        } else if (phase == 2) {
            host.trick_requests = Requests{};
        } else {
            updates = static_cast<std::int32_t>(static_cast<std::uint32_t>(updates) + 1u);
            if (updates > 1) {
                const auto& intents = host.animation.motion_intents;
                bool underflip = false;
                for (const char* n : {"U_L_F_Kickflip", "U_L_F_Heelflip", "U_L_B_Kickflip", "U_L_B_Heelflip",
                                      "U_Nollie", "U_Fingerflip", "U_Kickflip", "U_Heelflip", "U_N_Kickflip",
                                      "U_N_Heelflip"}) {
                    if (intents.contains_key(n)) {
                        underflip = true;
                        break;
                    }
                }
                if (underflip) {
                    host.trick_requests.underflip = true;
                } else if (intents.contains_key("DarkCatch")) {
                    host.trick_requests.dark_catch = true;
                }
            }
        }
    } else if (operation.is<Operation::Dark>()) {
        host.riding.dark = phase != 2;
        if (phase == 1) host.animation.emit_packet(encode("IsDark"), 1.0f);
    } else if (operation.is<Operation::WeightOnNose>() && phase != 1) {
        //82BB2670/26D0 -> ISkaterAnim20 ->82B97110, bit28.
        if (!host.animation.skater_animation_flags) {
            return rs::make_err(std::string("UpdateIsWeightOnNose requires live ISkaterAnim"));
        }
        std::uint32_t& flags = *host.animation.skater_animation_flags;
        flags = (flags & ~0x10000000u) | (phase == 0 ? 0x10000000u : 0u);
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_tricks
