// Port of skate-game/src/physics/boneless.rs.
//! Physical state602, TU3 vtable82327434. Stock no-comply/boneless/fastplant
//! animations select this state with AnimSkateboard and launch with FootJump.
#pragma once
#include <array>
#include <cstddef>
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::boneless {

struct Boneless {
    std::size_t toe = 0;            //52
    std::array<float, 4> anchor{};  //64
    bool right = false;             //80
    std::array<sc::point_graph::PointGraph<8>, 4> curves{};

    static rs::Result<Boneless, std::string> load(const sd::Collections& data);
};

///82D4C900: anchor the last completed physical toe position.
Status enter(GamePhysics& physics, SkaterRuntime& skater);
///82D4C9B8. Authored FootJump controls every launch request; no host timer.
Status update(GamePhysics& physics, SkaterRuntime& skater);
///82D4CB18/82D4CC18, four PointNegGraphData8 values from the stock schema. (Private in Rust.)
Status launch(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::boneless
