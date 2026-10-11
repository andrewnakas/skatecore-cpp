// Port of skate-game/src/skater_animation/state.rs.
//! State shared by graph construction, stance events and physics publication.
#pragma once
#include <cstdint>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/physics_packet.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::game::skater_animation::state {

using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::physics_packet::SkaterPublicationState;

/// `pub(super)` in Rust.
struct AnimationState {
    std::uint32_t flags = 0;
    SkaterPublicationState publication;
    /// Base animator14652: ctor82D17CDC sets zero; virtual40/82531200 writes it.
    float phase = 0.0f;

    static AnimationState new_(bool local_player);

    bool mirrored() const { return (flags & 0x40000000u) != 0; }
    bool fakie() const { return (flags & 0x20000000u) != 0; }
    bool switch_() const { return publication.relative_stance == 1; }

    ///82B98980 calls82D19010's first-match query. No scalar/status test:
    ///presence in the collected tree attribute list toggles the state once.
    void apply_stance_events(s3::animation::Slice<const AnimationAttribute> attributes);
    void prepare_publication();
    void finish_publication();
    ///SkaterAnim virtual172/82B98078, selected by local-player bit27.
    float cull_threshold() const;
};

}  // namespace s3::game::skater_animation::state
