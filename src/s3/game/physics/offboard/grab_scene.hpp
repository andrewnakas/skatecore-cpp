// Port of skate-game/src/physics/offboard/grab_scene.rs
//! Stateless offboard grab-scene execution over explicitly authored objects.
//! Ground owns pending/completion, validation and selection; this owns no ticks.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/offboard/grab_scene.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::grab_scene {

namespace native = ::s3::player::offboard::grab_scene;
using native::Descriptor;
using native::Hit;
using native::Line;
using native::Object;
using native::Query;
using native::Record;
using sc::physics::board_world::BoardWorld;

///Explicit collision mesh -> physical assembly relation, not identity guessing.
/// #[derive(Clone, Copy, Debug)]
struct MeshAssembly {
    std::uint32_t mesh = 0;
    std::uint32_t assembly = 0;
};

struct Registry {
    native::Registry objects;
    // Private in Rust.
    std::vector<MeshAssembly> mesh_assemblies;

    static rs::Result<Registry, std::string> new_(const BoardWorld& world, std::vector<Object> objects,
                                                  std::vector<MeshAssembly> mesh_assemblies);
    /// private
    std::optional<std::uint32_t> assembly(std::uint32_t mesh) const;
};

struct Scene {
    // Private in Rust.
    const BoardWorld* world = nullptr;
    const Registry* registry = nullptr;

    static Scene new_(const BoardWorld& world, const Registry& registry) { return Scene{&world, &registry}; }
    rs::Result<std::vector<Record>, std::string> query(const Query& query) const;
    ///8275F8D8 dispatches ONLY descriptor kinds1/2. Resolve the actual authored
    ///spline geometry and CURRENT physical transform, not an old query result.
    rs::Result<std::optional<Record>, std::string> resolve(Descriptor descriptor) const;
    rs::Result<std::optional<Hit>, std::string> line(Line line) const;
    std::optional<std::uint32_t> eligible_object(Hit hit) const;
};

}  // namespace s3::game::physics::offboard::grab_scene

#include "s3/game/physics/offboard/grab_scene/collision.hpp"
