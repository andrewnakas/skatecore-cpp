// Port of skate-core/src/camera/shake_curve.rs.
#include "s3/core/camera/shake_curve.hpp"

namespace s3::camera::shake_curve {

float sample(const F4x4& points, float input) {
    const F4x4 basis = {{
        {-1.0f, 3.0f, -3.0f, 1.0f},
        {3.0f, -6.0f, 3.0f, 0.0f},
        {-3.0f, 3.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
    }};
    const F4x4 coefficients = rs::map(basis, [&](const F4& row) {
        return rs::from_fn<4>([&](std::size_t lane) {
            return rs::mul_add(
                points[3][lane], row[3],
                rs::mul_add(points[2][lane], row[2],
                            rs::mul_add(points[1][lane], row[1], points[0][lane] * row[0])));
        });
    });
    float t = 0.5f;
    for (int iteration = 0; iteration < 10; ++iteration) {
        const float square = t * t;
        auto evaluate = [&](F4 powers, std::size_t lane) {
            return rs::mul_add(
                coefficients[3][lane], powers[3],
                rs::mul_add(coefficients[2][lane], powers[2],
                            rs::mul_add(coefficients[1][lane], powers[1], coefficients[0][lane] * powers[0])));
        };
        const float value = evaluate({square * t, square, t, 1.0f}, 0);
        const float result = evaluate({square * t, square, t, 1.0f}, 1);
        const float derivative = evaluate({square * 3.0f, t * 2.0f, 1.0f, 0.0f}, 0);
        const float residual = input - value;
        if (iteration == 9 ||
            !(rs::abs(residual) > rs::from_bits(0x3c23d70a) && rs::abs(derivative) > rs::from_bits(0x34000000))) {
            return result;
        }
        t = residual / derivative + t;
    }
    S3_PANIC("internal error: entered unreachable code");
}

}  // namespace s3::camera::shake_curve
