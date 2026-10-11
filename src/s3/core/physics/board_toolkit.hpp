// Port of skate-core/src/physics/board_toolkit.rs.
// Skateboard::PrepareBoardToolkit82C013F0, TU3. Derived physical axes and
// filtered normal are calculated from the same live deck used by the solver.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/slice.hpp"

namespace s3::physics::board_runtime {
class BoardRuntime;
}

namespace s3::physics::board_toolkit {

using V = std::array<float, 4>;
using M = std::array<V, 4>;

struct BoardToolkit {
    M deck{};
    M effective{};
    M inverse_effective{};
    V side{};
    V up{};
    V forward{};
    V horizontal_forward{};
    V transverse_up{};
    V forward_velocity{};
    V travel_direction{};
    V filtered_normal{};
    float absolute_speed = 0.0f;
    float control_sign = 0.0f;
    float total_mass = 0.0f;

    /// Board192 is retained by the owner and replaced by filtered_normal after
    /// this call. Reset must supply the recovered retained normal, not derive
    /// grounded state from board height.
    static BoardToolkit from_board(const board_runtime::BoardRuntime& board, std::uint32_t flags_2468,
                                   float speed_2612, V ground_normal_464, V retained_normal);
    static BoardToolkit calculate(M deck, Slice<const float> inverse_masses, std::uint32_t flags, float speed,
                                  V normal, V retained_normal);
};

}  // namespace s3::physics::board_toolkit
