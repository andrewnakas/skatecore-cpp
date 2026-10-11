// Port of skate-game/src/physics/revert_state.rs.
//! RevertGround102, TU3 vtable82327330: Enter82D43488, Update82D43518,
//! empty Exit, Ground Post82D4C070, Fill82D43B10. Stock layout018E8A2E5028AB3F.
#pragma once
#include <array>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::revert_state {

using V = std::array<float, 4>;
using sc::point_graph::PointGraph;

struct RevertState {
    PointGraph<8> speed{};
    PointGraph<4> tolerance{};
    float gain = 0.0f;
    float delay = 0.0f;
    float duration = 0.0f;
    float direction = 0.0f;
    float travel_sign = 0.0f;
    V velocity{};
    float elapsed = 0.0f;
    bool captured = false;
    bool active = false;

    static rs::Result<RevertState, std::string> load(const sd::Collections& d);
    ///82D43800: directed remaining angle, stock speed curve and rate damping.
    /// (Private in Rust.)
    V correction(V forward, V normal, V angular);
};

/// `pub(super)`.
Status enter(GamePhysics& physics, SkaterRuntime& skater);
/// `pub(super)`.
Status update(GamePhysics& physics, SkaterRuntime& skater);

/// Private helpers.
float dot(V a, V b);
sc::math::Vector3 xyz(V v);

}  // namespace s3::game::physics::revert_state
