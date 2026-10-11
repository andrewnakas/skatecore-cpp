// Port of simref sim/src/camera/stock_names.rs.
#include "s3/game/camera/stock_names.hpp"

namespace s3::game::camera::stock_names {
namespace {

std::uint64_t le64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

void mix(std::uint64_t& a, std::uint64_t& b, std::uint64_t& c) {
    a = (a - b - c) ^ (c >> 43);
    b = (b - c - a) ^ (a << 9);
    c = (c - a - b) ^ (b >> 8);
    a = (a - b - c) ^ (c >> 38);
    b = (b - c - a) ^ (a << 23);
    c = (c - a - b) ^ (b >> 5);
    a = (a - b - c) ^ (c >> 35);
    b = (b - c - a) ^ (a << 49);
    c = (c - a - b) ^ (b >> 11);
    a = (a - b - c) ^ (c >> 12);
    b = (b - c - a) ^ (a << 18);
    c = (c - a - b) ^ (b >> 22);
}

}  // namespace

std::uint64_t lookup8(const std::uint8_t* bytes, std::size_t size) {
    if (size == 0) {
        return 0;
    }
    std::uint64_t a = 0xabcdef0011223344ull;
    std::uint64_t b = a;
    std::uint64_t c = 0x9e3779b97f4a7c13ull;
    const std::size_t blocks = size / 24;
    for (std::size_t k = 0; k < blocks; ++k) {
        const std::uint8_t* block = bytes + 24 * k;
        a += le64(block);
        b += le64(block + 8);
        c += le64(block + 16);
        mix(a, b, c);
    }
    c += std::uint64_t(size);
    const std::uint8_t* rest = bytes + 24 * blocks;
    const std::size_t remainder = size - 24 * blocks;
    for (std::size_t i = 0; i < remainder; ++i) {
        const std::uint64_t byte = rest[i];
        if (i <= 7) {
            a += byte << (8 * i);
        } else if (i <= 15) {
            b += byte << (8 * (i - 8));
        } else {
            c += byte << (8 * (i - 15));
        }
    }
    mix(a, b, c);
    return c;
}

}  // namespace s3::game::camera::stock_names
