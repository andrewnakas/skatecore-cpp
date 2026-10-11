// Port of skate-game/src/physics/ground_runtime/settings.rs.
//! Stock bindings used by the complete Ground::UpdateSkateboard call.
//! Toolkit copies follow82D94C10; selected surface/mode fields stay explicit.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/manual/settings.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/riding/anti_flip.hpp"
#include "s3/core/riding/braking.hpp"
#include "s3/core/riding/ground_force.hpp"
#include "s3/core/riding/grounded/propulsion.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"
#include "s3/core/riding/heading.hpp"
#include "s3/core/riding/pumping/state.hpp"
#include "s3/core/riding/slide_friction.hpp"
#include "s3/core/riding/speed_model.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/core/riding/straighten.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/ground_runtime/input.hpp"

namespace s3::game::trainer {
struct TrainerTuning;  // trainer.hpp (coordinator)
}
namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}
namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}
namespace s3::game::physics::animation_input {
struct AnimationInput;  // physics/animation_input.hpp (group D)
}

namespace s3::game::physics::ground_runtime::settings {

using sc::physics::contact::RetailContactMaterial;
using sc::physics::manual::settings::ManualMode;
using sc::physics::manual::settings::ManualSettings;
using sc::point_graph::PointGraph;
using sc::riding::anti_flip::AntiFlipSettings;
using sc::riding::braking::LinearDragSettings;
using sc::riding::ground_force::GroundForceSettings;
using sc::riding::grounded::propulsion::GroundPropulsionSettings;
using sc::riding::grounded::state::board_types::GroundBoardInput;
using sc::riding::grounded::state::board_types::GroundBoardSettings;
using sc::riding::heading::HeadingSettings;
using sc::riding::slide_friction::SlideFrictionSettings;
using sc::riding::speed_model::SpeedModelSettings;
using sc::riding::speed_wobble::SpeedWobbleSettings;
using sc::riding::steering::SteeringSettings;
using sc::riding::straighten::StraightenSettings;

/// `#[derive(Clone)]`
struct GroundSettings {
    SteeringSettings steering{};
    SpeedWobbleSettings wobble{};
    GroundPropulsionSettings propulsion{};
    GroundForceSettings force{};
    SpeedModelSettings speed{};
    SlideFrictionSettings slide{};
    StraightenSettings straighten{};
    HeadingSettings heading{};
    AntiFlipSettings anti_flip{};
    ManualSettings manual{};
    ManualMode manual_mode{};
    LinearDragSettings drag{};
    float collision_duration = 0.0f;
    float collision_scale = 0.0f;
    float contact_force_time = 0.0f;
    RetailContactMaterial wheel_material{};
    float foot_force_offset = 0.0f;
    float push_target_multiplier = 0.0f;
    float absorption_front = 0.0f;
    float absorption_rear = 0.0f;
    float surface_braking_factor = 0.0f;
    float wobble_activation = 0.0f;
    float wobble_amplitude = 0.0f;

    static rs::Result<GroundSettings, std::string> load(const sd::Collections& data, std::string_view mode,
                                                        std::string_view surface);
    GroundSettings tuned(const trainer::TrainerTuning& tuning) const;
    GroundBoardSettings board() const;
    /// ground_runtime/input.rs.
    GroundBoardInput input(const sc::physics::board_toolkit::BoardToolkit& t,
                           const sc::player::input_phase::ProcessedPhysicsInput& p,
                           const animation_input::AnimationInput& a,
                           const sc::riding::pumping::state::PumpingState& pumping, float unintentional_pump_scalar,
                           const riding_outputs::RidingOutputs& riding,
                           const animated_skeleton::AnimatedSkeleton& skeleton,
                           std::array<sc::physics::drive_frames::RetailAffineTransform, 2> base_trucks,
                           input::GroundInputObservations extra) const;
};

/// Immutable stock tables selected by the processed packet, including its
/// difficulty-dependent SurfacePhysics normalization. No per-tick parsing.
/// (Rust tuple struct `GroundProfiles(Vec<Vec<Arc<GroundSettings>>>)`.)
struct GroundProfiles {
    std::vector<std::vector<std::shared_ptr<const GroundSettings>>> _0;

    static rs::Result<GroundProfiles, std::string> load(const sd::Collections& data);
    rs::Result<std::shared_ptr<const GroundSettings>, std::string> select(std::uint32_t mode,
                                                                          std::uint32_t surface) const;
};

/// Private in Rust.
rs::Result<ManualSettings, std::string> manual_settings(const sd::Collections& d);
/// `pub(super)`.
rs::Result<PointGraph<8>, std::string> curve8(const sd::Collections& d, std::string_view c, std::string_view k,
                                              std::string_view f);
/// `SKATE_WHEEL_GRIP` scales the wheel contact's Coulomb friction, for one experiment only.
///
/// The board scrubs sideways an order of magnitude less than retail: lateral wheel speed passes
/// 0.75 m/s in 0.4% of held frames here against retail's 2-8.7%, measured by
/// `tools/audio/skid_scrub_levels.py` off the skid family's own intensity word. Powerslides, which
/// swap this material out for a low-friction one, slip correctly -- so the grip that ordinary
/// riding has and a powerslide lacks is the suspect, and this scales exactly that and nothing
/// else.
///
/// **A measurement knob, not a fix.** The vault's own values (0.7-1.0 per surface) are what retail
/// reads, so anything other than 1.0 here is a deliberate deviation. If lowering it moves the
/// histogram toward retail, the contact solve is over-gripping and the real fix is in the solver;
/// if it barely moves, the riding-layer forces are responsible instead. Either way this goes away.
float wheel_grip_scale();
rs::Result<PointGraph<16>, std::string> curve16(const sd::Collections& d, std::string_view c, std::string_view k,
                                                std::string_view f);

}  // namespace s3::game::physics::ground_runtime::settings
