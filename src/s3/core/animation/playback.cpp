// Port of skate-core/src/animation/playback.rs.
#include "s3/core/animation/playback.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::animation::playback {

using Status = rs::Result<rs::Unit, std::string>;

Status PlayAnimationInstance::begin(const PlayAnimation& operation, PlaybackContext& context,
                                    PlaybackService& service) {
    first_update = true;
    {
        auto r = refresh_parameters(operation.parameters, true, service);
        if (r.is_err()) return r;
    }
    service.set_construction_value(skeleton_input::name::encode("ProSkater"), context.pro_skater);
    // The graph override is consumed/reset before variant choice or play.
    std::optional<TransitionSettings> taken = context.transition_override;
    context.transition_override.reset();
    const TransitionSettings transition = (taken && taken->kind != 0) ? *taken : operation.transition;
    const std::string* animation = &operation.animation;
    if (operation.switch_animation) {
        if (!context.is_switch) return rs::make_err(std::string("PlayAnimation needs skater switch state"));
        if (*context.is_switch) animation = &*operation.switch_animation;
    }
    if (operation.mirror_animation) {
        if (!context.is_mirrored) return rs::make_err(std::string("PlayAnimation needs skater mirror state"));
        if (*context.is_mirrored) animation = &*operation.mirror_animation;
    }
    if (operation.no_board_animation) {
        if (!context.board_available)
            return rs::make_err(std::string("PlayAnimation needs PhysOut board availability"));
        if (!*context.board_available) animation = &*operation.no_board_animation;
    }
    service.set_posture_enabled(operation.apply_posture);
    bool played = false;
    if (transition.kind >= 1 && transition.kind <= 4) {
        auto r = service.play(PlaybackRequest{*animation, operation.playback_speed, 0.0f, transition});
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        played = r.unwrap();
    }
    if (!played) {
        // This fallback is present in native Begin82BB5630..5660. The
        // service must not use it to conceal missing tree implementations.
        TransitionSettings fallback = operation.transition;
        fallback.kind = 1;
        auto r = service.play(PlaybackRequest{"KeepDefaultAnim", 1.0f, 0.0f, fallback});
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    return rs::Unit{};
}

Status PlayAnimationInstance::update(const PlayAnimation& operation, PlaybackService& service) {
    if (!first_update) {
        auto r = refresh_parameters(operation.parameters, false, service);
        if (r.is_err()) return r;
    }
    first_update = false;
    return rs::Unit{};
}

Status refresh_parameters(Slice<const PlaybackParameter> parameters, bool beginning, PlaybackService& service) {
    // Keep read/write effects interleaved across parameters while avoiding two
    // mutable borrows of the same concrete animation service.
    struct One : AttributeSink {
        std::optional<playback_parameters::SettableAttribute> _0;
        void set_attribute(playback_parameters::SettableAttribute value) override { _0 = value; }
    };
    for (const PlaybackParameter& parameter : parameters) {
        One output;
        auto r = parameter.update(beginning, service, output);
        if (r.is_err()) return r;
        if (output._0) {
            service.set_attribute(*output._0);
        }
    }
    return rs::Unit{};
}

}  // namespace s3::animation::playback
