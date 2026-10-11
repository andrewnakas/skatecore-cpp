// Port of skate-game/src/physics/grind/acquisition.rs.
//! Coordinator boundary only. No copied ZIP query, admission or family priority.
#pragma once
#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/physics/grind_contact/families.hpp"

namespace s3::game::physics::grind::acquisition {

using ::s3::physics::grind_contact::Primitive;
using ::s3::physics::grind_contact::families::Contact;

/// The investigator owns admission, state priority and fallback proximity.
/// Publish this only for an admitted physical contact; a proximity-only result
/// stays with the manager and must not be converted into an active grind here.
struct AcquiredContact {
    /// Includes the manager's seven-valued entry_kind, independently of kind.
    Contact contact;
    /// Exact winning primitive endpoints/owner, not fabricated truck endpoints.
    Primitive primitive;
};

}  // namespace s3::game::physics::grind::acquisition
