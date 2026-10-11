// Port of skate-game/src/physics/offboard/grab_scene.rs
#include "s3/game/physics/offboard/grab_scene.hpp"

namespace s3::game::physics::offboard::grab_scene {

rs::Result<Registry, std::string> Registry::new_(const BoardWorld& world, std::vector<Object> objects,
                                                 std::vector<MeshAssembly> mesh_assemblies) {
    auto metadata_r = world.query_metadata();
    if (metadata_r.is_err()) return rs::make_err(std::string(metadata_r.unwrap_err()));
    const auto* metadata = metadata_r.unwrap();
    auto objects_r = native::Registry::new_(std::move(objects));
    if (objects_r.is_err()) return rs::make_err(std::string(objects_r.unwrap_err()));
    for (std::size_t index = 0; index < mesh_assemblies.size(); ++index) {
        const auto& binding = mesh_assemblies[index];
        bool exists = false;
        for (const auto& m : metadata->meshes) exists = exists || m.geometry == binding.mesh;
        if (binding.assembly == 0 || !exists) {
            return rs::make_err(
                std::string("Grab collision binding must name an existing mesh and nonzero physical assembly"));
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (mesh_assemblies[prior].mesh == binding.mesh) {
                return rs::make_err(std::string("One canonical collision mesh cannot name two assemblies"));
            }
        }
    }
    return Registry{std::move(objects_r.unwrap()), std::move(mesh_assemblies)};
}

std::optional<std::uint32_t> Registry::assembly(std::uint32_t mesh) const {
    for (const auto& b : mesh_assemblies) {
        if (b.mesh == mesh) return b.assembly;
    }
    return std::nullopt;
}

rs::Result<std::vector<Record>, std::string> Scene::query(const Query& query) const {
    auto r = native::query(registry->objects, query);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return r.unwrap();
}

rs::Result<std::optional<Record>, std::string> Scene::resolve(Descriptor descriptor) const {
    if (!(descriptor.kind == 1 || descriptor.kind == 2)) return std::optional<Record>{};
    for (const auto& object : registry->objects.objects) {
        for (const auto& spline : object.splines) {
            if (spline.descriptor == descriptor) {
                auto r = object.record(spline);
                if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
                return std::optional<Record>(r.unwrap());
            }
        }
    }
    return std::optional<Record>{};
}

rs::Result<std::optional<Hit>, std::string> Scene::line(Line line) const { return collision::line(*this, line); }

std::optional<std::uint32_t> Scene::eligible_object(Hit hit) const { return registry->objects.eligible_object(hit); }

}  // namespace s3::game::physics::offboard::grab_scene
