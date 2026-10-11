// Port of skate-core/src/physics/grind_contact/investigator_tests.rs.
#include "harness.hpp"
#include "s3/core/physics/grind_contact/investigator.hpp"

namespace {
using namespace s3::physics::grind_contact;
using namespace s3::physics::grind_contact::investigator;
using Opt = std::optional<TruckContact>;

Opt hit(std::size_t primitive) { return TruckContact{{0.0f, 0.0f, 0.0f, 0.0f}, primitive}; }

S3_TEST(fallback_keeps_native_contact_order_and_classifies_only_ordinary_deck) {
    auto result = s3::rs::unwrap(fallback({hit(1), hit(2)}, hit(3), hit(4), {hit(5), hit(6)}));
    CHECK_EQ(result.contact.primitive, std::size_t(1));
    CHECK(!result.deck_contact);
    result = s3::rs::unwrap(fallback({Opt{}, Opt{}}, hit(3), hit(4), {hit(5), hit(6)}));
    CHECK_EQ(result.contact.primitive, std::size_t(3));
    CHECK(result.deck_contact);
    result = s3::rs::unwrap(fallback({Opt{}, Opt{}}, Opt{}, hit(4), {hit(5), hit(6)}));
    CHECK_EQ(result.contact.primitive, std::size_t(4));
    CHECK(!result.deck_contact);
    result = s3::rs::unwrap(fallback({Opt{}, Opt{}}, Opt{}, Opt{}, {Opt{}, hit(6)}));
    CHECK_EQ(result.contact.primitive, std::size_t(6));
    CHECK(!result.deck_contact);
    CHECK(!fallback({Opt{}, Opt{}}, Opt{}, Opt{}, {Opt{}, Opt{}}).has_value());
}
}  // namespace
