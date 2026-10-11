// Port of skate-core/src/physics/startup/skateboard.rs.
#include "s3/core/physics/startup/skateboard.hpp"

#include <algorithm>

#include "s3/core/rs.hpp"

namespace s3::physics::startup::skateboard {

namespace {
template <std::size_t N>
void put(std::array<std::uint8_t, N>& bytes, std::size_t offset, std::uint32_t value) {
    const auto b = rs::to_be_bytes_u32(value);
    for (std::size_t i = 0; i < 4; ++i) bytes.at(offset + i) = b[i];
}
template <std::size_t N>
void fill(std::array<std::uint8_t, N>& bytes, std::size_t start, std::size_t end) {
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(start), bytes.begin() + static_cast<std::ptrdiff_t>(end),
              std::uint8_t(0));
}
}  // namespace

std::uint32_t reset_skateboard(std::array<std::uint8_t, 1360>& wrapper, std::uint32_t wrapper_base,
                               std::array<std::uint8_t, 8400>& body_state, std::array<PartPose, 7>& parts,
                               PartPose& hook, std::array<PoseMatrix, 7> authored, PoseMatrix requested,
                               std::uint32_t processed_flags_2468,
                               const std::array<std::array<std::uint32_t, 4>, 7>& gravity) {
    for (const auto& part : parts) S3_ASSERT(part.body.has_value());
    for (std::size_t i : {6u, 0u, 1u, 2u, 3u, 4u, 5u}) {
        board_pose::set_part_transform(parts[i], authored[i]);
    }
    if ((processed_flags_2468 & 0x00100000u) != 0) {
        for (std::size_t i : {0u, 1u, 2u, 3u, 8u, 9u, 10u, 11u}) {
            requested[i] ^= 0x80000000u;
        }
    }
    board_pose::set_board_transform(parts, hook, requested);
    std::array<std::array<std::uint32_t, 44>, 7> bodies{};
    for (std::size_t i = 0; i < 7; ++i) bodies[i] = rs::unwrap(parts[i].body);
    reset::reset_board_body(body_state, bodies, gravity);
    for (std::size_t i = 0; i < 7; ++i) parts[i].body = bodies[i];
    put(wrapper, 256, 0);
    put(body_state, 7680, 0);
    put(body_state, 7684, 0);
    fill(wrapper, 240, 256);
    put(wrapper, 252, 0x3f800000u);
    wrapper[290] = 0;
    fill(body_state, 960, 992);
    put(body_state, 972, 2);
    put(body_state, 988, 2);
    put(wrapper, 324, wrapper_base + 352u);
    put(wrapper, 320, wrapper_base + 352u);
    put(wrapper, 328, wrapper_base + 1360u);
    put(wrapper, 260, 0);
    fill(wrapper, 192, 208);
    put(wrapper, 196, 0x3f800000u);
    wrapper[291] = 0;
    return 240;
}

}  // namespace s3::physics::startup::skateboard
