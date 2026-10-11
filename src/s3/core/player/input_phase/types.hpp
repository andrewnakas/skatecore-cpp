// Port of skate-core/src/player/input_phase/types.rs (whole file).
// Skate 3 TU3 `PhysicalPlayerHiLOD::Input` (`0x82DB4048`) packet types.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <tuple>

#include "s3/core/player/_deps/animation_output_actor_packet.hpp"
#include "s3/core/player/_deps/input_animation_packet.hpp"
#include "s3/core/player/input_phase/air_output.hpp"
#include "s3/core/player/input_phase/grind_input.hpp"
#include "s3/core/player/input_phase/grind_output.hpp"
#include "s3/core/player/input_phase/pose_output.hpp"
#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/player/teleport_state.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::input_phase::types {

using air_output::AirOutputFields;
using animation::output::actor_packet::ExternalPhysicsInput;
using grind_input::GrindInvestigationFields;
using grind_output::GrindOutputFields;
using input::animation_packet::AnimationPacketFields;
using pose_output::AnimationOutputFields;
using pose_output::ScoringOutputFields;
using pose_output::SkeletonOutputFields;

struct ProbeFields {
    std::array<RawVector, 3> vectors_16_32_48{};
    std::array<std::uint32_t, 2> words_64_68{};
    std::array<std::uint8_t, 2> bytes_72_73{};
    RawVector vector_80{};
    std::array<std::uint32_t, 2> words_96_100{};
    std::uint8_t byte_104 = 0;
};
S3_DERIVE_EQ(ProbeFields, vectors_16_32_48, words_64_68, bytes_72_73, vector_80, words_96_100, byte_104)

struct LineTestFields {
    RawVector position{};
    RawVector normal{};
    std::uint32_t surface = 0;
    /// The native byte is copied in full; conditionals test it for nonzero.
    std::uint8_t valid = 0;
};
S3_DERIVE_EQ(LineTestFields, position, normal, surface, valid)

struct StateVariantFields {
    /// Byte +60 of the object referenced by the variant block's +4 pointer.
    std::uint8_t surface_override_enabled_60 = 0;
};
S3_DERIVE_EQ(StateVariantFields, surface_override_enabled_60)

/// Reset82DE53F0 copies the template initialized by82DE3BE8:
/// every represented word and byte in this subset initially equals zero.
struct CurrentStateFields {
    std::uint32_t identifier_8 = 0;
    std::uint32_t category_12 = 0;
    std::uint32_t state_16 = 0;
    float surface_height_32 = 0.0f;
    /// BipedGround Fill82D32D38, independent of State40.
    std::uint32_t counter_36 = 0;
    std::uint32_t skitch_value_40 = 0;
    std::uint8_t flag_61 = 0;
    std::uint8_t flag_66 = 0;
    std::uint8_t flag_69 = 0;
    std::uint8_t flag_74 = 0;
    std::uint8_t signed_ground_step_84 = 0;
};
S3_DERIVE_EQ(CurrentStateFields, identifier_8, category_12, state_16, surface_height_32, counter_36, skitch_value_40,
             flag_61, flag_66, flag_69, flag_74, signed_ground_step_84)

struct SkateboardMotionFields {
    RawVector vector_64{};
    RawVector vector_80{};
    RawVector vector_96{};
    RawVector vector_128{};
    RawVector vector_144{};
    float scalar_160 = 0.0f;
    float scalar_164 = 0.0f;
    float scalar_168 = 0.0f;
    float scalar_172 = 0.0f;
};
S3_DERIVE_EQ(SkateboardMotionFields, vector_64, vector_80, vector_96, vector_128, vector_144, scalar_160, scalar_164,
             scalar_168, scalar_172)

struct SystemReckoningFields {
    /// Physical COM velocity from Skeleton16176 (82BE1D84/94).
    /// Restored r28 at82DB49B0 is16, not128.
    RawVector vector_16{};
    RawVector vector_64{};
    RawVector vector_96{};
    RawVector vector_144{};
    std::uint8_t flag_164 = 0;
};
S3_DERIVE_EQ(SystemReckoningFields, vector_16, vector_64, vector_96, vector_144, flag_164)

struct GroundOutputFields {
    /// State503 Fill82D4E090..E0C8.
    std::int32_t landing_half_turns_312 = 0;
    std::uint8_t hippy_jumping_322 = 0;
    std::uint8_t hippy_takeoff_323 = 0;
    RawVector vector_64{};
    /// Board Fill82C03318: wheel-contact normal (not Motion80 velocity).
    RawVector vector_80{};
    /// Host storage for Motion273, NOT native Ground273. Common82DB7598
    /// publishes Processed2468 bit20; keep this distinct from the normal.
    std::uint8_t flag_273 = 0;
    RawVector vector_96{};
    RawVector vector_128{};
    float scalar_276 = 0.0f;
    float scalar_292 = 0.0f;
    float scalar_296 = 0.0f;
    std::uint8_t flag_317 = 0;
    std::uint8_t flag_318 = 0;
};
S3_DERIVE_EQ(GroundOutputFields, landing_half_turns_312, hippy_jumping_322, hippy_takeoff_323, vector_64, vector_80,
             flag_273, vector_96, vector_128, scalar_276, scalar_292, scalar_296, flag_317, flag_318)

struct CollisionOutputFields {
    std::uint32_t wheel_count_0 = 0;
    float scalar_28 = 0.0f;
    /// Offboard landing manager82D79498; reset82DE3290 clears this position.
    RawVector vector_48{};
    RawVector predicted_position_64{};
    std::uint8_t flag_215 = 0;
    std::uint8_t flag_216 = 0;
    std::array<std::uint8_t, 4> wheel_contact_3296_3299{};
    std::uint8_t flag_3472 = 0;
    std::uint8_t flag_3475 = 0;
    std::uint8_t flag_3477 = 0;
    std::uint8_t flag_3478 = 0;
    std::uint8_t flag_3479 = 0;
    std::uint8_t flag_3480 = 0;
    std::uint8_t flag_3481 = 0;
    /// Offboard landing manager82D7948C; reset82DE32E4 clears availability.
    std::uint8_t flag_3482 = 0;
    std::uint8_t flag_3483 = 0;
};
S3_DERIVE_EQ(CollisionOutputFields, wheel_count_0, scalar_28, vector_48, predicted_position_64, flag_215, flag_216,
             wheel_contact_3296_3299, flag_3472, flag_3475, flag_3477, flag_3478, flag_3479, flag_3480, flag_3481,
             flag_3482, flag_3483)

struct PhysicsOutputFields {
    std::uint8_t flag_32 = 0;
    /// Physics+16. This native record is only48bytes long.
    RawVector vector_16{};
};
S3_DERIVE_EQ(PhysicsOutputFields, flag_32, vector_16)

struct OffBoardOutputFields {
    /// BipedGround Fill82D32D38 and shared Feet Fill82D785F8.
    std::uint32_t kind_88 = 0;
    float scalar_112 = 0.0f;
    float distance_116 = 0.0f;
    std::array<std::uint8_t, 2> flags_306_307{};
    std::uint8_t flag_329 = 0;
    std::uint8_t flag_330 = 0;
    std::uint8_t flag_334 = 0;
    /// Common ProcessOutput82DB724C..7258 from the shared Biped720/716.
    float cadence_phase_80 = 0.0f;
    std::uint32_t locomotion_state_84 = 0;
    /// SkateboardController FillPhysOut82D76D20 orientation outputs +36/+40.
    float angle_36 = 0.0f;
    float angle_40 = 0.0f;
    /// Reset82DE40C0; Biped trajectory publication writes the duration here.
    float trajectory_time_120 = 0.0f;
    /// Reset82DE4170; BipedAir Fill82D30324 publishes trajectory availability.
    std::uint8_t trajectory_valid_331 = 0;
    float scalar_32 = 0.0f;
    std::uint8_t flag_304 = 0;
    /// Common ProcessOutput82DB76F4: Processed2480 bit19, not grab-object304.
    std::uint8_t flag_308 = 0;
    std::uint8_t flag_311 = 0;
    /// SkateboardController82D76D88: state448 is FREE/HIDING/RETURNING.
    std::uint8_t free_board_312 = 0;
    /// 82D76DA0: physical RETURNING state, distinct from animation activity323.
    std::uint8_t returning_board_313 = 0;
    std::uint8_t flag_314 = 0;
    std::uint8_t flag_315 = 0;
    std::uint8_t flag_316 = 0;
    /// OffBoard reset82DE4138 clears this. LandingOnDeckManager Fill82D79420
    /// publishes manager260 && !manager258; only Biped/LandingOnDeck state
    /// outputs call that producer. Ordinary Ground retains the reset value.
    std::uint8_t hippy_hurdling_317 = 0;
    std::uint8_t flag_318 = 0;
    std::uint8_t dropping_board_322 = 0;
    /// 82D76ED4: state448 is HIDING.
    std::uint8_t hiding_board_321 = 0;
    std::uint8_t flag_319 = 0;
    std::uint8_t retrieving_board_323 = 0;
    /// 82D76F68: retrieving animation OR retrieve pulse while RETURNING.
    std::uint8_t flag_324 = 0;
    std::uint8_t flag_328 = 0;
    RawVector vector_64{};
    /// BipedAir Fill82D30300: distinct from the on-board Air output block.
    float scalar_92 = 0.0f;
    RawVector vector_96{};
    std::uint32_t word_144 = 0;
    float scalar_148 = 0.0f;
    float scalar_152 = 0.0f;
    float scalar_156 = 0.0f;
    RawVector vector_160{};
    RawVector vector_176{};
    RawVector vector_192{};
    RawVector vector_208{};
    RawVector vector_224{};
    RawVector vector_240{};
    std::uint8_t flag_320 = 0;
};
inline bool operator==(const OffBoardOutputFields& a, const OffBoardOutputFields& b) {
    auto t = [](const OffBoardOutputFields& v) {
        return std::tie(v.kind_88, v.scalar_112, v.distance_116, v.flags_306_307, v.flag_329, v.flag_330, v.flag_334,
                        v.cadence_phase_80, v.locomotion_state_84, v.angle_36, v.angle_40, v.trajectory_time_120,
                        v.trajectory_valid_331, v.scalar_32, v.flag_304, v.flag_308, v.flag_311, v.free_board_312,
                        v.returning_board_313, v.flag_314, v.flag_315, v.flag_316, v.hippy_hurdling_317, v.flag_318,
                        v.dropping_board_322, v.hiding_board_321, v.flag_319, v.retrieving_board_323, v.flag_324,
                        v.flag_328, v.vector_64, v.scalar_92, v.vector_96, v.word_144, v.scalar_148, v.scalar_152,
                        v.scalar_156, v.vector_160, v.vector_176, v.vector_192, v.vector_208, v.vector_224,
                        v.vector_240, v.flag_320);
    };
    return t(a) == t(b);
}
inline bool operator!=(const OffBoardOutputFields& a, const OffBoardOutputFields& b) { return !(a == b); }

struct PhysicalPlayerInput {
    /// SkateboardReckoning+176; restored r29 at82DB4994 is176.
    RawVector board_reckoning_side_176{};
    SkateboardMotionFields skateboard;
    AirOutputFields air;
    PhysicsOutputFields physics;
    GrindOutputFields grinds;
    SkeletonOutputFields skeleton;
    AnimationOutputFields animation;
    ScoringOutputFields scoring;
    /// Present only when Teleporting702 publishes a ready reset target.
    std::optional<teleport_state::Output> teleport_output{};
    CollisionOutputFields collision;
    CurrentStateFields state;
    GroundOutputFields ground;
    SystemReckoningFields reckoning;
    std::uint32_t filtered_state_0 = 0;
    OffBoardOutputFields off_board;
    /// `player+1896 -> +0 -> +24 -> +16`, before the variant override.
    std::uint32_t surface_default_mode = 0;
    /// Player component +1832, word +1876; source bit 28 is consumed.
    std::uint32_t component_1832_word_1876 = 0;
};
S3_DERIVE_EQ(PhysicalPlayerInput, board_reckoning_side_176, skateboard, air, physics, grinds, skeleton, animation,
             scoring, teleport_output, collision, state, ground, reckoning, filtered_state_0, off_board,
             surface_default_mode, component_1832_word_1876)

/// Rust `AnimationInputPacket<'a>`: the two borrowed records are const references.
struct AnimationInputPacket {
    const AnimationPacketFields& publication;
    const ExternalPhysicsInput& external_physics_10512;
    std::uint8_t flag_10369 = 0;
    std::uint8_t flag_10370 = 0;
    std::uint8_t flag_10373 = 0;
    std::uint8_t suppress_transition_10376 = 0;
    std::uint8_t use_external_physics_10688 = 0;
    std::uint8_t external_physics_flag_10689 = 0;
    std::uint8_t flag_10786 = 0;
    std::uint8_t flag_10787 = 0;
    std::uint8_t force_braking_10796 = 0;
    RawVector vector_10816{};
    RawVector vector_10832{};
    RawVector vector_10848{};
    RawVector vector_10864{};
    RawVector vector_10880{};
    RawVector vector_10896{};
    float scalar_10912 = 0.0f;
    float scalar_10916 = 0.0f;
    std::uint32_t state_variant_10928 = 0;
    std::uint32_t flags_10932 = 0;
};

enum class NativeReferenceBase {
    Player,
    SurfaceSelector,
};

struct NativeRelativeReference {
    NativeReferenceBase base = NativeReferenceBase::Player;
    std::uint16_t offset = 0;
};
S3_DERIVE_EQ(NativeRelativeReference, base, offset)

inline ExternalPhysicsInput empty_external_physics() { return ExternalPhysicsInput{}; }

struct PlayerInputState {
    std::uint32_t manager_1856_counter_320 = 0;
    std::uint8_t manager_1852_flag_256 = 0;
    RawVector manager_1852_vector_176{};
    std::uint32_t flags_1296 = 0;
    std::uint32_t grounded_frames_1300 = 0;
    std::uint32_t state_count_1312 = 0;
    std::uint32_t update_count_1316 = 0;
    std::uint32_t spin_same_direction_frames_1324 = 0;
    std::uint32_t frames_since_teleport_1328 = 0;
    /// ProcessOutput82DB78A8 keeps a dismount request alive for three outputs.
    std::uint32_t dismount_request_frames_1332 = 0;
    std::uint32_t state_value_1336 = 0;
    float state_timer_1344 = 0.0f;
    float time_since_last_input_1348 = 0.0f;
    float time_on_ground_1352 = 0.0f;
    float signed_ground_time_1356 = 0.0f;
    float previous_spin_input_1360 = 0.0f;
    float previous_crouch_1364 = 0.0f;
    float time_off_board_1368 = 0.0f;
    std::uint32_t skitch_value_1372 = 0;
    float skitch_timer_1376 = 0.0f;
    float ground_timer_1380 = 0.0f;
    float secondary_ground_timer_1384 = 0.0f;
    ProbeFields probe{};
    RawVector queued_vector_1280{};
    ExternalPhysicsInput external_physics_cache_1008 = empty_external_physics();
    RawVector prepared_jump_velocity_1184{};
    RawVector previous_ground_position_1200{};
    RawVector current_ground_position_1216{};
    RawVector ground_delta_1232{};
    RawVector damped_ground_delta_1248{};
    std::array<StateVariantFields, 5> state_variants_1408{};
    LineTestFields hips_line_test_1488{};
    LineTestFields left_line_test_1536{};
    LineTestFields right_line_test_1584{};
    /// 1632: the hips test's ray *origin*, the hips part's world translation.
    ///
    /// StartSkeletonLineTests82DB63C0 keeps the three origins at 1632/1648/1664 beside the
    /// three results, and `82DB6EC0` publishes this one as PhysOut.Ground +192. The score
    /// collectors need it as a point in its own right, not only as the start of a ray:
    /// 82DAC780's flip bonus tests it against the hit, and 82DA7A38's ground query reports
    /// its height above the hit. The two foot origins are kept by retail too and are not
    /// read by anything ported here.
    RawVector hips_line_origin_1632{};
    /// Number at player +1304 used to gate the ground-history filter.
    std::int32_t ground_history_frames_1304 = 0;
};
S3_DERIVE_EQ(PlayerInputState, manager_1856_counter_320, manager_1852_flag_256, manager_1852_vector_176, flags_1296,
             grounded_frames_1300, state_count_1312, update_count_1316, spin_same_direction_frames_1324,
             frames_since_teleport_1328, dismount_request_frames_1332, state_value_1336, state_timer_1344,
             time_since_last_input_1348, time_on_ground_1352, signed_ground_time_1356, previous_spin_input_1360,
             previous_crouch_1364, time_off_board_1368, skitch_value_1372, skitch_timer_1376, ground_timer_1380,
             secondary_ground_timer_1384, probe, queued_vector_1280, external_physics_cache_1008,
             prepared_jump_velocity_1184, previous_ground_position_1200, current_ground_position_1216,
             ground_delta_1232, damped_ground_delta_1248, state_variants_1408, hips_line_test_1488,
             left_line_test_1536, right_line_test_1584, hips_line_origin_1632, ground_history_frames_1304)

/// Default: every word zero, refs None, external physics empty.
struct ProcessedPhysicsInput {
    RawMatrix effective_anim_transform_192{};
    std::array<RawVector, 2> vectors_400_416{};
    std::array<RawVector, 5> vectors_464_480_496_512_528{};
    std::array<RawVector, 4> vectors_544_560_592_608{};
    std::array<RawVector, 4> vectors_624_640_656_672{};
    RawVector prepared_jump_704{};
    /// Skeleton::ProcessData82BD8E70..8EA8, after ordered animation attributes.
    RawVector collision_pose_error_736{};
    RawVector animation_com_to_deck_752{};
    RawVector animation_com_to_deck_delta_768{};
    std::array<RawVector, 6> vectors_720_784_800_816_832_864{};
    std::array<RawVector, 5> vectors_880_896_912_928_944{};
    std::array<LineTestFields, 3> line_tests_960_1008_1056{};
    GrindInvestigationFields grind{};
    RawVector vector_1520{};
    RawMatrix matrix_1536{};
    std::uint8_t byte_1600 = 0;
    ExternalPhysicsInput external_physics_1616 = empty_external_physics();
    ProbeFields probe_1792{};
    /// Actual grab-spline publication payloads; flags2480 bits22/21 carry readiness.
    std::array<std::array<std::uint32_t, 72>, 2> grab_records_1888_2176{};
    /// Interactable identity published by82D740F8; bit20 carries validity.
    std::uint32_t object_2464 = 0;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t flags_2488 = 0;
    std::uint32_t state_identifier_2496 = 0;
    std::uint32_t state_2504 = 0;
    std::uint32_t state_2508 = 0;
    std::uint32_t category_2512 = 0;
    std::uint32_t category_2516 = 0;
    std::uint32_t player_state_value_2520 = 0;
    std::uint32_t filtered_state_2524 = 0;
    std::uint32_t state_variant_index_2528 = 0;
    std::array<std::uint32_t, 2> grind_words_2532_2536{};
    std::uint32_t surface_mode_2540 = 0;
    std::optional<NativeRelativeReference> surface_primary_ref_2544{};
    std::optional<NativeRelativeReference> state_variant_ref_2548{};
    std::optional<NativeRelativeReference> surface_secondary_ref_2552{};
    std::uint32_t wheel_count_2556 = 0;
    std::uint32_t state_count_2564 = 0;
    std::uint32_t update_count_2568 = 0;
    std::uint32_t spin_same_direction_frames_2580 = 0;
    std::uint32_t frames_since_teleport_2584 = 0;
    std::uint32_t skitch_value_2592 = 0;
    std::uint32_t left_surface_2596 = 0;
    std::uint32_t right_surface_2600 = 0;
    float timestep_2604 = 0.0f;
    float scalar_2612 = 0.0f;
    /// PrepareBoardToolkit82C01744: signed speed times its ordered travel sign.
    float scalar_2616 = 0.0f;
    float transition_2636 = 0.0f;
    /// Skeleton82BDD008: BodySpin plus the actual grind-air adjustment result.
    /// Read only with flags2484bit15; Reset82BFA394 clears it each input tick.
    float grind_adjusted_body_spin_2644 = 0.0f;
    /// Reset82BFA2D0 writes original literal822F8B40, -9.8.
    float gravity_2648 = 0.0f;
    float scalar_2652 = 0.0f;
    float scalar_2656 = 0.0f;
    float state_timer_2664 = 0.0f;
    float scalar_2668 = 0.0f;
    /// Written by Skeleton::ProcessData before the post-skeleton suffix.
    float spin_input_2672 = 0.0f;
    float scalar_2696 = 0.0f;
    float scalar_2700 = 0.0f;
    float scalar_2736 = 0.0f;
    float time_since_last_input_2748 = 0.0f;
    float time_on_ground_2752 = 0.0f;
    float signed_ground_time_2756 = 0.0f;
    float truck_tightness_2760 = 0.0f;
    float scalar_2764 = 0.0f;
    /// Current minus preceding authored board forward-axis Y,82BD8E20.
    float board_at_y_delta_2768 = 0.0f;
    float air_scalar_2772 = 0.0f;
    /// Written by Skeleton::ProcessData before crouch-delta publication.
    float crouch_2776 = 0.0f;
    float crouch_delta_2780 = 0.0f;
    float off_board_scalar_2832 = 0.0f;
    float time_off_board_2836 = 0.0f;
    float ground_timer_2848 = 0.0f;
    float secondary_ground_timer_2852 = 0.0f;
    float collision_scalar_2924 = 0.0f;
    std::uint32_t actor_query_2948 = 0;
    std::uint32_t actor_query_2952 = 0;
};
inline bool operator==(const ProcessedPhysicsInput& a, const ProcessedPhysicsInput& b) {
    auto t = [](const ProcessedPhysicsInput& v) {
        return std::tie(
            v.effective_anim_transform_192, v.vectors_400_416, v.vectors_464_480_496_512_528,
            v.vectors_544_560_592_608, v.vectors_624_640_656_672, v.prepared_jump_704, v.collision_pose_error_736,
            v.animation_com_to_deck_752, v.animation_com_to_deck_delta_768, v.vectors_720_784_800_816_832_864,
            v.vectors_880_896_912_928_944, v.line_tests_960_1008_1056, v.grind, v.vector_1520, v.matrix_1536,
            v.byte_1600, v.external_physics_1616, v.probe_1792, v.grab_records_1888_2176, v.object_2464,
            v.flags_2468, v.flags_2472, v.flags_2476, v.flags_2480, v.flags_2484, v.flags_2488,
            v.state_identifier_2496, v.state_2504, v.state_2508, v.category_2512, v.category_2516,
            v.player_state_value_2520, v.filtered_state_2524, v.state_variant_index_2528, v.grind_words_2532_2536,
            v.surface_mode_2540, v.surface_primary_ref_2544, v.state_variant_ref_2548, v.surface_secondary_ref_2552,
            v.wheel_count_2556, v.state_count_2564, v.update_count_2568, v.spin_same_direction_frames_2580,
            v.frames_since_teleport_2584, v.skitch_value_2592, v.left_surface_2596, v.right_surface_2600,
            v.timestep_2604, v.scalar_2612, v.scalar_2616, v.transition_2636, v.grind_adjusted_body_spin_2644,
            v.gravity_2648, v.scalar_2652, v.scalar_2656, v.state_timer_2664, v.scalar_2668, v.spin_input_2672,
            v.scalar_2696, v.scalar_2700, v.scalar_2736, v.time_since_last_input_2748, v.time_on_ground_2752,
            v.signed_ground_time_2756, v.truck_tightness_2760, v.scalar_2764, v.board_at_y_delta_2768,
            v.air_scalar_2772, v.crouch_2776, v.crouch_delta_2780, v.off_board_scalar_2832, v.time_off_board_2836,
            v.ground_timer_2848, v.secondary_ground_timer_2852, v.collision_scalar_2924, v.actor_query_2948,
            v.actor_query_2952);
    };
    return t(a) == t(b);
}
inline bool operator!=(const ProcessedPhysicsInput& a, const ProcessedPhysicsInput& b) { return !(a == b); }

/// Immutable publication of the completed physical-input phase for one tick.
/// The mutable record remains private to the input producer; selectors and
/// later phases consume this value object so they cannot observe a half-written
/// packet.
struct ProcessedPhysicsSnapshot {
    std::uint64_t tick = 0;
    ProcessedPhysicsInput input{};

    static ProcessedPhysicsSnapshot new_(std::uint64_t tick, const ProcessedPhysicsInput& input) {
        return ProcessedPhysicsSnapshot{tick, input};
    }
};
S3_DERIVE_EQ(ProcessedPhysicsSnapshot, tick, input)

}  // namespace s3::player::input_phase::types
