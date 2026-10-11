// Port of the #[cfg(test)] mod tests in skate-core/src/animation/foot_ik/post_contact.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/animation/foot_ik/post_contact.hpp"

using s3::animation::foot_ik::post_contact::tangent;

S3_TEST(original_tangent_rational_spans_both_constructor_quadrants) {
    // Independent mathematical values test argument reduction and rational
    // operand order, without claiming original hardware estimate parity.
    for (float angle : {0.01f, 0.25f, 0.75f, 1.0f, 1.5607964f}) {
        const float actual = tangent(angle);
        const float reference = static_cast<float>(std::tan(static_cast<double>(angle)));
        CHECK_MSG(s3::rs::abs(actual - reference) < 3e-5f * s3::rs::max(s3::rs::abs(reference), 1.0f),
                  angle << ": " << actual << " vs " << reference);
    }
}
