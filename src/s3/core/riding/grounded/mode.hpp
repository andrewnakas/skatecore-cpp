// Port of skate-core/src/riding/grounded/mode.rs.
// Mode binding producer, TU3 ProcessInput 82DB4420..447C.
// Numeric mode identities are preserved without assigning guessed names.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::riding::grounded::mode {

/// The two fields are deliberately separate: an unsupported request is still
/// published, while the previously selected attribute binding remains active.
template <class T>
struct ProcessedMode {
    /// ProcessedPhysIn+2528, copied from skeleton+10928 on every update.
    std::uint32_t requested_mode = 0;
    /// ProcessedPhysIn+2548. Supply actual resource bindings, not host addresses.
    T selected_attributes{};

    /// `attributes` correspond in order to player+1408/1424/1440/1456/1472.
    /// Their constructor/asset loading is a separate dependency. There is no
    /// guessed initial binding and no default-mode fallback for invalid input.
    void update(std::uint32_t requested_mode_, const std::array<T, 5>& attributes) {
        if (requested_mode_ <= 4) {
            selected_attributes = attributes[requested_mode_];
        }
        requested_mode = requested_mode_;
    }

    bool operator==(const ProcessedMode& o) const {
        return requested_mode == o.requested_mode && selected_attributes == o.selected_attributes;
    }
    bool operator!=(const ProcessedMode& o) const { return !(*this == o); }
};

}  // namespace s3::riding::grounded::mode
