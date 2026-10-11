// Port of skate-core/src/player/offboard/biped_air/recovered.rs and the State
// methods of its #[path] submodules (height, post, collision, orientation).
// BipedAir501 state from original TU3 IDA, build SHA256
// 431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
// Native result offsets identify fields;288 is velocity,304 is a normal.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/biped_air/collision.hpp"
#include "s3/core/player/offboard/biped_air/feet.hpp"
#include "s3/core/player/offboard/biped_air/height.hpp"
#include "s3/core/player/offboard/biped_air/post.hpp"
#include "s3/core/player/offboard/biped_air/result.hpp"
#include "s3/core/player/offboard/biped_air/sampling.hpp"
#include "s3/core/player/offboard/biped_air/selection.hpp"
#include "s3/core/player/offboard/cadence.hpp"
#include "s3/core/player/offboard/controller.hpp"

namespace s3::player::offboard::biped_air::recovered {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;
using result::OffBoardOutput;
using result::TrajectoryResult;
inline const float DT = rs::from_bits(0x3c888889u);

struct EnterInput {
    Frame animation_frame{};
    Vector body_position_15872{};
    Vector body_position_15936{};
    Vector position_592{};
    Vector up_544{};
    std::uint32_t flags_2484 = 0;
};

struct State {
    Frame frame_80{};
    Frame frame_144{};
    Frame frame_208{};
    TrajectoryResult result{};
    Vector body_target_416{};
    float height_432 = 0.0f;
    float body_offset_436 = 0.0f;
    float blend_440 = 0.0f;
    float time_remaining_444 = 0.0f;
    float duration_448 = 0.0f;
    std::int32_t frame_452 = 0;
    Vector local_contact_464{};
    Vector initial_up_480{};
    Vector adjustment_496{};
    Vector vector_512{};
    Vector restart_normal_528{};
    std::array<bool, 7> flags_544_550{};
    ///Host publication of the shared Biped cadence; never reset on Air entry.
    cadence::BipedCadence cadence{};
    bool active = false;

    /// Rust `impl Default` (identity frames, then reset()).
    State();

    ///82D2EDD8, including selective result reset and byte548=true.
    void reset();
    ///State-local82D2E5C0. Caller performs manager/feet reset and conditional
    ///launch, then Biped.place between these two explicit entry stages.
    void begin_enter(const EnterInput& input);
    ///Host places the shared landing reset between Air reset and initialization.
    void begin_enter_after_reset(const EnterInput& input);
    void finish_enter(const EnterInput& input);
    ///Only Enter's !selector8494 branch and contact restart call this producer.
    rs::Result<air_launch::Packet, const char*> launch_packet(const air_launch::Processed& input,
                                                              const controller::State& biped,
                                                              const point_graph::PointGraph<8>& turn,
                                                              air_launch::Settings settings) const;
    ///82D2EF20 clears the shared selector, not the landing manager or result.
    void exit(sampling::SelectorState& selector);
    std::int32_t begin_update();
    ///82D2F170..434, after frame sampling and first-valid orientation setup.
    void update_times();
    ///82D2F564..650. Receives the SAME Biped cadence used while walking.
    void update_cadence(controller::State& biped, float requested_phase);
    OffBoardOutput output() const;

    // ---- height.rs ----
    void correct_height(const height::HeightInput& input);

    // ---- collision.rs ----
    collision::Response collision_response(const std::array<Vector, 2>& errors_16288_16304, Vector up_544,
                                           bool restart_allowed_8493);
    ///82D2FC18..FD58: caller first produces the complete native launch packet.
    air_launch::Packet restart_packet(air_launch::Packet packet, Vector normal, Vector position_592, Vector up_544,
                                      float foot_height);
    ///Only after the host successfully submits the restart82D6CA58.
    void finish_restart(const Frame& effective_animation_frame, Vector normal);
    ///82D2F490..534 adjusts the current sample after a restart.
    void correct_restarted_sample();

    // ---- orientation.rs ----
    /// Return the actual adjustment to submit to selector82D6DE08.
    std::optional<Vector> animation_adjustment(Vector value_2880);
    /// Called immediately after selector sampling.
    void orient_sample(float start_angle_2936, std::uint32_t flags_2476);
    ///82D2F698..6CC: the larger early assist decays over the source interval.
    static float landing_assist_limit(float elapsed_2664);
    void finish_landing_latch();

    // ---- post.rs ----
    ///Caller synchronizes the canonical landing manager immediately before this.
    void post_physics(const post::PostInput& input, const post::Settings& settings, const wipeout::Frame& frame,
                      wipeout::Mode mode, Vector root_velocity, wipeout::Requests& requests) const;

private:
    void landing_orientation(Vector velocity, Vector up, float remaining, float start_angle,
                             std::uint32_t flags_2476);
};

}  // namespace s3::player::offboard::biped_air::recovered
