// Port of skate-core/src/physics/contact_feedback/collision_info.rs.
#include "s3/core/physics/contact_feedback/collision_info.hpp"

#include <algorithm>

namespace s3::physics::contact_feedback::collision_info {

namespace {
std::uint32_t word(const CollisionInfo& bytes, std::size_t offset) {
    return (std::uint32_t(bytes.at(offset)) << 24) | (std::uint32_t(bytes.at(offset + 1)) << 16) |
           (std::uint32_t(bytes.at(offset + 2)) << 8) | std::uint32_t(bytes.at(offset + 3));
}

void put(CollisionInfo& bytes, std::size_t offset, std::uint32_t value) {
    const auto b = rs::to_be_bytes_u32(value);
    for (std::size_t i = 0; i < 4; ++i) bytes.at(offset + i) = b[i];
}

void fill(CollisionInfo& bytes, std::size_t start, std::size_t end, std::uint8_t value) {
    S3_ASSERT(start <= end && end <= bytes.size());
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(start), bytes.begin() + static_cast<std::ptrdiff_t>(end),
              value);
}

void up(CollisionInfo& bytes, std::size_t offset) {
    fill(bytes, offset, offset + 16, 0);
    put(bytes, offset + 4, 0x3f800000u);
}
}  // namespace

void reset_frame_collision_info(CollisionInfo& info) {
    up(info, 0);
    for (std::size_t i = 0; i < 7; ++i) {
        up(info, 32 + i * 16);
        for (std::size_t offset : {144u, 368u, 480u}) {
            fill(info, offset + i * 16, offset + i * 16 + 16, 0);
        }
        info.at(780 + i) = 0;
    }
    for (std::size_t i = 4; i < 7; ++i) {
        for (std::size_t offset : {692u, 720u, 748u}) {
            put(info, offset + i * 4, 0);
        }
    }
    fill(info, 592, 608, 0);
    fill(info, 688, 692, 1);
    for (std::size_t offset : {776u, 788u, 796u, 800u}) {
        put(info, offset, 0);
    }
    info[804] = 0;
    info[805] = 0;
    const std::uint32_t flags = word(info, 808) & 0x01ffffffu;
    put(info, 808, flags);
}

void choose_physics_surface(CollisionInfo& info) {
    const auto surfaces = rs::from_fn<4>([&](std::size_t i) { return word(info, 692 + i * 4); });
    const auto contacts = rs::from_fn<4>([&](std::size_t i) { return info[780 + i] != 0; });
    const std::uint32_t result = choose_surface(surfaces, contacts, (word(info, 808) & 0x02000000u) != 0);
    put(info, 776, result);
}

std::uint32_t choose_surface(std::array<std::uint32_t, 4> surfaces, std::array<bool, 4> contacts,
                             bool forced_twelve) {
    std::array<std::uint32_t, 16> votes{};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t surface = surfaces[i];
        if (surface != 0) {
            if (!(surface < 16)) S3_PANIC("native surface histogram index exceeded");
            votes[surface] += contacts[i] ? 4u : 1u;
        }
    }
    std::uint32_t winner = 1;
    std::uint32_t maximum = 0;
    for (std::size_t surface = 1; surface < 14; ++surface) {
        const std::uint32_t count = votes[surface];
        if (count > maximum) {
            maximum = count;
            winner = static_cast<std::uint32_t>(surface);
        }
    }
    return forced_twelve ? 12u : winner;
}

}  // namespace s3::physics::contact_feedback::collision_info
