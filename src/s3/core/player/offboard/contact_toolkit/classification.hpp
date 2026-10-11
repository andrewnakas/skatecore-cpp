// Port of skate-core/src/player/offboard/contact_toolkit/classification.rs (whole file).
// TU3 82D848D0: slope classification and retained three-sample acceptance.
#pragma once
#include "s3/core/player/offboard/contact_toolkit/prefix.hpp"

namespace s3::player::offboard::contact_toolkit::classification {

/// Toolkit fields +30452..30468, retained across query refreshes.
struct History {
    std::uint32_t accepted = 0;
    std::array<std::uint32_t, 3> samples{};
    std::size_t cursor = 0;

    void classify(ContactPrefix& prefix, Vector direction);
};
S3_DERIVE_EQ(History, accepted, samples, cursor)

}  // namespace s3::player::offboard::contact_toolkit::classification
