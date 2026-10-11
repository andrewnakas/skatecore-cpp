// Port of skate-game/src/physics/grind_names.rs.
#include "s3/game/physics/grind_names.hpp"

namespace s3::game::physics::grind_names {

std::optional<Name> lookup(std::array<std::uint32_t, 6> c) {
    constexpr std::array<std::uint32_t, 6> bounds = {2, 2, 2, 2, 4, 6};
    for (std::size_t k = 0; k < 6; ++k) {
        if (c[k] >= bounds[k]) return std::nullopt;
    }
    const std::size_t i =
        static_cast<std::size_t>((((((c[0] * 2u + c[1]) * 2u + c[2]) * 2u + c[3]) * 4u + c[4]) * 6u + c[5]));
    const std::array<Name, 48>* table = nullptr;
    switch (i / 48) {
        case 0: table = &table0::TABLE; break;
        case 1: table = &table1::TABLE; break;
        case 2: table = &table2::TABLE; break;
        case 3: table = &table3::TABLE; break;
        case 4: table = &table4::TABLE; break;
        case 5: table = &table5::TABLE; break;
        case 6: table = &table6::TABLE; break;
        default: table = &table7::TABLE; break;
    }
    return (*table)[i % 48];
}

}  // namespace s3::game::physics::grind_names
