// Port of skate-core/src/physics/board.rs.
// Physical body indices and stock geometry used by the TU3 board assembly.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/f32_bits.hpp"

namespace s3::physics::board {

inline constexpr std::size_t BODY_COUNT = 7;

enum class BodyId : std::uint8_t {
    RightFrontWheel = 0,
    LeftFrontWheel = 1,
    RightBackWheel = 2,
    LeftBackWheel = 3,
    FrontTruck = 4,
    BackTruck = 5,
    Deck = 6,
};

/// `BodyId::ORDER`.
inline constexpr std::array<BodyId, BODY_COUNT> BodyId_ORDER{
    BodyId::RightFrontWheel, BodyId::LeftFrontWheel, BodyId::RightBackWheel,
    BodyId::LeftBackWheel,   BodyId::FrontTruck,     BodyId::BackTruck,
    BodyId::Deck,
};

/// `BodyId::index(self)`.
constexpr std::size_t index(BodyId id) { return static_cast<std::size_t>(id); }

inline constexpr float RETAIL_WHEEL_RADIUS = f32_from_bits(0x3CFDF3B6u);
inline constexpr float RETAIL_TRUCK_Y_POSITION = f32_from_bits(0xBD676C8Bu);
inline constexpr float RETAIL_TRUCK_Z_POSITION_BACK = f32_from_bits(0xBD54FDF4u);
inline constexpr float RETAIL_TRUCK_Z_POSITION_FRONT = f32_from_bits(0xBD54FDF4u);
inline constexpr float RETAIL_TRUCK_ROTATION_AXIS_ANGLE_DEGREES = f32_from_bits(0x41F80000u);
inline constexpr float RETAIL_DECK_MID_LENGTH = f32_from_bits(0x3F170A3Du);

}  // namespace s3::physics::board
