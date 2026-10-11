// Port of skate-core/src/physics/grind_contact/investigator.rs.
// Family/proximity arbitration from S3 82D875A8.
// Geometry refinement and balance/engagement run afterward in the manager.
#pragma once
#include "s3/core/physics/grind_contact/families.hpp"

namespace s3::physics::grind_contact::investigator {

using admission::Admission;
using families::Contact;

struct Query {
    std::array<V, 4> board{};
    Admission admission;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t tip_state = 0;
    float truck_to_wheel = 0.0f;
    float deck_to_truck = 0.0f;
    float test_above = 0.0f;
    float test_below = 0.0f;
    float translation = 0.0f;
    float stability_nudge = 0.0f;
    float balance = 0.0f;
    V reference_right{};
    std::uint32_t ground_frames = 0;
    std::uint32_t low_wheel_frames = 0;
    /// Grind permission OR retained70-frame exclusion while published kind!=2.
    bool forbidden = false;
};

struct Proximity {
    TruckContact contact;
    /// Investigator412 bit26: deck/inverted rather than truck contact.
    bool deck_contact = false;
};

struct Investigation {
    std::optional<Contact> candidate;
    /// Only the fallback path sets412 bit27. It does not authorize a grind.
    std::optional<Proximity> proximity;
};

/// `edges` is already the native bounded query result, in its selection order.
Investigation investigate(const Query& query, const std::vector<Primitive>& edges);

// Private in Rust; exposed for the ported unit test.
std::optional<Proximity> fallback(std::array<std::optional<TruckContact>, 2> trucks, std::optional<TruckContact> deck,
                                  std::optional<TruckContact> inverted, std::array<std::optional<TruckContact>, 2> tips);

}  // namespace s3::physics::grind_contact::investigator
