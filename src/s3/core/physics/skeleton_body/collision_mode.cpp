// Port of skate-core/src/physics/skeleton_body/collision_mode.rs.
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::collision_mode {

using rigid_body::world_inverse_inertia;

namespace {
std::array<std::array<bool, PART_COUNT>, PART_COUNT> all_true() {
    std::array<std::array<bool, PART_COUNT>, PART_COUNT> t{};
    for (auto& row : t) row.fill(true);
    return t;
}
}  // namespace

void SkeletonCollisionMode::disable_handplant_contacts(std::uint32_t frames) {
    pending_reenable = true;
    for (std::size_t part : {7, 3, 8, 4, 1}) {
        disable_count[part] = frames;
        parts[part].enabled = false;
    }
}

void SkeletonCollisionMode::reset_body_state(SkeletonCollisionFeedback& feedback) {
    is_ragdoll = false;
    feedback.reset();
    disable_count.fill(0);
    pending_reenable = false;
}

SkeletonCollisionMode SkeletonCollisionMode::new_normal(SkeletonCollisionSettings settings,
                                                        bool cull_all_self_pairs) {
    //82BE5130..5164 assigns this zero material to parts0,24,25.
    const RetailContactMaterial zero{0.0f, 0.0f, 0.0f};
    SkeletonCollisionMode result;
    for (std::size_t part = 0; part < PART_COUNT; ++part) {
        result.parts[part] = SkeletonPartCollision{
            false,
            part == 0 ? 4u : 0u,
            5,
            (part >= 1 && part < 24) ? settings.normal_material : zero,
        };
    }
    result.disable_count.fill(0);
    result.pending_reenable = false;  //82BE4660, following the24 counter clears.
    result.assembly_group = 5;
    result.partial_ragdoll = false;
    result.is_ragdoll = false;
    result.settings = settings;
    result.self_culling = all_true();
    result.normal_self_culling = all_true();
    if (!cull_all_self_pairs) {
        const std::size_t pairs[6][2] = {{14, 4}, {14, 8}, {23, 4}, {23, 8}, {4, 18}, {8, 22}};
        for (const auto& pair : pairs) {
            result.self_culling[pair[0]][pair[1]] = false;
            result.self_culling[pair[1]][pair[0]] = false;
        }
    }
    if (settings.enabled) {
        result.normal_collision();
    } else {
        result.disable_all(true);
    }
    result.normal_self_culling = result.self_culling;
    return result;
}

rs::Result<rs::Unit, const char*> SkeletonCollisionMode::select_driven(std::uint32_t mode) {
    switch (mode) {
        case 3:
            select_biped();
            return rs::Unit{};
        case 5:
        case 6:
            normal_collision();
            break;
        case 1:
            //82BE3808 enables eligible bones before switching the group.
            enable_eligible_bones();
            set_group(3);
            break;
        default:
            return rs::make_err("Skeleton collision selector requires its non-driven physical setup");
    }
    partial_ragdoll = false;
    disable_root();
    parts[24].enabled = false;
    parts[25].enabled = false;
    return rs::Unit{};
}

void SkeletonCollisionMode::normal_collision() {
    set_group(5);
    disable_root();
    enable_eligible_bones();
}

void SkeletonCollisionMode::disable_all(bool clear_counts) {
    disable_root();
    parts[24].enabled = false;
    parts[25].enabled = false;
    for (std::size_t part = 1; part < 24; ++part) {
        parts[part].enabled = false;
        if (clear_counts) {
            disable_count[part] = 0;
        }
    }
}

void SkeletonCollisionMode::enable_bone(std::size_t part) {
    if (disable_count[part] == 0) {
        parts[part].enabled = true;
        parts[part].volume_group = 0;
    }
    parts[part].part_group = 5;
}

void SkeletonCollisionMode::finish_contact_frame() {
    if (!pending_reenable) {
        return;
    }
    bool remaining = false;
    for (std::size_t part = 0; part < ANIMATION_PART_COUNT; ++part) {
        if (static_cast<std::int32_t>(disable_count[part]) > 0) {
            disable_count[part] -= 1;
            const bool enabled = disable_count[part] == 0;
            parts[part].enabled = enabled;
            remaining |= !enabled;
        }
    }
    pending_reenable = remaining;
}

void SkeletonCollisionMode::normal_bone(std::size_t part, bool has_collision) {
    if (has_collision) {
        if (disable_count[part] == 0) {
            parts[part].enabled = true;
            parts[part].volume_group = 0;
        }
    } else {
        parts[part].enabled = false;
        parts[part].volume_group = 4;
        disable_count[part] = 0;
    }
}

void SkeletonCollisionMode::restore_normal_properties(SkeletonBody& skeleton) {
    self_culling = normal_self_culling;
    set_group(5);
    for (std::size_t part = 0; part < PART_COUNT; ++part) {
        const SkeletonPart animated = skeleton.definition.parts[part];
        auto& body = skeleton.bodies_mut()[part];
        body.inertia.linear_drag = 0.0f;
        body.inertia.angular_drag = 0.0f;
        if (part >= 1 && part < 24) {
            body.inertia.inverse_mass = animated.inverse_mass_animated;
            body.inertia.inverse_tensor = animated.animated.dynamics.inverse_tensor;
            const auto tensor = body.inertia.inverse_tensor;
            float minimum = tensor.x;
            if (minimum >= tensor.y) {
                minimum = tensor.y;
            }
            if (minimum >= tensor.z) {
                minimum = tensor.z;
            }
            body.inertia.spherical = 1.0f / minimum;
            body.rates.world_inverse_inertia = world_inverse_inertia(body.rates.basis, tensor);
            parts[part].material = settings.normal_material;
        }
    }
    if (settings.enabled) {
        normal_collision();
    } else {
        disable_all(true);
    }
    is_ragdoll = false;
}

void SkeletonCollisionMode::apply_ragdoll_properties(SkeletonBody& skeleton, bool inverse_mass,
                                                     bool inverse_inertia, std::array<float, 2> drag,
                                                     std::array<RetailContactMaterial, 2> materials) {
    //Exact raw82BE8900..A664 stores, not the asymmetric decompiler output.
    static constexpr std::uint32_t CULLED[PART_COUNT] = {
        0x03FFFFFF, 0x03FFF447, 0x03FFFC47, 0x039B8479, 0x03BB8479, 0x039F8479, 0x03FFFFFF,
        0x03B987C1, 0x03BB87C1, 0x03F987C1, 0x03FFFFFF, 0x03FFFC45, 0x03FFFC47, 0x03FFFC47,
        0x03FFFC47, 0x03FFFFFF, 0x038FFFFF, 0x038FFD7F, 0x03CFFC67, 0x03FFFFFF, 0x03F8FFFF,
        0x03F8FFD7, 0x03FCFE47, 0x03FFFFFF, 0x03FFFFFF, 0x03FFFFFF,
    };
    //Constructor byte8227 selects the all-culled table for both modes.
    bool all_culled = true;
    for (const auto& row : normal_self_culling) {
        for (bool culled : row) {
            if (!culled) all_culled = false;
        }
    }
    if (all_culled) {
        self_culling = all_true();
    } else {
        for (std::size_t a = 0; a < PART_COUNT; ++a) {
            for (std::size_t b = 0; b < PART_COUNT; ++b) {
                self_culling[a][b] = (CULLED[a] & (1u << b)) != 0;
            }
        }
    }
    set_group(6);
    for (auto& body : skeleton.bodies_mut()) {
        body.inertia.linear_drag = drag[0];
        body.inertia.angular_drag = drag[1];
    }
    for (std::size_t part = 1; part < 24; ++part) {
        const SkeletonPart authored = skeleton.definition.parts[part];
        auto& body = skeleton.bodies_mut()[part];
        disable_count[part] = 0;
        parts[part].enabled = true;
        pending_reenable = false;
        parts[part].material = materials[(part == 1 || part == 3 || part == 7) ? 1 : 0];
        if (inverse_mass) {
            body.inertia.inverse_mass = authored.inverse_mass_ragdoll;
        }
        if (inverse_inertia) {
            const auto tensor = authored.ragdoll.dynamics.inverse_tensor;
            body.inertia.inverse_tensor = tensor;
            float minimum = tensor.x;
            if (minimum >= tensor.y) {
                minimum = tensor.y;
            }
            if (minimum >= tensor.z) {
                minimum = tensor.z;
            }
            body.inertia.spherical = 1.0f / minimum;
        }
        body.rates.world_inverse_inertia = world_inverse_inertia(body.rates.basis, body.inertia.inverse_tensor);
    }
    is_ragdoll = true;
}

void SkeletonCollisionMode::finish_ragdoll_request(std::uint32_t mode) {
    switch (mode) {
        case 7:
        case 10:
            //Skeleton82BE3350 clears partial mode, root and extra volumes.
            partial_ragdoll = false;
            disable_root();
            parts[24].enabled = false;
            parts[25].enabled = false;
            break;
        case 8:
            //82D915A0..C4 uses constructor material3076 for all bones.
            for (std::size_t part = 1; part < 24; ++part) {
                parts[part].material = RetailContactMaterial{0.5f, 0.3f, 0.4f};
            }
            break;
        case 9:
            set_group(7);
            break;
        default:
            S3_PANIC("internal error: entered unreachable code: Only original Wipeout requests8..11 enter this adapter");
    }
}

void SkeletonCollisionMode::set_group(std::uint32_t group) {
    assembly_group = group;
    for (auto& part : parts) {
        part.part_group = group;
    }
}

void SkeletonCollisionMode::select_biped() {
    set_group(6);
    disable_all(false);
    for (std::size_t part : {17, 21, 15, 19, 16, 20}) {
        enable_bone(part);
        parts[part].part_group = 17;
    }
    for (std::size_t part : {5, 4, 3, 9, 8, 7, 2, 1}) {
        enable_bone(part);
    }
    for (std::size_t part : {6, 10, 11, 12, 13, 14, 23}) {
        enable_bone(part);
        parts[part].part_group = 18;
    }
    partial_ragdoll = true;
    for (std::size_t part : {24, 25}) {
        parts[part].part_group = 20;
        parts[part].enabled = true;
    }
}

void SkeletonCollisionMode::disable_root() {
    parts[0].volume_group = 4;
    parts[0].enabled = false;
    disable_count[0] = 0;
}

void SkeletonCollisionMode::enable_eligible_bones() {
    for (std::size_t part = 1; part < 24; ++part) {
        if (disable_count[part] == 0) {
            parts[part].enabled = true;
            parts[part].volume_group = 0;
        }
    }
}

}  // namespace s3::physics::skeleton_body::collision_mode
