// Port of skate-core/src/riding/grounded/state/contact_state.rs.
// Ground contact identity/update leaf (`82D38430`).
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/rs.hpp"

namespace s3::riding::grounded::state::data {
template <class T>
struct GroundContactHistory;
}  // namespace s3::riding::grounded::state::data

namespace s3::riding::grounded::state::contact_state {

using data::GroundContactHistory;

using Vector4 = std::array<float, 4>;

/// Values copied into the native contact publication record.
struct GroundContactStateInput {
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    Vector4 velocity_400{};
    Vector4 axis_464{};
    Vector4 contact_position_560{};
    Vector4 trajectory_position_592{};
    Vector4 reckoning_vector_1200{};
    /// Selected global collection +396 -> layout+4 -> +560.
    std::int32_t differing_contact_frame_limit = 0;
};
S3_DERIVE_EQ(GroundContactStateInput, flags_2476, flags_2480, velocity_400, axis_464, contact_position_560,
             trajectory_position_592, reckoning_vector_1200, differing_contact_frame_limit)

/// Rust trait `GroundContactServices` (duck-typed template parameter `S`).
/// Required owner/collision operations around state-owned counter logic.
///   using Error = ...;
///   using Handle = ...;  // Eq
///   /// `82D62F20`, called first on the inactive branch.
///   rs::Result<rs::Unit, Error> stop_contact_tracking_82d62f20();
///   /// Exact owner+3024 reset at `82D38468..82D3848C`.
///   rs::Result<rs::Unit, Error> clear_inactive_contact_output_3024();
///   /// Owner+1920, read only when the retained handle is empty.
///   rs::Result<std::optional<Handle>, Error> initial_contact_handle_1920();
///   /// `82D63C30` over the first three vectors of the publication record.
///   rs::Result<std::optional<Handle>, Error> identify_contact_82d63c30(const GroundContactStateInput& frame);
///   /// `82D61268`, with all five vectors and the retained handle (`Option<&Handle>`).
///   rs::Result<rs::Unit, Error> publish_contact_82d61268(const GroundContactStateInput& frame,
///                                                        const Handle* retained);

/// Runs the full TU3 state/call order. Counter updates use wrapping integer
/// arithmetic because the native body uses ordinary `addi` instructions.
template <class S>
rs::Result<rs::Unit, typename S::Error> update(GroundContactHistory<typename S::Handle>& history,
                                               GroundContactStateInput input, S& services) {
    if ((input.flags_2476 & 0x00400000u) == 0) {
        {
            auto r = services.stop_contact_tracking_82d62f20();
            if (r.is_err()) return rs::make_err(r.unwrap_err());
        }
        {
            auto r = services.clear_inactive_contact_output_3024();
            if (r.is_err()) return rs::make_err(r.unwrap_err());
        }
        return rs::Unit{};
    }

    if ((input.flags_2480 & 0x20000000u) != 0 || (input.flags_2480 & 0x10000000u) == 0) {
        history.tracked_contact_2760 = std::nullopt;
        history.differing_contact_frames_2756 = 0;
    }

    if ((input.flags_2480 & 0x10000000u) != 0) {
        if (input.axis_464[1] < rs::from_bits(0x3f733333u)) {
            if (!history.tracked_contact_2760.has_value()) {
                auto r = services.initial_contact_handle_1920();
                if (r.is_err()) return rs::make_err(r.unwrap_err());
                history.tracked_contact_2760 = r.unwrap();
            }
            auto identified = services.identify_contact_82d63c30(input);
            if (identified.is_err()) return rs::make_err(identified.unwrap_err());
            const auto current = identified.unwrap();
            history.differing_contact_frames_2756 =
                current == history.tracked_contact_2760
                    ? 0
                    : std::int32_t(std::uint32_t(history.differing_contact_frames_2756) + 1u);
            if (history.differing_contact_frames_2756 > input.differing_contact_frame_limit) {
                history.tracked_contact_2760 = std::nullopt;
            }
        } else {
            // Native label clears only the retained handle here.
            history.tracked_contact_2760 = std::nullopt;
        }
    }

    return services.publish_contact_82d61268(
        input, history.tracked_contact_2760.has_value() ? &*history.tracked_contact_2760 : nullptr);
}

}  // namespace s3::riding::grounded::state::contact_state

#include "s3/core/riding/grounded/state/data.hpp"
