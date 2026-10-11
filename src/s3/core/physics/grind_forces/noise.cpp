// Port of skate-core/src/physics/grind_forces/noise.rs.
#include "s3/core/physics/grind_forces/noise.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_forces::noise {

std::array<float, 3> angles(float speed, float amount, std::array<std::uint32_t, 3> samples) {
    const float speed_factor = rs::min(speed * 0.5f, 1.0f);
    const float amplitude = speed_factor * amount * 0.06f;
    const float width = amplitude * 2.0f;
    std::array<float, 3> out{};
    for (std::size_t i = 0; i < 3; ++i) {
        const float unit = static_cast<float>(samples[i] % 100000u) * 0.00001f;
        out[i] = rs::mul_add(unit, width, -amplitude);
    }
    return out;
}

std::array<V, 4> apply(std::array<V, 4> frame, float speed, float amount,
                       std::array<std::uint32_t, 3> samples) {
    const auto [x, y, z] = angles(speed, amount, samples);
    const auto [sx, cx] = trigonometry::sin_cos(x);
    const auto [sy, cy] = trigonometry::sin_cos(y);
    const auto [sz, cz] = trigonometry::sin_cos(z);
    // Raw standard-VMX words82D407F0=10CC526E and82D407F4=118C4AAE
    // disambiguate IDA's misleading printed multiply/add operand order.
    const float sx_cz = sx * cz;
    const float cx_sz = cx * sz;
    const float cx_cz = cx * cz;
    const float sx_sz = sx * sz;
    const std::array<std::array<float, 3>, 3> columns = {{
        {cy * cz, cy * sz, -sy},
        {sy * sx_cz - cx_sz, rs::mul_add(sy, sx_sz, cx_cz), cy * sx},
        {rs::mul_add(sy, cx_cz, sx_sz), sy * cx_sz - sx_cz, cy * cx},
    }};
    std::array<V, 4> result = frame;
    for (std::size_t column = 0; column < 3; ++column) {
        const auto& v = columns[column];
        for (std::size_t lane = 0; lane < 4; ++lane) {
            result[column][lane] =
                rs::mul_add(v[2], frame[2][lane], rs::mul_add(v[1], frame[1][lane], v[0] * frame[0][lane]));
        }
    }
    // 82D407EC/40810/40828 multiply the three basis vectors by zero
    // and add the original translation. No positional jitter is introduced.
    return result;
}

}  // namespace s3::physics::grind_forces::noise
