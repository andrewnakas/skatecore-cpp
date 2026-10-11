// Port of the inline #[cfg(test)] mod tests in skate-core/src/physics/mass.rs.
#include "harness.hpp"
#include "s3/core/physics/mass.hpp"

namespace {
using namespace s3::physics::mass;
namespace f32 = s3::rs::f32;
using s3::rs::to_bits;

S3_TEST(both_mass_finalizers_preserve_nan_and_use_volume_only_below_threshold) {
    // A unit cube has volume one. Both original shape paths converge at
    // 82AE79E8/79EC: fallback only when CR6.LT is set by requested mass.
    const MassShape shape = MassShape::RoundedBox{Vector3{0.5f, 0.5f, 0.5f}, 0.0f};
    const MassMoments moments = MassMoments::from_primitive(s3::rs::unwrap(primitive_mass(shape)));
    const std::pair<float, float> cases[] = {{f32::NAN_, f32::NAN_}, {0.0f, 1.0f}, {2.0f, 0.5f}};
    for (const auto& [requested_mass, expected_inverse] : cases) {
        const auto primitive = s3::rs::unwrap(primitive_mass_properties(
            PartMassInput{shape, requested_mass}, RETAIL_UNBOUNDED_VELOCITY, 0.0f));
        const auto aggregate =
            aggregate_mass_properties(moments, requested_mass, RETAIL_UNBOUNDED_VELOCITY, 0.0f);
        for (const auto& body : {primitive, aggregate}) {
            if (s3::rs::is_nan(expected_inverse)) {
                CHECK(s3::rs::is_nan(body.dynamics.inverse_mass));
            } else {
                CHECK_EQ(body.dynamics.inverse_mass, expected_inverse);
            }
        }
    }
}

S3_TEST(stock_simple_parts_reach_recovered_finalizer_words) {
    const auto wheel = retail_wheel_mass_properties().dynamics;
    CHECK_EQ((std::array<std::uint32_t, 5>{
                 to_bits(wheel.inverse_tensor.x), to_bits(wheel.inverse_tensor.y),
                 to_bits(wheel.inverse_tensor.z), to_bits(wheel.inverse_mass), to_bits(wheel.spherical)}),
             (std::array<std::uint32_t, 5>{0x45C3E491u, 0x45C3E491u, 0x45C3E491u, 0x401A3785u,
                                           0x3927466Fu}));

    const auto truck = retail_truck_mass_properties().dynamics;
    CHECK_EQ((std::array<std::uint32_t, 5>{
                 to_bits(truck.inverse_tensor.x), to_bits(truck.inverse_tensor.y),
                 to_bits(truck.inverse_tensor.z), to_bits(truck.inverse_mass), to_bits(truck.spherical)}),
             (std::array<std::uint32_t, 5>{0x438EE68Eu, 0x438EE68Eu, 0x46904D8Bu, 0x3F034835u,
                                           0x3B654E66u}));
}

S3_TEST(deck_frame_override_retains_geometry_driven_inertia) {
    const DeckGeometry stock = DeckGeometry::new_(DeckGeometrySettings::STOCK);
    const auto deck = deck_mass_properties(stock, 6.0f, 27.0f);
    CHECK_EQ(deck.local_mass_frame, RetailLocalMassFrame::IDENTITY);
    CHECK_EQ(deck.dynamics.inverse_mass, 1.0f / 6.0f);
    const auto doubled_mass = deck_mass_properties(stock, 12.0f, 27.0f);
    const std::pair<float, float> pairs[] = {
        {deck.dynamics.inverse_tensor.x, doubled_mass.dynamics.inverse_tensor.x},
        {deck.dynamics.inverse_tensor.y, doubled_mass.dynamics.inverse_tensor.y},
        {deck.dynamics.inverse_tensor.z, doubled_mass.dynamics.inverse_tensor.z},
    };
    for (const auto& [one, two] : pairs) {
        CHECK(s3::rs::abs(one - 2.0f * two) < 1e-5f);
    }
    DeckGeometrySettings wider = DeckGeometrySettings::STOCK;
    wider.width *= 1.5f;
    const auto changed = deck_mass_properties(DeckGeometry::new_(wider), 6.0f, 27.0f);
    CHECK_NE(deck.dynamics.inverse_tensor, changed.dynamics.inverse_tensor);
}
}  // namespace
