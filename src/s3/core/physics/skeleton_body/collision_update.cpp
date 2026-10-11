// Port of skate-core/src/physics/skeleton_body/collision_update.rs.
// Complete contact traversal and classification of TU3 82BD4A30.
#include <limits>

#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::collision_feedback {

using collision_vector::add;
using collision_vector::clamp_length;
using collision_vector::cross;
using collision_vector::dot;
using collision_vector::length;
using collision_vector::normalize;
using collision_vector::scale;
using collision_vector::sub;
using collision_vector::transform;

namespace {

// Original read-only table820CFCB0. Part0 is not a region participant.
constexpr std::size_t REGIONS[24] = {
    std::numeric_limits<std::size_t>::max(),
    0, 1, 2, 2, 2, 1, 3, 3, 3, 1, 1, 1, 1, 1, 6, 6, 4, 4, 7, 7, 5, 5, 1,
};

float mass(SkeletonContactBody body) { return 1.0f / (body.state_flags == 1 ? 0.001f : body.inverse_mass); }
float maximum(float a, float b) { return a - b >= 0.0f ? a : b; }

}  // namespace

void SkeletonCollisionFeedback::update(const SkeletonCollisionInput& input,
                                       const std::vector<SkeletonContactReport>& reports) {
    update(input, reports.data(), reports.size());
}

void SkeletonCollisionFeedback::update(const SkeletonCollisionInput& input,
                                       const SkeletonContactReport* reports, std::size_t reports_len) {
    begin_frame(input);
    const float reference_speed = length(input.reference_velocity);
    const V direction = normalize(input.reference_velocity);
    for (std::size_t r = 0; r < reports_len; ++r) {
        const SkeletonContactReport& report = reports[r];
        // Original constructor enables reports for the24 animated parts.
        // A different owner must not silently index this state as part24/25.
        if (!(report.part < 24)) {
            S3_PANIC("SkeletonCollision report part exceeds native24-part array");
        }
        const std::size_t part = report.part;
        const std::uint32_t material = (report.tag >> 7) & 31u;
        flags.any = true;
        if (report.other_group == 4 && (part == 15 || part == 16 || part == 19 || part == 20)) {
            flags.foot_board = true;
        }
        if (material == 6) {
            flags.material_6 = true;
        }
        if (material == 12) {
            flags.material_12 = true;
            material_12_height = report.point[1];
        }
        if (ignore_ground(input, report)) {
            continue;
        }
        observe_normal(input.ragdoll, report, material);
        bool specific_hit = false;
        for (auto& point : specific) {
            const V delta = sub(report.point, point.world_point);
            if (part == point.part && dot(delta, delta) < point.radius_squared) {
                point.current = true;
                point.recent = true;
                specific_hit = true;
            }
        }
        // This gate precedes priority replacement and all force recording.
        if (planes.size() >= 20) {
            continue;
        }
        contact_age[part] = 0.0f;
        current[part] = true;
        if (priority[part] > maximum_priority) {
            maximum_priority = priority[part];
            planes.clear();
        }
        if (priority[part] == maximum_priority) {
            planes.push_back(ContactPlane{report.normal, part});
        }

        const float mass_a = mass(report.body_a);
        const float mass_b = mass(report.body_b);
        const float other_mass = report.side_a ? mass_b : mass_a;
        const bool small = other_mass > 0.0001f && other_mass < settings.small_object_mass;
        const float factor = ((mass_b + mass_a) / (mass_b * mass_a)) * rs::from_bits(0x3991A2B5u);
        const float weighted_force = length(scale(report.solved_vector, factor)) * input.part_weights[part];
        const V part_velocity = input.physical.velocities[part];
        V own_velocity;
        if (input.ragdoll) {
            own_velocity = scale(add(input.com_velocity, part_velocity), 0.5f);
        } else if (input.offboard || input.entering_offboard) {
            own_velocity = input.com_velocity;
            own_velocity[1] = 0.0f;
        } else {
            const V projected = scale(direction, dot(part_velocity, direction));
            own_velocity = clamp_length(projected, reference_speed);
        }
        const SkeletonContactBody other = report.side_a ? report.body_b : report.body_a;
        const V relative = sub(other.linear_velocity, own_velocity);
        float force = rs::abs(dot(relative, report.normal));
        if (small) {
            force *= 0.5f;
        }
        switch (report.other_group) {
            case 5:
                force *= input.ai_collision_scalar ? settings.ai_scalar : settings.skater_scalar;
                maximum_skater_force = maximum(maximum_skater_force, force);
                if (report.other_entity) {
                    other_skater = *report.other_entity;
                }
                flags.nonboard = true;
                break;
            case 8:
                flags.group_8 = true;
                maximum_group_8_force = maximum(maximum_group_8_force, force);
                flags.nonboard = true;
                break;
            case 11:
                maximum_group_11_force = maximum(maximum_group_11_force, force);
                flags.nonboard = true;
                break;
            case 4:
                break;
            default:
                flags.nonboard = true;
                break;
        }
        if (part > 0) {
            record_force(report, specific_hit, relative, force, weighted_force);
            timer = settings.body.effect_time;
        }
        if (!compliant[part] || small) {
            flags.noncompliant = true;
        } else {
            flags.compliant = true;
        }
        flags.has_impulse = flags.compliant || input.offboard;
    }
    if (input.ragdoll) {
        flags.impaled = check_impaled();
    } else {
        flags.conflicting = check_conflicting(input);
    }
    const float duration = settings.body.effect_time;
    if (duration == 0.0f) {
        drive_weight = 1.0f;
    } else {
        const float ratio = timer / duration;
        const float low = -ratio >= 0.0f ? 0.0f : ratio;
        drive_weight = 1.0f - (1.0f - low >= 0.0f ? low : 1.0f);
    }
    flags.recovering = drive_weight < 1.0f;
}

void SkeletonCollisionFeedback::begin_frame(const SkeletonCollisionInput& input) {
    timer -= input.dt;
    wipeout_times[2] += input.dt;
    flags = SkeletonContactFlags{};
    flags.ragdoll = input.ragdoll;
    planes.clear();
    maximum_priority = 0.0f;
    maximum_skater_force = 0.0f;
    other_skater = -1;
    maximum_group_8_force = 0.0f;
    maximum_group_11_force = 0.0f;
    material_12_height = 0.0f;
    highest_normal = {0.0f, -1.0f, 0.0f, 0.0f};
    foot_normal = {0.0f, 0.0f, 0.0f, 0.0f};
    material_normals = {V{0.0f, -1.0f, 0.0f, 0.0f}, V{0.0f, -1.0f, 0.0f, 0.0f}};
    for (std::size_t part = 0; part < 24; ++part) {
        contact_age[part] -= input.dt;
        current[part] = false;
        bones[part] = BoneContact{};
    }
    for (auto& region : regions) {
        // Unlike Reset, Update leaves the material flag untouched.
        ContactRegion fresh{};
        fresh.material_flags = region.material_flags;
        region = fresh;
    }
    if (!input.ragdoll) {
        wipeout_times = {0.0f, 0.0f, 0.0f};
    }
    for (auto& point : specific) {
        point.recent = point.recent && input.ragdoll;
        point.current = false;
        point.world_point = transform(input.physical.pose[point.part], point.local_point);
    }
    if (input.request_partial_ragdoll) {
        timer = settings.body.effect_time;
    }
}

bool SkeletonCollisionFeedback::ignore_ground(const SkeletonCollisionInput& input,
                                              const SkeletonContactReport& report) const {
    if (input.ragdoll || (input.disable_ground_filter && !input.category_600)) {
        return false;
    }
    const float distance = dot(input.plane_normal, sub(report.point, input.plane_point));
    const float threshold = input.offboard ? 0.3f : settings.ground_plane_max_distance;
    return distance < threshold &&
           (input.offboard || rs::abs(dot(report.normal, input.plane_normal)) > settings.ground_plane_max_angle);
}

void SkeletonCollisionFeedback::observe_normal(bool ragdoll, const SkeletonContactReport& report,
                                               std::uint32_t material) {
    const V normal = report.normal;
    if (normal[1] > highest_normal[1]) {
        highest_normal = normal;
    }
    if (ragdoll) {
        if (material == 10 && normal[1] > material_normals[0][1]) {
            material_normals[0] = normal;
            flags.material_10 = true;
        } else if (material == 11 && normal[1] > material_normals[1][1]) {
            material_normals[1] = normal;
            flags.material_11 = true;
        }
    } else if ((report.part == 15 || report.part == 19) && normal[1] > foot_normal[1]) {
        foot_normal = normal;
    }
}

void SkeletonCollisionFeedback::record_force(const SkeletonContactReport& report, bool specific_hit,
                                             V relative, float force, float weighted_force) {
    BoneContact& bone = bones[report.part];
    ContactRegion& region = regions[REGIONS[report.part]];
    const float previous = specific_hit ? bone.specific_force : bone.force;
    const V tangent = (force > previous || force > region.force) ? cross(relative, report.normal)
                                                                 : V{0.0f, 0.0f, 0.0f, 0.0f};
    if (force > previous) {
        if (specific_hit) {
            bone.specific_force = force;
            bone.specific_normal = report.normal;
            bone.specific_tangent = tangent;
            bone.specific_tag = report.tag;
        } else {
            bone.force = force;
            bone.normal = report.normal;
            bone.tangent = tangent;
            bone.point = report.point;
            bone.tag = report.tag;
        }
        const std::uint32_t groups[3] = {8, 11, 5};
        for (std::size_t i = 0; i < 3; ++i) {
            bone.groups[i] = bone.groups[i] || report.other_group == groups[i];
        }
        bone.groups[3] = bone.groups[3] ||
                         !(report.other_group == 8 || report.other_group == 11 || report.other_group == 5);
    }
    if (force > region.force) {
        region = ContactRegion{
            force, weighted_force, length(tangent), report.tag & 0x7fu, report.normal, report.part,
        };
    }
}

bool SkeletonCollisionFeedback::check_conflicting(const SkeletonCollisionInput& input) const {
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const ContactRegion& a = regions[i];
        if (!a.part) {
            continue;
        }
        const std::size_t part_a = *a.part;
        if (!(a.force > 0.0f) || !compliant[part_a]) {
            continue;
        }
        for (std::size_t j = 0; j < regions.size(); ++j) {
            const ContactRegion& b = regions[j];
            if (!b.part) {
                continue;
            }
            const std::size_t part_b = *b.part;
            if (i == j || !(b.force > 0.0f) || !compliant[part_b]) {
                continue;
            }
            if (dot(a.normal, b.normal) < -0.7f &&
                dot(sub(input.body_frames[part_b][3], input.body_frames[part_a][3]), a.normal) < 0.0f) {
                return true;
            }
        }
    }
    return false;
}

bool SkeletonCollisionFeedback::check_impaled() const {
    for (std::size_t i = 1; i < 23; ++i) {
        const BoneContact a = bones[i];
        if (!(a.force > 0.0f)) {
            continue;
        }
        for (std::size_t j = i + 1; j < 24; ++j) {
            const BoneContact& b = bones[j];
            if (b.force > 0.0f && dot(a.normal, b.normal) < -0.99f &&
                rs::abs(dot(a.normal, sub(a.point, b.point))) < 0.03f) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace s3::physics::skeleton_body::collision_feedback
