// Port of skate-core/src/physics/grind_contact/families.rs.
// Additional TU3 deck/truck contact queries and family admission leaves.
#pragma once
#include "s3/core/physics/grind_contact.hpp"

namespace s3::physics::grind_contact::families {

using admission::Admission;
using admission::EntryKind;

struct Contact {
    FiftyFiftyCandidate geometry;
    bool front = false;
    std::uint32_t kind = 0;
    EntryKind entry_kind = EntryKind::RideFromAbove;
};

///82C1F968: inverted deck only, .3 longitudinal half-length, inverted depth.
std::optional<TruckContact> inverted_contact(std::array<V, 4> board, std::uint32_t flags, float epsilon, float depth,
                                             const std::vector<Primitive>& edges);

///82C20130: longitudinal trapezoids beyond each truck, with the native
///current-tip suppression so the loaded end does not swap on each tick.
std::array<std::optional<TruckContact>, 2> tip_contacts(std::array<V, 4> board, std::uint32_t flags_2484,
                                                        std::uint32_t state, std::uint32_t flags_2468,
                                                        float truck_distance, const std::vector<Primitive>& edges);

///82D883E0: a tip on a height-discontinuous edge becomes a backslash when
///the deck lies over the higher side. Existing tipslides remain tipslides.
bool is_backslash(V board_position, V point, std::array<V, 2> far_points, V support, float truck_distance,
                  std::uint32_t state);

///82D88E88. Unlike boardslides, inverted decks have no truck-clearance test.
std::optional<Contact> darkslide(std::array<V, 4> board, TruckContact hit, Primitive edge, V velocity,
                                 std::uint32_t category, std::uint32_t low_wheel_frames, const Admission& admission);

///82D89898/82D89820: valid single-truck contacts. Selection between two valid
///ends is performed below using the native travel-relative branches.
std::optional<Contact> five_o(std::array<V, 4> board, std::array<std::optional<TruckContact>, 2> hits,
                              const std::vector<Primitive>& edges, V velocity, std::uint32_t flags_2476,
                              std::uint32_t flags_2472, std::uint32_t ground_frames, float translation,
                              const Admission& admission);

///82D89428 selects the positive tip first, then checks penetration, descent,
///cross-rail velocity and the animation's lateral balance intent.
std::optional<Contact> tipslide(std::array<V, 4> board, std::array<std::optional<TruckContact>, 2> hits,
                                const std::vector<Primitive>& edges, V velocity, std::uint32_t category,
                                std::uint32_t flags_2476, std::uint32_t flags_2472, std::uint32_t ground_frames,
                                float balance, V reference_right, const Admission& admission);

}  // namespace s3::physics::grind_contact::families
