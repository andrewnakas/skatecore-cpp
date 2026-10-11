// Port of skate-core/src/player/input_phase/grind_output.rs (whole file).
// Typed Grinds PhysOut block. Offsets identify native producers, not Rust ABI.
// Reset82DE3518; common Fill82D40EA0; tipslide Fill82D42788;
// chromosome publication82DEE508; contact conditioner82DF0640.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::input_phase::grind_output {

using animation::output::attributes::AttributeName;
using types::RawVector;

namespace detail {
inline constexpr RawVector X{0x3f800000u, 0, 0, 0};
inline constexpr RawVector Y{0, 0x3f800000u, 0, 0};
inline constexpr std::array<std::uint32_t, 6> EMPTY{0, 0, 0, 0, 0, 0xffffffffu};
}  // namespace detail

/// Non-trivial Rust `Default` expressed as member initialisers.
struct GrindOutputFields {
    RawVector direction_0 = detail::X;
    RawVector point_16{};
    RawVector normal_32 = detail::Y;
    RawVector across_48 = detail::Y;
    RawVector primitive_start_64{};
    RawVector primitive_end_80{};
    RawVector camera_target_96{};
    RawVector high_side_112{};
    float impact_speed_128 = 0.0f;
    float crouch_132 = 0.0f;
    /// Physical family0..5 (not admission kind), then substate.
    std::array<std::uint32_t, 2> words_136_140{0xffffffffu, 0};
    std::uint32_t animation_id_144 = 0;
    std::uint32_t scoring_id_148 = 0;
    std::uint32_t scorable_id_152 = 0xffffffffu;
    /// Five-word stock FastString representation, not a hash of the display name.
    /// None means the external reset-name template has not been supplied.
    /// 82F8AF08 initializes template830C0D60 from empty string82060799.
    std::optional<AttributeName> animation_name_156 = animation::skeleton_input::name::encode("");
    std::optional<AttributeName> scoring_name_176 = animation::skeleton_input::name::encode("");
    std::optional<AttributeName> surface_name_196{};
    std::uint32_t audio_surface_216 = 0;
    std::array<std::uint64_t, 2> spline_guids_224_232{0xffffffffffffffffull, 0xffffffffffffffffull};
    /// Common physical classification104, consumed by GrindTrickOutTypeAllowed.
    std::uint32_t trick_out_240 = 0;
    /// Approach, location, twist, tilt, orientation, family. No game enum here.
    std::array<std::uint32_t, 6> volatile_chromosome_244 = detail::EMPTY;
    /// First component at268 is the graph's filtered approach observation.
    std::array<std::uint32_t, 6> animation_chromosome_268 = detail::EMPTY;
    std::array<std::uint32_t, 6> scoring_chromosome_292 = detail::EMPTY;
    /// Selector-owned; the camera conditioner must not infer this flag.
    std::uint8_t grinding_316 = 0;
    std::uint8_t leaving_317 = 0;
    std::uint8_t flag_318 = 0;
    std::uint8_t flag_319 = 0;
    std::uint8_t is_ledge_320 = 0;
    std::uint8_t curb_321 = 0;
    std::uint8_t flag_322 = 0;
    std::uint8_t flag_323 = 0;
    std::uint8_t dropping_in_324 = 0;
    std::uint8_t tipslide_325 = 0;
    std::uint8_t tipslide_326 = 0;

    /// The native reset copies two external FastString templates; it does not
    /// construct a grind name from family0. Supply decoded original templates.
    /// Keeping these optional distinguishes an unavailable template from zeros.
    void reset_names(std::optional<AttributeName> grind, std::optional<AttributeName> surface) {
        animation_name_156 = grind;
        scoring_name_176 = grind;
        surface_name_196 = surface;
    }
};
S3_DERIVE_EQ(GrindOutputFields, direction_0, point_16, normal_32, across_48, primitive_start_64, primitive_end_80,
             camera_target_96, high_side_112, impact_speed_128, crouch_132, words_136_140, animation_id_144,
             scoring_id_148, scorable_id_152, animation_name_156, scoring_name_176, surface_name_196,
             audio_surface_216, spline_guids_224_232, trick_out_240, volatile_chromosome_244,
             animation_chromosome_268, scoring_chromosome_292, grinding_316, leaving_317, flag_318, flag_319,
             is_ledge_320, curb_321, flag_322, flag_323, dropping_in_324, tipslide_325, tipslide_326)

}  // namespace s3::player::input_phase::grind_output
