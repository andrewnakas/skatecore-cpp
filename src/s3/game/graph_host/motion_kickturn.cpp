// Port of skate-game/src/graph_host/motion_kickturn.rs (module `motion_kickturn`).
#include "s3/game/graph_host/motion_kickturn.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_kickturn {

using s3::animation::skeleton_input::name::encode;

std::optional<Operation> Operation::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    if (*name == "ResetTimeSinceKickturn") return Operation(ResetTimer{});
    //82BC7218: both constructor defaults are literal8209975C=0.5.
    if (*name == "KickTurnSteering") {
        kickturn::Parameters p;
        p.max_height = rs::from_bits(a.float_bits("maxHeight", 0x3f000000u));
        p.spin_scale = rs::from_bits(a.float_bits("spinScale", 0x3f000000u));
        return Operation(Steering{p});
    }
    return std::nullopt;
}

rs::Result<kickturn::Settings, std::string> load_settings(const s3::data::Collections& data) {
    //Original8289FB90..FBB0 binds Globals324 to A168897919752875,
    //the independently verified stock anim_motion/kickturn collection key.
    kickturn::Settings s;
    S3_MOTION_TRY(const auto spin, motion_collections::words<20>(data, "anim_motion", "kickturn", "kickturn_spin"));
    s.spin = motion_collections::graph<8>(spin, 4, 12);  //layout240, source X256/Y288
    S3_MOTION_TRY(const auto height,
                  motion_collections::words<20>(data, "anim_motion", "kickturn", "kickturn_balance"));
    s.height = motion_collections::graph<8>(height, 4, 12);  //layout320, X336/Y368
    return s;
}

rs::Result<rs::Unit, std::string> execute(kickturn::State& state, kickturn::Parameters parameters,
                                          const kickturn::Settings& settings, MotionAnimation& animation, float dt,
                                          std::uint8_t phase) {
    if (phase == 0) {
        state.begin(settings);
        return rs::Unit{};
    }
    //Native End82B61BB8 is the no-op lifecycle method.
    if (phase != 1) return rs::Unit{};
    if (state.needs_animation_length()) {
        //82BAE51C..560: IAnimatable84 applies pending parameters through
        //82D18F98, then v48 supplies the tree for GetLength(v20).
        S3_MOTION_CHECK(animation.apply_parameters());
        S3_MOTION_TRY(const float length, animation.current_length());
        state.capture_animation_length(length);
    }
    //Original830BE1E0 initializer82F84BC8 identifies KickTurn. HasIntent's
    //destination is initialized to0; an absent/zero intent retains the sign.
    const kickturn::Output output =
        state.update(dt, animation.motion_intent("KickTurn").value_or(0.0f), parameters, settings);
    //MGspecific v16=8258F6E0 appends attributes, not animation parameters.
    //82F85A08/82F858D0 identify the two original names and this order.
    animation.emit_packet(encode("Balance"), output.balance);
    animation.emit_packet(encode("Spin"), output.spin);
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_kickturn
