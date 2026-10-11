// Port of simref sim/src/camera/shot_data.rs.
#include "s3/game/camera/shot_data.hpp"

#include <cstdio>
#include <vector>

#include "s3/game/camera/_result.hpp"
#include "s3/game/camera/stock_names.hpp"

namespace s3::game::camera::shot_data {

namespace cam = s3::camera;
using s3::data::Collections;

namespace {

const char* const CLASS = "camera_shots";
constexpr std::uint64_t CLASS_HASH = 0xf27dd93e059ef6cbull;
constexpr float RADIANS = rs::from_bits(0x3c8efa35);

std::string to_ascii_lowercase(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return s;
}

std::string hex16(std::uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof b, "%016llX", (unsigned long long)v);
    return b;
}

rs::Result<bool, std::string> has_field(const Collections& data, const std::string& key, const std::string& name) {
    std::string current = key;
    for (std::size_t step = 0; step <= data.entries().size(); ++step) {
        const s3::data::Collection* entry = nullptr;
        for (const auto& c : data.entries()) {
            if (c.class_name == CLASS && c.key == current) {
                entry = &c;
                break;
            }
        }
        if (entry == nullptr) {
            return rs::make_err("Missing camera collection " + current);
        }
        if (entry->fields.count(name) != 0) {
            return true;
        }
        if (entry->parent.empty()) {
            return false;
        }
        current = entry->parent;
    }
    return rs::make_err("Cyclic camera collection inheritance " + key);
}

}  // namespace

rs::Result<StockShots, std::string> StockShots::from_collections(const Collections& data) {
    std::map<std::uint64_t, std::string> names;
    for (const auto& entry : data.entries()) {
        if (entry.class_name != CLASS) continue;
        S3_CAM_TRY_DATA(const s3::data::Field* text, data.field(CLASS, entry.key, "CollectionName"));
        if (text->type_name != "EA::Reflection::Text") {
            return rs::make_err("Invalid camera collection name " + entry.key);
        }
        if (!names.emplace(stock_names::lookup8(entry.key), text->data).second) {
            return rs::make_err(std::string("Duplicate camera collection hash"));
        }
    }
    std::map<std::string, cam::ShotDefinition> definitions;
    for (const auto& entry : data.entries()) {
        if (entry.class_name != CLASS) continue;
        const std::string& key = entry.key;
        auto flt = [&](const std::string& field) -> rs::Result<float, std::string> {
            return from_data(data.float_(CLASS, key, field));
        };
        auto enumeration = [&](const std::string& field) -> rs::Result<std::uint32_t, std::string> {
            S3_CAM_TRY_DATA(auto words, data.words<1>(CLASS, key, field));
            return words[0];
        };
        auto boolean = [&](const std::string& field) -> rs::Result<std::uint8_t, std::string> {
            S3_CAM_TRY_DATA(bool v, data.boolean(CLASS, key, field));
            return std::uint8_t(v ? 1 : 0);
        };
        auto floats = [&](const std::vector<const char*>& fields) -> rs::Result<std::vector<float>, std::string> {
            std::vector<float> out;
            for (const char* field : fields) {
                S3_CAM_TRY(float v, flt(field));
                out.push_back(v);
            }
            return out;
        };
        auto child = [&](const std::string& field) -> rs::Result<std::optional<std::string>, std::string> {
            S3_CAM_TRY_DATA(auto words, data.words<6>(CLASS, key, field));
            const std::uint64_t cls = (std::uint64_t(words[0]) << 32) | std::uint64_t(words[1]);
            const std::uint64_t hash = (std::uint64_t(words[2]) << 32) | std::uint64_t(words[3]);
            if (hash == 0) {
                return std::optional<std::string>();
            }
            if (cls != CLASS_HASH) {
                return rs::make_err("Non-shot reference " + key + "/" + field);
            }
            auto found = names.find(hash);
            if (found == names.end()) {
                return rs::make_err("Missing camera child " + key + "/" + field + ": " + hex16(hash));
            }
            if (found->second.empty()) {
                return std::optional<std::string>();
            }
            return std::optional<std::string>(to_ascii_lowercase(found->second));
        };
        cam::Shot shot = cam::Shot::new_();
        S3_CAM_TRY(shot.distance, flt("PositionDistance"));
        S3_CAM_TRY(shot.lens_length, flt("FramingLensLength"));
        S3_CAM_TRY(auto smoothing,
                   floats({"SmoothingDirection", "SmoothingElevation", "SmoothingYaw", "SmoothingPitch"}));
        shot.smoothing = rs::from_fn<4>([&](std::size_t i) { return smoothing[i]; });
        S3_CAM_TRY(auto weights,
                   floats({"ReferenceWeightHead", "ReferenceWeightHips", "ReferenceWeightGrind", "ReferenceWeightCOM",
                           "ReferenceWeightBoard", "ReferenceWeightTrajectory", "ReferenceWeightAnchor",
                           "ReferenceWeightFeet", "ReferenceWeightDampedCOM", "Hash_D70C95EDC16A7DFC"}));
        shot.reference_weights = rs::from_fn<10>([&](std::size_t i) { return weights[i]; });
        S3_CAM_TRY(shot.board_offset, flt("ReferenceBoardOffset"));
        S3_CAM_TRY(float position_heading, flt("PositionHeading"));
        shot.position_heading = position_heading * RADIANS;
        S3_CAM_TRY(float position_elevation, flt("PositionElevation"));
        shot.position_elevation = position_elevation * RADIANS;
        S3_CAM_TRY(float roll, flt("FramingRoll"));
        S3_CAM_TRY(float yaw, flt("FramingYaw"));
        S3_CAM_TRY(float pitch, flt("FramingPitch"));
        shot.framing = {roll * RADIANS, yaw * RADIANS, pitch * RADIANS};
        S3_CAM_TRY(shot.follow_subject_in_air, boolean("OptionFollowSubjectInAir"));
        S3_CAM_TRY(shot.mirror_for_stance, boolean("OptionMirrorForStance"));
        S3_CAM_TRY(shot.snap_to_reference_point, boolean("OptionSnapToReferencePoint"));
        S3_CAM_TRY(shot.use_previous_shot, boolean("OptionUsePreviousShot"));
        S3_CAM_TRY(shot.use_drop_predictor, boolean("OptionUseDropPredictor"));
        S3_CAM_TRY(shot.use_free_camera_stick, boolean("OptionUseFreeCamStick"));
        // The sole dynamic attribute has the native missing-value zero.
        // All layout fields above are required in the converted data.
        S3_CAM_TRY(bool avoidance_present, has_field(data, key, "OptionAvoidanceOverride"));
        if (avoidance_present) {
            S3_CAM_TRY(shot.avoidance_override, boolean("OptionAvoidanceOverride"));
        } else {
            shot.avoidance_override = 0;
        }
        S3_CAM_TRY(shot.blur, flt("FXBlurShot"));
        S3_CAM_TRY(shot.transition_blur, flt("FXBlurTransition"));
        S3_CAM_TRY(shot.subject_opacity, flt("FXSubjectOpacity"));
        S3_CAM_TRY(shot.collision_hint, enumeration("OptionCollisionType"));
        S3_CAM_TRY(shot.anchor, enumeration("OptionAnchor"));
        S3_CAM_TRY(shot.compass_north, enumeration("OptionCompassNorth"));
        S3_CAM_TRY(float world_heading, flt("WorldHeading"));
        shot.world_heading = world_heading * RADIANS;

        cam::ShotDefinition definition;
        definition.name = to_ascii_lowercase(key);
        S3_CAM_TRY(definition.shot_type, enumeration("ShotType"));
        definition.shot = shot;
        S3_CAM_TRY(definition.transition_time, flt("TransitionTime"));
        S3_CAM_TRY(definition.transition_units, enumeration("TransitionTimeUnits"));
        S3_CAM_TRY(definition.children[0], child("BlendShot1"));
        S3_CAM_TRY(definition.children[1], child("BlendShot2"));
        S3_CAM_TRY(definition.children[2], child("BlendShot3"));
        S3_CAM_TRY(definition.blend_points[0], flt("BlendPoint1"));
        S3_CAM_TRY(definition.blend_points[1], flt("BlendPoint2"));
        S3_CAM_TRY(definition.blend_points[2], flt("BlendPoint3"));
        S3_CAM_TRY(definition.blend_value, flt("BlendValue"));
        S3_CAM_TRY(definition.blend_type, enumeration("BlendType"));
        S3_CAM_TRY(definition.blend_smoothing, flt("SmoothingBlendValue"));
        const std::string name = definition.name;
        if (!definitions.emplace(name, std::move(definition)).second) {
            return rs::make_err("Duplicate lowercase camera shot " + key);
        }
    }
    for (const auto& [_, definition] : definitions) {
        for (const auto& name : definition.children) {
            if (name.has_value() && definitions.count(*name) == 0) {
                return rs::make_err("Camera shot " + definition.name + " references missing named shot " + *name);
            }
        }
    }
    StockShots out;
    out._0 = std::move(definitions);
    return out;
}

rs::Result<cam::ShotDefinition, std::string> StockShots::load(const std::string& name) const {
    auto found = this->_0.find(to_ascii_lowercase(name));
    if (found == this->_0.end()) {
        return rs::make_err("Missing stock camera shot " + name);
    }
    return found->second;
}

}  // namespace s3::game::camera::shot_data
