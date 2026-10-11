// Port of skate-game/src/skater_animation/state.rs.
#include "s3/game/skater_animation/state.hpp"

#include "s3/core/rs.hpp"

namespace s3::game::skater_animation::state {

AnimationState AnimationState::new_(bool local_player) {
    //82B975F4 selects bit27;82B97674 selects bit17 and clears the
    //orientation,mirror,fakie,weight and request bits used by this owner.
    //Unrepresented allocator/padding bits are not gameplay state here.
    AnimationState out;
    out.flags = (static_cast<std::uint32_t>(local_player) << 27) | 0x00020000u;
    out.phase = 0.0f;
    SkaterPublicationState& p = out.publication;
    p.orientation_bit31 = false;
    p.mirrored = false;
    p.riding_fakie = false;
    p.weight_on_nose = false;
    //82B975B8..82B97674: GOOFY natural stance, NATURAL relative.
    p.natural_stance = 1;
    p.relative_stance = 0;
    p.request_bit16 = false;
    p.request_bit15 = false;
    p.air_dismount_revert_requested = false;
    p.air_dismount_revert_frames = 0;
    p.signal = std::nullopt;
    return out;
}

void AnimationState::apply_stance_events(s3::animation::Slice<const AnimationAttribute> attributes) {
    auto present = [&](std::string_view name) {
        const auto key = s3::animation::skeleton_input::name::encode(name);
        for (const AnimationAttribute& a : attributes)
            if (a.name == key) return true;
        return false;
    };
    if (present("animboardbackward")) flags ^= 0x80000000u;
    if (present("mirrored")) flags ^= 0x40000000u;
    if (present("switch")) publication.relative_stance = static_cast<std::int32_t>(publication.relative_stance == 0);
}

void AnimationState::prepare_publication() {
    SkaterPublicationState& p = publication;
    p.orientation_bit31 = (flags & 0x80000000u) != 0;
    p.mirrored = (flags & 0x40000000u) != 0;
    p.riding_fakie = (flags & 0x20000000u) != 0;
    p.weight_on_nose = (flags & 0x10000000u) != 0;
    p.request_bit16 = (flags & 0x00010000u) != 0;
    p.request_bit15 = (flags & 0x00008000u) != 0;
    p.air_dismount_revert_requested = (flags & 0x00100000u) != 0;
}

void AnimationState::finish_publication() { flags &= ~(0x00010000u | 0x00008000u | 0x00100000u); }

float AnimationState::cull_threshold() const {
    return rs::from_bits((flags & 0x08000000u) != 0 ? 0x3c23d70au : 0x3dcccccdu);
}

}  // namespace s3::game::skater_animation::state
