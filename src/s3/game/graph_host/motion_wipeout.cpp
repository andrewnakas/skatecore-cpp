// Port of skate-game/src/graph_host/motion_wipeout.rs (module `motion_wipeout`).
#include "s3/game/graph_host/motion_wipeout.hpp"

#include "s3/game/graph_host/condition_nodes.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_wipeout {

namespace {
std::string debug_opt(const std::string* value) {
    return value ? "Some(" + s3::data::debug_str(*value) + ")" : std::string("None");
}
}  // namespace

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& data) {
    //82BC0358/051C and stock anim_wipeout schema offsets0..28.
    auto f = [&](const char* name) { return motion_collections::float_(data, "anim_wipeout", "default", name); };
    Settings s;
    S3_MOTION_TRY(s.threshold, f("controlled_drives_thresh"));
    S3_MOTION_TRY(s.twist_velocity, f("clamp_twist_vel"));
    S3_MOTION_TRY(s.twist_acceleration, f("clamp_twist_acc"));
    S3_MOTION_TRY(s.lean_velocity, f("clamp_lean_vel"));
    S3_MOTION_TRY(s.twist_blend, f("blend_twist"));
    S3_MOTION_TRY(s.lean_blend, f("blend_lean"));
    S3_MOTION_TRY(s.gesture_y_blend, f("blend_gesture_y"));
    S3_MOTION_TRY(s.gesture_x_blend, f("blend_gesture_x"));
    return s;
}

rs::Result<std::optional<Condition>, std::string> Condition::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    const std::string name = name_ptr ? *name_ptr : std::string();
    if (name == "IsDoneWipingOut") return std::optional<Condition>(Done{});
    if (name == "WipeoutTimeToLand") return std::optional<Condition>(TimeToLand{condition_nodes::numeric(a)});
    if (name == "WipeoutTimeSinceContact") {
        return std::optional<Condition>(TimeSinceContact{condition_nodes::numeric(a)});
    }
    //82BA82C0: exact authored names; invalid strings leave native storage undefined.
    if (name == "GestureType") {
        const std::string* g = a.text("gesture");
        const char* names[] = {"Freefall", "CannonBall", "JudoKick", "SwanDive", "Torpedo"};
        for (std::uint32_t i = 0; i < 5; ++i) {
            if (g != nullptr && *g == names[i]) return std::optional<Condition>(GestureType{i});
        }
        return rs::make_err("Invalid Wipeout gesture type " + debug_opt(g));
    }
    //82BCAAF8: unrecognized orientation has no native initialized value.
    if (name == "IsInWater") {
        const std::string* o = a.text("orientation");
        const char* names[] = {"onback", "onfront", "either"};
        for (std::uint32_t i = 0; i < 3; ++i) {
            if (o != nullptr && *o == names[i]) return std::optional<Condition>(InWater{i});
        }
        return rs::make_err("Invalid water orientation " + debug_opt(o));
    }
    return std::optional<Condition>();
}

rs::Result<bool, std::string> Condition::evaluate(std::optional<Physical> physical) const {
    if (!physical) return rs::make_err(std::string("Wipeout condition requires completed physical output"));
    const Physical& p = *physical;
    if (std::holds_alternative<Done>(value)) return p.over_599;  //82BC0AB8: skeleton599.
    if (const auto* c = std::get_if<TimeToLand>(&value)) return c->_0.matches(p.collision_time_144);  //82BC0B28.
    if (const auto* c = std::get_if<TimeSinceContact>(&value)) return c->_0.matches(p.no_support_time_548);  //82BC0BB0.
    if (const auto* c = std::get_if<GestureType>(&value)) return p.profile_148 == c->_0;  //82BA8468.
    const std::uint32_t orientation = std::get<InWater>(value)._0;
    //82BBBE90 returns before the vector read when not submerged.
    if (!p.below_surface_82) return false;
    if (!p.orientation_y) return rs::make_err(std::string("IsInWater requires skeleton output80.Y"));
    const float y = *p.orientation_y;
    return static_cast<std::uint32_t>(!(y > 0.0f)) == orientation || orientation == 2;
}

}  // namespace s3::game::graph_host::motion_wipeout
