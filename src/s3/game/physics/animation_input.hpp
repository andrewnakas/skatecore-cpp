// Port of skate-game/src/physics/animation_input.rs.
//! Animation attributes become owned physical inputs through the actual
//! Skeleton dispatcher. This adapter loads stock settings and retains caches;
//! it does not choose animations or manufacture a push from controller input.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/skeleton_input/attribute_finalization.hpp"
#include "s3/core/animation/skeleton_input/contact_events.hpp"
#include "s3/core/animation/skeleton_input/extended_attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/animation/skeleton_input/process_attributes.hpp"
#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/input/controller.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animation_input {

struct AnimationInput {
    sc::animation::skeleton_input::scalar_attributes::ScalarAttributeInputs fields{};
    sc::animation::skeleton_input::extended_attributes::ExtendedAttributes extra{};
    sc::animation::skeleton_input::contact_events::ContactEventState contacts{};
    sc::animation::skeleton_input::scalar_attributes::AnimationControlOutput output{};
    // Private in Rust.
    sc::animation::skeleton_input::attribute_finalization::JumpAttributeState cached_jump{};
    std::vector<sc::animation::output::attributes::AttributeName> bone_names;
    std::size_t right_toe = 0;
    sc::animation::skeleton_input::attribute_finalization::FinalizationInput settings{};
    std::array<bool, 5> height_overrides{};

    static rs::Result<AnimationInput, std::string> load(const sd::Collections& data, const sd::AnimationFrames& frames,
                                                        std::string_view mode);

    Status select_physics_mode(std::uint32_t mode);

    /// ProcessedPhysIn Reset precedes physical/packet flag publication. The
    /// caller then publishes those flags before dispatching the current pose.
    void reset_processed();

    ///82DB7DCC..7E0C runs AFTER publishing AnimOut into the physical output.
    /// It is distinct from ProcessedPhysIn reset at the start of input.
    void finish_output_publication();

    Status process(sc::animation::Slice<const sc::animation::output::attributes::AnimationAttribute> attributes,
                   sc::animation::Slice<const sc::animation::output::NativeMatrix> hierarchy, float timestep,
                   std::uint32_t animation_flags, bool external_impulse_active,
                   sc::input::controller::ActionMap& map);
};

}  // namespace s3::game::physics::animation_input
