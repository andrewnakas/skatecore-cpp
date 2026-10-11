// Port of skate-core/src/physics/contact_feedback/spy.rs.
#include "s3/core/physics/contact_feedback/spy.hpp"

#include "s3/core/rs.hpp"

namespace s3::physics::contact_feedback::spy {

namespace {
std::array<float, 4> vector(Slice<std::uint32_t> words, std::size_t offset) {
    return {rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]), rs::from_bits(words[offset + 2]),
            rs::from_bits(words[offset + 3])};
}
}  // namespace

void spy_contact_jacobians(Slice<std::uint32_t> storage, std::uint32_t contact_count, std::uint32_t& spy_count,
                           float frequency,
                           const std::function<std::array<std::uint32_t, 4>(std::uint32_t)>& body_center) {
    S3_ASSERT(storage.size() >= static_cast<std::size_t>(contact_count) * 64);
    const float squared_frequency = frequency * frequency;
    std::size_t write = 0;
    for (std::size_t index = 0; index < static_cast<std::size_t>(contact_count); ++index) {
        const std::size_t start = index * 64;
        if ((storage[start + 11] & 8u) == 0) {
            continue;
        }
        const auto impulse = vector(storage, start + 20);
        if (!(impulse[0] > 0.0f)) {
            continue;
        }
        const std::uint32_t a = storage[start + 31];
        const std::uint32_t b = storage[start + 43];
        const auto b_center = rs::from_bits(body_center(b));
        const auto a_center = rs::from_bits(body_center(a));
        const auto a_position = vector(storage, start);
        const auto b_position = vector(storage, start + 4);
        const float a_weight = rs::from_bits(storage[start + 35]);
        const float b_weight = rs::from_bits(storage[start + 39]);
        const auto normal = vector(storage, start + 28);
        const auto tangent0 = vector(storage, start + 40);
        const auto tangent1 = vector(storage, start + 52);
        std::array<float, 4> tangent{};
        for (std::size_t i = 0; i < 4; ++i) {
            tangent[i] = rs::mul_add(tangent0[i], impulse[1], tangent1[i] * impulse[2]);
        }
        const float inverse = 1.0f / (b_weight + a_weight);
        std::array<float, 4> position{};
        for (std::size_t i = 0; i < 4; ++i) {
            position[i] = rs::mul_add(a_center[i] + a_position[i], a_weight, (b_center[i] + b_position[i]) * b_weight) *
                          inverse;
        }
        const std::uint32_t tag = storage[start + 55];
        // All source vectors/scalars are loaded before the in-place stores.
        storage[write + 24] = a;
        storage[write + 25] = b;
        storage[write + 26] = tag;
        for (std::size_t i = 0; i < 4; ++i) {
            storage[write + 16 + i] = rs::to_bits((normal[i] * impulse[0]) * squared_frequency);
            storage[write + 20 + i] = rs::to_bits(tangent[i] * squared_frequency);
            storage[write + i] = rs::to_bits(normal[i]);
            storage[write + 4 + i] = rs::to_bits(tangent0[i]);
            storage[write + 8 + i] = rs::to_bits(tangent1[i]);
            storage[write + 12 + i] = rs::to_bits(position[i]);
        }
        spy_count = spy_count + 1u;
        write += 28;
    }
}

}  // namespace s3::physics::contact_feedback::spy
