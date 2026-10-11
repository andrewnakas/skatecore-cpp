// C++-only compile/instantiation check (no Rust counterpart): instantiates the
// duck-typed service templates of riding/grounded/** (board, ordinary, lifecycle,
// post_physics, manual_entry, pumping controller) so template errors surface here.
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/riding.hpp"

namespace {
using namespace s3::riding::grounded::state;
using s3::rs::Result;
using s3::rs::Unit;
using V4 = std::array<float, 4>;

struct BoardServices {
    using Error = int;
    using BoardError = int;
    using AnimatedPose = int;
    std::vector<std::string> events;

    // ManualAngleMeasurement
    Result<float, Error> angle_between(V4, V4, V4) { return 0.0f; }
    // AntiFlipNudgeMath
    Result<float, Error> dot3(V4 a, V4 b) { return s3::physics::native_arithmetic::dot3(a, b); }
    Result<V4, Error> scale_to_magnitude(V4 v, float squared, float magnitude) {
        return s3::riding::ground_correction_math::scale_to_magnitude(v, squared, magnitude);
    }
    // HangUpServices
    Result<V4, Error> build_hang_force() { return V4{}; }
    Result<Unit, Error> apply_hang_force(V4) { return Unit{}; }
    Result<bool, Error> detect_hung_up_geometry() { return false; }
    Result<Unit, Error> request_wipeout() { return Unit{}; }
    // HalfpipeWheelCatchServices
    Result<V4, Error> angular_displacement() { return V4{}; }
    Result<Unit, Error> apply_angular_displacement(V4) { return Unit{}; }
    // PinningServices
    Result<Unit, Error> pin_to_captured_position(float, float) { return Unit{}; }
    // GroundBoardServices
    Result<float, BoardError> center_of_mass_height_82d38838() { return 1.0f; }
    Result<Unit, BoardError> set_contact_wheel_materials() { return Unit{}; }
    Result<board_types::GroundContactResponse, BoardError> contact_response_82d93df0(board_types::GroundContactFrame,
                                                                                     V4 previous) {
        board_types::GroundContactResponse r;
        r.tag_16_force.tag = 16;
        r.vector_2688 = previous;
        r.animated_board_2708 = animate;
        return r;
    }
    Result<Unit, BoardError> update_body_accumulator_82d389dc() { return Unit{}; }
    Result<Unit, BoardError> set_animated_velocity_82c04168(V4) { return Unit{}; }
    Result<Unit, BoardError> write_processed_velocity_400(V4) { return Unit{}; }
    Result<AnimatedPose, BoardError> build_animated_pose_82d33448() { return 7; }
    Result<Unit, BoardError> publish_animated_pose_82be33d0(const AnimatedPose&) { return Unit{}; }
    Result<Unit, BoardError> update_external_player_82d67848(const AnimatedPose&, V4) { return Unit{}; }
    Result<Unit, BoardError> commit_external_player_82d68800() { return Unit{}; }
    Result<Unit, BoardError> finalize_animated_board_sk83_na_f_01a4() { return Unit{}; }
    Result<std::optional<board_types::CollisionForceResponse>, BoardError> collision_force_82d944e8(V4) {
        return std::optional<board_types::CollisionForceResponse>();
    }
    Result<float, BoardError> collision_force_dot_velocity_82d38e18(V4, V4) { return 0.0f; }
    Result<Unit, BoardError> apply_vector_82c07000(V4) {
        events.push_back("vector");
        return Unit{};
    }
    Result<Unit, BoardError> apply_angular_displacement_82c075b8(V4) {
        events.push_back("angular");
        return Unit{};
    }
    bool animate = false;
};

struct EnterServices {
    using Error = int;
    Result<Unit, Error> set_board_standard_82c03af8() { return Unit{}; }
    Result<Unit, Error> initialize_ground_skeleton() { return Unit{}; }
    Result<Unit, Error> set_collision_state_82d911a8(std::uint32_t) { return Unit{}; }
    Result<Unit, Error> reset_ground_external_objects() { return Unit{}; }
    Result<Unit, Error> initialize_board_speed_82d375d0() { return Unit{}; }
    Result<Unit, Error> enter_from_air_82bf2f18() { return Unit{}; }
    Result<s3::physics::force_queue::QueuedPointForce, Error> landing_on_deck_force() {
        return s3::physics::force_queue::QueuedPointForce{};
    }
    Result<Unit, Error> set_controller_mode_one() { return Unit{}; }
    Result<Unit, Error> set_default_wheel_materials() { return Unit{}; }
    Result<Unit, Error> restore_animated_board_velocity(V4) { return Unit{}; }
    Result<Unit, Error> check_for_ground_wipeout_82d8f9e0() { return Unit{}; }
    Result<Unit, Error> update_board_path_82c05ec0() { return Unit{}; }
};

struct Bodies {
    std::array<V4, 7> velocity{};
    V4 linear_velocity(std::size_t part) { return velocity[part]; }
    void set_linear_velocity(std::size_t part, V4 v) { velocity[part] = v; }
};
struct Projection {
    using Error = int;
    Result<float, Error> normal_speed(V4 n, V4 v) { return s3::physics::native_arithmetic::dot3(n, v); }
};

S3_TEST(cpp_only_grounded_state_templates_instantiate_and_run) {
    s3::riding::steering::SteeringSettings steering{};
    steering.speed_graph_max_speed = 1.0f;
    s3::riding::speed_wobble::SpeedWobbleSettings wobble{};
    s3::riding::ground_force::GroundForceSettings ground_force{};
    s3::riding::speed_model::SpeedModelSettings speed_model{};
    s3::riding::slide_friction::SlideFrictionSettings slide{};
    s3::riding::straighten::StraightenSettings straighten{};
    s3::riding::heading::HeadingSettings heading{};
    s3::riding::anti_flip::AntiFlipSettings anti_flip{};
    s3::physics::manual::settings::ManualSettings manual{};
    board_types::GroundBoardSettings settings{steering, wobble, {},    ground_force, speed_model, slide,
                                              straighten, heading, anti_flip, manual, {}, {}};
    s3::riding::speed_wobble::SpeedWobbleState wobble_state{};
    s3::riding::steering::TruckSteeringState trucks{};
    s3::riding::speed_model::SpeedModelState speed_state{};
    s3::physics::manual::state::ManualState manual_state{};
    float heading_previous = 0.0f;
    s3::physics::force_queue::BoardForceQueue queue{};
    const std::array<std::size_t, 2> indices{0, 1};
    std::array<s3::physics::rigid_body::RetailInertiaDynamics, 2> inertias{};
    s3::riding::grounded::drag::BodyInertias body_inertias{indices, inertias};
    board_types::GroundBoardComponents components{wobble_state, trucks,         speed_state,  manual_state,
                                                  heading_previous, queue, body_inertias};
    auto state = PhysicsGroundState::before_first_enter(true);
    BoardServices services{};
    board_types::GroundBoardInput input{};
    input.propulsion.timestep = 1.0f / 60.0f;
    input.pump_force.timestep = 1.0f / 60.0f;
    const auto outcome = board::update(state, components, settings, input, services).unwrap();
    CHECK(std::holds_alternative<board::GroundBoardOutcome::Ordinary>(outcome.value));
    CHECK_EQ(services.events.size(), std::size_t(4));
    services.animate = true;
    const auto animated = board::update(state, components, settings, input, services).unwrap();
    CHECK(std::holds_alternative<board::GroundBoardOutcome::Animated>(animated.value));
    CHECK(state.flag_2720);

    EnterServices enter_services{};
    Bodies bodies{};
    Projection projection{};
    s3::riding::pumping::state::PumpingState pumping{};
    lifecycle::GroundEnterInput enter_input{};
    enter_input.previous_state_2504 = 503;
    const auto entered = lifecycle::enter(state, lifecycle::GroundEntryComponents{pumping, wobble_state, manual_state, queue},
                                          enter_input, bodies, projection, enter_services)
                             .unwrap();
    CHECK(entered.landing_force_attempted);
    lifecycle::exit(state, pumping, body_inertias, enter_services).unwrap();
    post_physics::update_post_physics(-1, enter_services).unwrap();

    s3::riding::pumping::geometry::NativePumpingGeometry geometry{};
    s3::riding::pumping::controller::update_ground(pumping, s3::riding::pumping::settings::PumpingSettings{},
                                                   s3::riding::pumping::settings::PumpingMode{},
                                                   s3::riding::pumping::controller::PumpingSample{}, geometry)
        .unwrap();
    CHECK(pumping.record_valid);
}

}  // namespace
