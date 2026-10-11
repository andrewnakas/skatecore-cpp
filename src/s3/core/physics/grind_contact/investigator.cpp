// Port of skate-core/src/physics/grind_contact/investigator.rs.
#include "s3/core/physics/grind_contact/investigator.hpp"

namespace s3::physics::grind_contact::investigator {

Investigation investigate(const Query& query, const std::vector<Primitive>& edges) {
    if (edges.empty()) {
        return Investigation{};
    }
    const Query& q = query;
    const Admission& a = q.admission;
    const auto trucks = truck_contacts(q.board, q.flags_2484, q.truck_to_wheel, q.deck_to_truck, edges);
    const auto tips = families::tip_contacts(q.board, q.flags_2484, q.tip_state, q.flags_2468, q.deck_to_truck, edges);
    const auto deck = deck_contact(q.board, q.flags_2484, q.test_above, q.test_below, q.deck_to_truck, edges);
    const auto inverted = families::inverted_contact(q.board, q.flags_2484, q.test_above, q.test_below, edges);

    if (!q.forbidden) {
        auto fifty = [&]() -> std::optional<Contact> {
            const auto geometry =
                fifty_fifty_candidate_on_splines(q.board, {q.translation, q.stability_nudge}, trucks, edges);
            if (!geometry) return std::nullopt;
            const auto decision = a.test(401, geometry->direction, false);
            if (!decision.allowed) return std::nullopt;
            return Contact{*geometry, false, 0, decision.kind};
        };
        auto five = [&]() {
            return families::five_o(q.board, trucks, edges, a.velocity, q.flags_2476, q.flags_2472, q.ground_frames,
                                    q.translation, a);
        };
        auto tip = [&]() {
            return families::tipslide(q.board, tips, edges, a.velocity, a.category, q.flags_2476, q.flags_2472,
                                      q.ground_frames, q.balance, q.reference_right, a);
        };
        auto slide = [&]() -> std::optional<Contact> {
            if (!deck) return std::nullopt;
            const TruckContact hit = *deck;
            const auto geometry = boardslide_candidate(q.board, hit, edges.at(hit.primitive), a.velocity, a.category,
                                                       static_cast<std::int32_t>(q.low_wheel_frames), q.flags_2476,
                                                       q.deck_to_truck, q.truck_to_wheel, a);
            if (!geometry) return std::nullopt;
            return Contact{*geometry, false, 1, a.test(400, geometry->direction, false).kind};
        };
        auto dark = [&]() -> std::optional<Contact> {
            if (!inverted) return std::nullopt;
            const TruckContact hit = *inverted;
            return families::darkslide(q.board, hit, edges.at(hit.primitive), a.velocity, a.category,
                                       q.low_wheel_frames, a);
        };
        // The original retries the truck family as 50-50 first, even in403.
        std::optional<Contact> retained;
        switch (a.state) {
            case 401:
            case 403:
                retained = fifty();
                if (!retained) retained = five();
                break;
            case 402:
            case 404:
                retained = tip();
                break;
            case 400:
                retained = slide();
                break;
            case 405:
                retained = dark();
                break;
            default:
                break;
        }
        std::optional<Contact> candidate = retained;
        if (!candidate) candidate = fifty();
        if (!candidate) candidate = five();
        if (!candidate) candidate = tip();
        if (!candidate) candidate = slide();
        if (!candidate) candidate = dark();
        if (candidate) {
            return Investigation{candidate, std::nullopt};
        }
    }
    //82D87C5C..82D87E60: keep contact-only metadata even when entry is denied.
    return Investigation{std::nullopt, fallback(trucks, deck, inverted, tips)};
}

std::optional<Proximity> fallback(std::array<std::optional<TruckContact>, 2> trucks, std::optional<TruckContact> deck,
                                  std::optional<TruckContact> inverted, std::array<std::optional<TruckContact>, 2> tips) {
    //82D87D24 short-circuits: v82 becomes1 only on an actual deck hit.
    if (trucks[0]) return Proximity{*trucks[0], false};
    if (trucks[1]) return Proximity{*trucks[1], false};
    if (deck) return Proximity{*deck, true};
    if (inverted) return Proximity{*inverted, false};
    if (tips[0]) return Proximity{*tips[0], false};
    if (tips[1]) return Proximity{*tips[1], false};
    return std::nullopt;
}

}  // namespace s3::physics::grind_contact::investigator
