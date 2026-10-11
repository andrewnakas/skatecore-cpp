// Port of skate-core/src/physics/startup/reset.rs.
#include "s3/core/physics/startup/reset.hpp"

#include <algorithm>

#include "s3/core/rs.hpp"

namespace s3::physics::startup::reset {

namespace {
std::uint32_t word(const std::uint8_t* bytes, std::size_t offset) {
    return (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
           (std::uint32_t(bytes[offset + 2]) << 8) | std::uint32_t(bytes[offset + 3]);
}
void put(std::uint8_t* bytes, std::size_t offset, std::uint32_t value) {
    const auto b = rs::to_be_bytes_u32(value);
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = b[i];
}
void up(std::uint8_t* bytes, std::size_t offset) {
    std::fill(bytes + offset, bytes + offset + 16, std::uint8_t(0));
    put(bytes, offset + 4, 0x3f800000u);
}
}  // namespace

void reset_collision_info(std::uint8_t* info) {
    up(info, 0);
    up(info, 16);
    for (std::size_t i = 0; i < 7; ++i) {
        up(info, 32 + i * 16);
        for (std::size_t offset : {144u, 256u, 368u, 480u}) {
            std::fill(info + offset + i * 16, info + offset + i * 16 + 16, std::uint8_t(0));
        }
        for (std::size_t offset : {692u, 720u, 748u}) {
            put(info, offset + i * 4, 0);
        }
        info[780 + i] = 0;
    }
    std::fill(info + 592, info + 608, std::uint8_t(0));
    for (std::size_t i = 0; i < 4; ++i) {
        up(info, 608 + i * 16);
        put(info, 672 + i * 4, 0);
        info[688 + i] = 0;
    }
    for (std::size_t offset : {776u, 788u, 792u, 796u, 800u}) {
        put(info, offset, 0);
    }
    info[804] = 0;
    info[805] = 0;
    const std::uint32_t flags = word(info, 808) & 0x01ffffffu;
    put(info, 808, flags);
}

std::uint8_t reset_board_body(std::array<std::uint8_t, 8400>& board, Slice<std::array<std::uint32_t, 44>> bodies,
                              Slice<const std::array<std::uint32_t, 4>> simulation_gravity) {
    S3_ASSERT(bodies.size() == simulation_gravity.size());
    for (std::size_t k = 0; k < bodies.size(); ++k) {
        auto& body = bodies[k];
        const auto& gravity = simulation_gravity[k];
        std::fill(body.begin() + 8, body.begin() + 11, 0u);
        std::fill(body.begin() + 12, body.begin() + 15, 0u);
        for (std::size_t i = 0; i < 3; ++i) body[36 + i] = gravity[i];
        std::fill(body.begin() + 40, body.begin() + 43, 0u);
    }
    std::uint8_t* b = board.data();
    put(b, 7680, 0);
    put(b, 7684, 0);
    reset_collision_info(b + 64);
    const std::uint8_t old_flags = board[8384];
    board[8384] &= 0x3f;
    for (std::size_t offset : {7968u, 8032u}) {
        std::fill(b + offset, b + offset + 52, std::uint8_t(0));
        board[offset + 52] = 0;
    }
    std::fill(b + 1008, b + 1044, std::uint8_t(0));
    board[1044] = 0;
    return old_flags;
}

}  // namespace s3::physics::startup::reset
