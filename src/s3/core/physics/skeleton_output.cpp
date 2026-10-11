// Port of skate-core/src/physics/skeleton_output/mod.rs.
#include "s3/core/physics/skeleton_output.hpp"

namespace s3::physics::skeleton_output {

using animation::foot_ik::transforms::inverse_rigid;
using skeleton_animation_record::compose_affine;

rs::Result<rs::Unit, const char*> SkeletonOutput::publish(const Input& input, Slice<Transform> globals,
                                                          Slice<Transform> locals) const {
    bool out_of_range = globals.size() != locals.size();
    for (std::size_t i : bone_indices) out_of_range = out_of_range || i >= globals.size();
    for (std::size_t i : board_bones.all()) out_of_range = out_of_range || i >= locals.size();
    if (out_of_range) {
        return rs::make_err("physical pose output does not match the stock animation hierarchy");
    }
    // 82DB96B8..97E4: volume -> animation bone -> animation space.
    for (std::size_t part = 0; part < 24; ++part) {
        const Transform world = compose_affine(input.physical_parts[part], geometry.inverse_part_frames[part]);
        globals[bone_indices[part]] = compose_affine(input.world_to_animation, world);
    }
    // 82DB97F4..9938 descends through the physical mapping. This inverse
    // is a rigid transpose, distinct from the authored volume inverse.
    for (std::size_t part = 24; part-- > 0;) {
        const std::size_t bone = bone_indices[part];
        if (const auto parent = geometry.parents[part]) {
            locals[bone] = compose_affine(inverse_rigid(globals[bone_indices[*parent]]), globals[bone]);
        } else {
            locals[bone] = globals[bone];
        }
    }
    board::publish(input.board, board_settings, board_bones, locals);
    return rs::Unit{};
}

}  // namespace s3::physics::skeleton_output
