// Port of simref sim/src/camera/graph_conditions.rs.
#include "s3/game/camera/graph_conditions.hpp"

#include <algorithm>
#include <cstdlib>

#include "s3/game/graph_host/condition_nodes.hpp"

namespace s3::game::camera::graph_conditions {

using s3::data::state_graph_attributes::key_hash;

namespace {

/// Rust `str::parse::<u32>()`: optional '+', decimal digits only, no overflow.
bool parse_u32(const std::string& text, std::uint32_t& out) {
    std::size_t i = 0;
    if (!text.empty() && text[0] == '+') i = 1;
    if (i >= text.size()) return false;
    std::uint64_t v = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c < '0' || c > '9') return false;
        v = v * 10 + std::uint64_t(c - '0');
        if (v > 0xffffffffull) return false;
    }
    out = std::uint32_t(v);
    return true;
}

}  // namespace

rs::Result<Condition, std::string> Condition::parse(const Attributes& a) {
    using B = Boolean;
    using V = Value;
    const std::string* name_ptr = a.text("name");
    if (name_ptr == nullptr) {
        return rs::make_err(std::string("Camera condition requires name"));
    }
    const std::string& name = *name_ptr;
    static const std::pair<const char*, Boolean> BOOLEANS[] = {
        {"IsInObserverMode", B::Observer},
        {"IsWipingOut", B::WipingOut},
        {"IsBrokenBoneSlowMo", B::BrokenBone},
        {"IsOffboard", B::Offboard},
        {"IsAirOffboard", B::OffboardAir},
        {"IsInOnBoardAir", B::OnboardAir},
        {"IsFixedInAir", B::FixedInAir},
        {"IsPreparingToJump", B::Preparing},
        {"IsInManual", B::Manual},
        {"SkaterIsDroppingIn", B::DroppingIn},
        {"SkaterIsMovingObject", B::MovingObject},
        {"SkaterIsSkitching", B::Skitching},
        {"SkaterIsPerformingFootPlant", B::Footplant},
        {"SkaterIsPerformingHandPlant", B::Handplant},
        {"SkaterIsPerformingBoneless", B::Boneless},
        {"SkaterIsPerformingHippyJump", B::HippyJump},
        {"SkaterIsPerformingHippyHurdle", B::HippyHurdle},
        {"SkaterIsGrinding", B::Grinding},
        {"SkaterWasGrinding", B::WasGrinding},
        {"IsPlayerSkateboardOnGround", B::OnGround},
        {"IsReentry", B::Reentry},
        {"SkaterIsOnRoad", B::Road},
        {"IsGrindLedgeLeft", B::LedgeLeft},
        {"IsGrindLedgeRight", B::LedgeRight},
        {"IsInWorldPainterRegion", B::WorldPainter},
        {"SkaterIsUnderThreat", B::Threat},
        {"IsCameraInBadCollision", B::BadCollision},
    };
    for (const auto& [text, value] : BOOLEANS) {
        if (name == text) {
            return Condition{BooleanV{value}};
        }
    }
    static const std::pair<const char*, Value> VALUES[] = {
        {"CameraSubjectSpeed", V::Speed},
        {"CameraSubjectSpeedY", V::SpeedY},
        {"CameraSubjectAcceleration", V::Acceleration},
        {"PredictedAirTime", V::AirTime},
        {"PredictedLandingHeight", V::LandingHeight},
        {"PredictedMaxHeight", V::MaxHeight},
        {"PredictedHeightDistanceRatio", V::HeightRatio},
        {"FrontsideAngle", V::FrontsideAngle},
        {"TransferAngle", V::TransferAngle},
        {"LaunchInclineAngle", V::LaunchIncline},
        {"LandingInclineAngle", V::LandingIncline},
        {"GrindInclineAngle", V::GrindIncline},
        {"InclineAngle", V::Incline},
        {"InclineAngleAbs", V::AbsoluteIncline},
        {"TurningCentredTime", V::CentredTime},
        {"TurningTime", V::TurningTime},
        {"TimeSinceLastPlayerInput", V::TimeSinceInput},
    };
    for (const auto& [text, value] : VALUES) {
        if (name == text) {
            return Condition{Numeric{value, s3::game::graph_host::condition_nodes::numeric(a)}};
        }
    }
    if (name == "IsCameraTypeActive") {
        const std::string* type = a.text("type");
        if (type == nullptr) {
            return rs::make_err(std::string("IsCameraTypeActive requires type"));
        }
        std::uint32_t v = 0;
        if (!parse_u32(*type, v)) {
            return rs::make_err(std::string("Invalid normal camera type"));
        }
        return Condition{Type{v}};
    }
    if (name == "IsPreviousShot") {
        Previous p;
        for (const std::string& v : shot_names(a)) p._0.push_back(key_hash(v));
        return Condition{std::move(p)};
    }
    if (name == "IsInVolume") {
        const std::string* volume = a.text("volume");
        if (volume == nullptr) {
            return rs::make_err(std::string("IsInVolume requires volume"));
        }
        return Condition{Volume{*volume}};
    }
    if (name == "IsWipeoutBodyTweak") {
        // Constructor82DF5588 uses case-sensitive comparison and keeps0
        // for both Freefall and an unrecognized tweak name.
        const std::string* tweak_ptr = a.text("tweak");
        const std::string tweak = tweak_ptr != nullptr ? *tweak_ptr : std::string();
        std::uint32_t v = 0;
        if (tweak == "CannonBall") v = 1;
        else if (tweak == "JudoKick") v = 2;
        else if (tweak == "SwanDive") v = 3;
        else if (tweak == "Torpedo") v = 4;
        return Condition{Tweak{v}};
    }
    return rs::make_err("Unimplemented stock camera condition " + name);
}

bool Condition::evaluate(const s3::camera::CameraMan& m, const s3::camera::ManagerSubject& s,
                         const CameraGraphSubject& g, const CameraGraphEnvironment& world) const {
    using B = Boolean;
    using V = Value;
    const auto& f = m.rig.fields;
    const auto& state = m.state;
    constexpr float DEGREES = rs::from_bits(0x42652ee1);
    if (const auto* v = std::get_if<Type>(&this->value)) {
        return v->_0 == world.camera_type;
    }
    if (const auto* v = std::get_if<Previous>(&this->value)) {
        const std::uint32_t h = key_hash(m.shots.current().name);
        return std::find(v->_0.begin(), v->_0.end(), h) != v->_0.end();
    }
    if (const auto* v = std::get_if<Tweak>(&this->value)) {
        return v->_0 == g.wipeout_tweak;
    }
    if (const auto* v = std::get_if<Volume>(&this->value)) {
        //82DF90C0 compares exactly strlen(requested) bytes with82990158.
        for (const std::string& volume : world.volumes) {
            if (volume.compare(0, v->_0.size(), v->_0) == 0 && volume.size() >= v->_0.size()) return true;
        }
        return false;
    }
    if (const auto* v = std::get_if<BooleanV>(&this->value)) {
        switch (v->_0) {
            case B::Observer: return s.special_effect != 0;
            case B::WipingOut: return s.rig.wiping_out != 0;
            case B::BrokenBone: return s.rig.broken_bone_slowmo != 0;
            case B::Offboard: return s.rig.off_board != 0 || g.running_out;
            case B::OffboardAir: return g.offboard_air;
            case B::OnboardAir: return g.onboard_air;
            case B::FixedInAir: return s.rig.air_flag_452 != 0;
            case B::Preparing: return g.preparing_to_jump;
            case B::Manual: return g.manual;
            case B::DroppingIn: return g.dropping_in;
            case B::MovingObject: return g.moving_object;
            case B::Skitching: return g.skitching;
            case B::Footplant: return g.footplant;
            case B::Handplant: return g.handplant;
            case B::Boneless: return g.boneless;
            case B::HippyJump: return g.hippy_jump;
            case B::HippyHurdle: return g.hippy_hurdle;
            case B::Grinding: return f.height_mode == 2;
            case B::WasGrinding: return f.previous_height_mode == 2;
            case B::OnGround: return f.height_mode == 0;
            case B::Reentry: return (state.flags & 0x10) != 0;
            case B::Road: return world.on_road;
            case B::LedgeLeft: return world.ledge_left;
            case B::LedgeRight: return world.ledge_right;
            // TU382DF5050 returns false. AI is intentionally absent from this game.
            case B::WorldPainter:
            case B::Threat: return false;
            case B::BadCollision:
                return (m.rig.positioner.flags & 0x80) != 0 && m.rig.positioner.distance < f.distance * 0.75f;
        }
        return false;
    }
    const auto& numeric = std::get<Numeric>(this->value);
    float value = 0.0f;
    switch (numeric._0) {
        //82E03060: signed velocity projected on the subject's forward axis.
        case V::Speed: {
            const auto& n = s.rig.transform[2];
            const auto& v = f.velocity;
            value = (v[0] * n[0] + v[1] * n[1]) + v[2] * n[2];
            break;
        }
        case V::SpeedY: value = f.velocity[1]; break;
        case V::Acceleration: value = f.smoothed_acceleration; break;
        case V::AirTime: value = s.valid_trajectory_duration(); break;
        case V::LandingHeight: value = state.landing_height_delta; break;
        case V::MaxHeight: value = state.apex_height; break;
        case V::HeightRatio: value = state.apex_height_ratio; break;
        case V::FrontsideAngle: value = state.frontside_angle; break;
        case V::TransferAngle: value = state.launch_landing_heading_delta * DEGREES; break;
        case V::LaunchIncline: value = state.launch_incline * DEGREES; break;
        case V::LandingIncline: value = state.landing_incline * DEGREES; break;
        case V::GrindIncline: value = state.direction_incline * DEGREES; break;
        case V::Incline: value = state.velocity_incline * DEGREES; break;
        case V::AbsoluteIncline: value = rs::abs(state.absolute_velocity_incline * DEGREES); break;
        case V::CentredTime: value = state.centred_time; break;
        case V::TurningTime: value = state.steering_time; break;
        case V::TimeSinceInput: value = g.time_since_player_input; break;
    }
    return numeric._1.matches(value);
}

std::vector<std::string> shot_names(const Attributes& a) {
    std::vector<std::string> out;
    for (std::size_t i = 1;; ++i) {
        const std::string* text = a.text("shot" + std::to_string(i));
        if (text == nullptr) break;
        out.push_back(*text);
    }
    return out;
}

}  // namespace s3::game::camera::graph_conditions
