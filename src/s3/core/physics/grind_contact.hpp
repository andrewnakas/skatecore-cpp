// Port of skate-core/src/physics/grind_contact.rs.
// Native grind contact leaf82C1E2C8. This is distinct from the tolerant wheel
// triangle query: exact parallel rejection and closed segment/barycentric bounds.
//
// Submodules (each its own header under grind_contact/): admission, arithmetic (private),
// balance, control, entry, families, investigator, manager.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/physics/grind_contact/admission.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::grind_contact {

using V = std::array<float, 4>;
using native_arithmetic::dot3;

/// Native query entry: endpoints plus a spline owner, not three vectors.
struct Primitive {
    V start{};
    V end{};
    std::uint64_t owner = 0;
};

struct TruckContact {
    V position{};
    std::size_t primitive = 0;
};

struct FiftyFiftyCandidate {
    V direction{};
    V centre{};
    V front{};
    V rear{};
    std::size_t primitive = 0;
};

///82C1FB98: the longitudinal deck rectangle. Unlike the two truck rectangles,
///this catches a rail crossing between the trucks (boardslides/lipslides).
std::optional<TruckContact> deck_contact(std::array<V, 4> board, std::uint32_t flags_2484, float above, float below,
                                         float deck_center_to_truck, const std::vector<Primitive>& primitives);

///82D889A8 geometry admission. The state manager owns approach-angle and
///candidate arbitration separately. No nearest-rail substitute is used.
std::optional<FiftyFiftyCandidate> boardslide_candidate(std::array<V, 4> board, TruckContact contact, Primitive edge,
                                                        V velocity, std::uint32_t category, std::int32_t manager_frames,
                                                        std::uint32_t flags_2476, float deck_center_to_truck,
                                                        float truck_to_wheel, const admission::Admission& admission);

///82D89150's same-primitive contact branch, before shared admission82D886B8.
/// Linked/adjacent primitive arbitration belongs to the caller; this boundary
/// explicitly returns none for that case instead of guessing an owner.
std::optional<FiftyFiftyCandidate> fifty_fifty_candidate(std::array<V, 4> board, std::array<float, 2> balance,
                                                         std::array<std::optional<TruckContact>, 2> contacts);

///82D89150 also accepts two segment indices when their hits pass its signed
/// cross-rail separation check. Splines with multiple segments need this path.
std::optional<FiftyFiftyCandidate> fifty_fifty_candidate_on_splines(
    std::array<V, 4> board, std::array<float, 2> balance, std::array<std::optional<TruckContact>, 2> contacts,
    const std::vector<Primitive>& primitives);

///82D370F8: upward-facing normal perpendicular to the spline direction.
V upright_normal(V direction);

///82D88518, common active-grind angular admission. Plane projection occurs
/// before the direction comparison; vertical speed is not an approach angle.
bool within_approach_angle(V direction, V velocity, float degrees);

/// Truck rectangles from82C1FDC0. The geometry dimensions come from the
/// physics_grinds collection (TruckToWheel584, DeckCenterToTruck636).
/// Output order is positive board-forward truck, negative board-forward truck.
/// These are geometric contacts;82D89150 still decides whether to engage.
std::array<std::optional<TruckContact>, 2> truck_contacts(std::array<V, 4> board, std::uint32_t flags_2484,
                                                          float truck_to_wheel, float deck_center_to_truck,
                                                          const std::vector<Primitive>& primitives);

/// Intersect a grind primitive segment with a board/truck probe triangle.
/// Returns the native hit position; acquisition/scoring belongs to the caller.
/// Both windings are accepted. Failed branches do not synthesize a contact.
std::optional<V> segment_triangle(V start, V end, std::array<V, 3> triangle);

// Private helpers of grind_contact.rs, visible to its submodules via `super::*`.
inline V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V add(V a, V b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline V scale(V a, float s) { return V{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}

}  // namespace s3::physics::grind_contact
