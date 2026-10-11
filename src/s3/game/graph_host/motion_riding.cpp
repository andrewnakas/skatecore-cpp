// Port of skate-game/src/graph_host/motion_riding.rs (module `motion_riding`).
#include "s3/game/graph_host/motion_riding.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_riding {

using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

rs::Result<s3::animation::body_tilt::Settings, std::string> body_tilt_settings(const s3::data::Collections& data) {
    S3_MOTION_TRY(const auto words,
                  motion_collections::words<12>(data, "anim_motion", "body_tilt", "body_tilt_bodyspin_factor"));
    auto f = [&](const char* name) { return motion_collections::float_(data, "anim_motion", "body_tilt", name); };
    s3::animation::body_tilt::Settings s;
    s.body_spin_factor = motion_collections::graph<4>(words, 4, 8);
    S3_MOTION_TRY(s.ground_velocity, f("body_tilt_ground_clamp_vel"));
    S3_MOTION_TRY(s.ground_acceleration, f("body_tilt_ground_clamp_acc"));
    S3_MOTION_TRY(s.air_velocity, f("body_tilt_air_clamp_vel"));
    S3_MOTION_TRY(s.air_acceleration, f("body_tilt_air_clamp_acc"));
    return s;
}

std::optional<RidingOperation> RidingOperation::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    const std::string& n = *name;
    auto text_or_empty = [&](const char* key) {
        const std::string* v = a.text(key);
        return v ? *v : std::string();
    };
    if (n == "CreateAttribute") {
        //82BAE918: the always branch returns before reading set and
        //writes Update twice, leaving End's initialized zero intact.
        std::array<std::optional<float>, 3> values{};
        bool set = false;
        if (const auto* always = a.get("always")) {
            const float value = rs::from_bits(always->float_bits);
            values = {value, value, 0.0f};
            set = false;
        } else if (a.get("begin") == nullptr && a.get("update") == nullptr && a.get("end") == nullptr) {
            values = {std::nullopt, 0.0f, std::nullopt};
            set = false;
        } else {
            const char* keys[] = {"begin", "update", "end"};
            for (std::size_t i = 0; i < 3; ++i) {
                const auto* v = a.get(keys[i]);
                values[i] = v ? std::optional<float>(rs::from_bits(v->float_bits)) : std::nullopt;
            }
            set = a.boolean_byte("set", 0) != 0;
        }
        return RidingOperation(CreateAttribute{encode(text_or_empty("attName")), values, set});
    }
    if (n == "SetDistComToBoard") {
        return RidingOperation(SetDistComToBoard{encode(text_or_empty("attribute")),
                                                 a.boolean_byte("setManually", 0) != 0,
                                                 a.boolean_byte("adjustForVel", 0) != 0});
    }
    if (n == "ForcePhysics") {
        const std::string* force_ptr = a.text("force");
        const std::string force = force_ptr ? *force_ptr : std::string("FOLLOW_ANIMATION_DATA");
        const ForceMode mode = force == "FORCE_PHYSICS_SKATEBOARD" ? ForceMode::PhysicsBoard
                               : force == "FORCE_ANIM_SKATEBOARD"  ? ForceMode::AnimationBoard
                                                                   : ForceMode::AnimationData;
        return RidingOperation(ForcePhysics{mode});
    }
    if (n == "UpdateTimeSinceTeleport") return RidingOperation(TimeSinceTeleport{});
    if (n == "UpdateTimeSinceKickturn") return RidingOperation(TimeSinceKickturn{});
    if (n == "UpdateManualOutTimer") return RidingOperation(ManualOutTimer{});
    if (n == "SetManualOutTimer") {
        return RidingOperation(SetManualOutTimer{rs::from_bits(a.float_bits("length", 0x3e29fbe7u))});  //factory82BCA0FC
    }
    return std::nullopt;
}

rs::Result<rs::Unit, std::string> RidingState::execute(const RidingOperation& operation, std::uint8_t phase,
                                                       const Frame& frame, MotionAnimation& animation,
                                                       std::optional<std::uint32_t> physical_category,
                                                       std::optional<float> animation_height, bool manualing) {
    if (const auto* op = std::get_if<RidingOperation::CreateAttribute>(&operation.value)) {
        if (phase < 3) {
            if (const std::optional<float> value = op->values[phase]) {
                //82BAEC90 emits the graph record before setting tree data.
                animation.emit_packet(op->name, *value);
                if (op->set) animation.set_attribute(SettableAttribute{op->name, *value, false, -1});
            }
        } else {
            S3_PANIC("index out of bounds");  // Rust `values[usize::from(phase)]`
        }
    } else if (const auto* op = std::get_if<RidingOperation::SetDistComToBoard>(&operation.value)) {
        if (phase == 0) {
            //82BB04E8: source literal0.65 only when explicitly authored.
            float height = 0.0f;
            if (op->manually) {
                height = rs::from_bits(0x3f266666u);
            } else {
                if (!animation_height) {
                    return rs::make_err(std::string("SetDistComToBoard requires actual PhysOutAnimation distance"));
                }
                height = *animation_height;
            }
            float correction = 0.0f;
            if (op->adjust_for_velocity) {
                const float v = frame.dt * last_good_landing_velocity;
                const float cap = rs::from_bits(0x3e19999au);
                correction = cap - v >= 0.0f ? v : cap;
            }
            animation.set_attribute(SettableAttribute{op->name, height - correction, false, -1});
        }
    } else if (const auto* op = std::get_if<RidingOperation::ForcePhysics>(&operation.value)) {
        if (phase == 0) force_mode = op->_0;
    } else if (std::holds_alternative<RidingOperation::TimeSinceTeleport>(operation.value)) {
        if (phase == 1) {
            //82BB9058: actual filtered category5 resets; all others add
            //controller delta through8258F968, with no imposed maximum.
            if (!physical_category) {
                return rs::make_err(std::string("UpdateTimeSinceTeleport needs filtered physical state"));
            }
            if (*physical_category == 5) {
                time_since_teleport = 0.0f;
            } else {
                time_since_teleport += frame.dt;
            }
        }
    } else if (std::holds_alternative<RidingOperation::TimeSinceKickturn>(operation.value)) {
        if (phase == 1) time_since_kickturn += frame.dt;  //82BAE848/8258F940
    } else if (std::holds_alternative<RidingOperation::ManualOutTimer>(operation.value)) {
        if (phase == 2) {
            manual_out_timer = 0.0f;  //82BB9260/8258F980
        } else if (phase == 1 && !manualing) {
            const float remaining = manual_out_timer - frame.dt;
            manual_out_timer = remaining >= 0.0f ? remaining : 0.0f;  //82BB91A8/8258F998
        }
    } else if (const auto* op = std::get_if<RidingOperation::SetManualOutTimer>(&operation.value)) {
        //Vtable8232090C: Begin/Update empty; End82BB9158 sets length.
        if (phase == 2) manual_out_timer = op->_0;
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_riding
