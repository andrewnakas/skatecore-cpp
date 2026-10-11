// Port of skate-core/src/physics/solver/contact.rs and solver/contact_data.rs.
#include "s3/core/physics/solver/contact.hpp"

#include "s3/core/rs.hpp"

namespace s3::physics::solver::contact {

namespace data {

namespace {

// 82AE297C..2A18: linear + angular cross arm. Fuse the first product into
// linear before subtracting the second; a separate cross product rounds early.
Components point_correction(Components arm, Components linear, Components angular) {
    Components out{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const std::size_t next = lane == 3 ? 3 : (lane + 1) % 3;
        const std::size_t previous = lane == 3 ? 3 : (lane + 2) % 3;
        const float first = rs::mul_add(angular[next], arm[previous], linear[lane]);
        out[lane] = rs::mul_add(-angular[previous], arm[next], first);
    }
    return out;
}
Components read(Slice<std::uint32_t> words, std::size_t row) {
    Components out{};
    for (std::size_t lane = 0; lane < 4; ++lane) out[lane] = rs::from_bits(words[4 * row + lane]);
    return out;
}
void write(Slice<std::uint32_t> words, std::size_t row, Components value) {
    for (std::size_t lane = 0; lane < 4; ++lane) words[4 * row + lane] = rs::to_bits(value[lane]);
}

}  // namespace

ContactRows ContactRows::read(Slice<std::uint32_t> words) {
    S3_ASSERT(words.size() == 64);
    ContactRows rows;
    rows.arm_a = data::read(words, 0);
    rows.arm_b = data::read(words, 1);
    for (std::size_t i = 0; i < 3; ++i) rows.correction_columns[i] = data::read(words, 2 + i);
    rows.static_friction = rs::from_bits(words[15]);
    rows.dynamic_friction = rs::from_bits(words[19]);
    rows.accumulated = data::read(words, 5);
    rows.target = data::read(words, 6);
    for (std::size_t i = 0; i < 3; ++i) {
        rows.axes[i] = data::read(words, 7 + 3 * i);
        rows.angular_a[i] = data::read(words, 8 + 3 * i);
        rows.angular_b[i] = data::read(words, 9 + 3 * i);
    }
    return rows;
}

void ContactRows::publish_impulses(Slice<std::uint32_t> words, Components impulse) const {
    write(words, 5, impulse);
}

Reaction Reaction::read(Slice<std::uint32_t> words) {
    S3_ASSERT(words.size() == 16);
    Reaction r;
    r.linear = data::read(words, 0);
    r.position = data::read(words, 1);
    r.angular = data::read(words, 2);
    r.orientation = data::read(words, 3);
    return r;
}

Components Reaction::point_position(Components arm) const {
    return point_correction(arm, position, orientation);
}

Components Reaction::point_velocity(Components arm) const {
    return point_correction(arm, linear, angular);
}

void Reaction::apply_contact(const ContactRows& rows, Components change, bool body_a) {
    const auto& response = body_a ? rows.angular_a : rows.angular_b;
    const float inverse_mass = response[0][3];
    for (std::size_t lane = 0; lane < 4; ++lane) {
        float linear_impulse = rs::mul_add(rows.axes[0][lane], change[0], 0.0f);
        linear_impulse = rs::mul_add(rows.axes[1][lane], change[1], linear_impulse);
        linear_impulse = rs::mul_add(rows.axes[2][lane], change[2], linear_impulse);
        const float position_impulse = rs::mul_add(rows.axes[0][lane], change[3], 0.0f);
        if (body_a) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                angular[lane] = rs::mul_add(response[axis][lane], change[axis], angular[lane]);
            }
            orientation[lane] = rs::mul_add(response[0][lane], change[3], orientation[lane]);
            linear[lane] = rs::mul_add(linear_impulse, inverse_mass, linear[lane]);
            position[lane] = rs::mul_add(position_impulse, inverse_mass, position[lane]);
        } else {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                angular[lane] = rs::mul_add(-response[axis][lane], change[axis], angular[lane]);
            }
            orientation[lane] = rs::mul_add(-response[0][lane], change[3], orientation[lane]);
            linear[lane] = rs::mul_add(-linear_impulse, inverse_mass, linear[lane]);
            position[lane] = rs::mul_add(-position_impulse, inverse_mass, position[lane]);
        }
    }
}

void Reaction::write(Slice<std::uint32_t> words) const {
    const Components rows[4] = {linear, position, angular, orientation};
    for (std::size_t row = 0; row < 4; ++row) data::write(words, row, rows[row]);
}

}  // namespace data

using data::Components;
using data::ContactRows;
using data::Reaction;

void solve(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b) {
    const ContactRows rows = ContactRows::read(record);
    Reaction reaction_a = Reaction::read(a);
    Reaction reaction_b = Reaction::read(b);
    const Components position_a = reaction_a.point_position(rows.arm_a);
    const Components position_b = reaction_b.point_position(rows.arm_b);
    const Components velocity_a = reaction_a.point_velocity(rows.arm_a);
    const Components velocity_b = reaction_b.point_velocity(rows.arm_b);
    Components position_error{};
    Components total_error{};
    for (std::size_t i = 0; i < 4; ++i) position_error[i] = position_b[i] - position_a[i];
    for (std::size_t i = 0; i < 4; ++i) total_error[i] = (velocity_b[i] - velocity_a[i]) + position_error[i];

    // 82AE2A94..2ABC: the first three impulses respond to combined position
    // and velocity correction. The fourth responds only to position error.
    Components candidate{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const Components& error = lane == 3 ? position_error : total_error;
        float impulse = rows.accumulated[lane] + rows.target[lane];
        for (std::size_t component = 0; component < 3; ++component) {
            const std::size_t coefficient_lane = lane == 3 ? 0 : lane;
            impulse = rs::mul_add(rows.correction_columns[component][coefficient_lane], error[component], impulse);
        }
        candidate[lane] = impulse;
    }

    // 82AE2AC4..2B08: friction uses the preceding normal impulse. Crossing
    // the static limit selects the dynamic limit. The two normal lanes use
    // [0, f32::MAX]. Ordered comparisons also preserve the native selectors.
    Components next{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float normal = (lane == 1 || lane == 2) ? rows.accumulated[0] : 0.0f;
        const float upper_offset = (lane == 0 || lane == 3) ? rs::f32::MAX : 0.0f;
        const float static_low = rs::mul_add(-rows.static_friction, normal, 0.0f);
        const float static_high = rs::mul_add(rows.static_friction, normal, upper_offset);
        const float dynamic_low = rs::mul_add(-rows.dynamic_friction, normal, 0.0f);
        const float dynamic_high = rs::mul_add(rows.dynamic_friction, normal, upper_offset);
        const float lower_selected = candidate[lane] >= static_low ? candidate[lane] : dynamic_low;
        next[lane] = static_high >= candidate[lane] ? lower_selected : dynamic_high;
    }
    Components change{};
    for (std::size_t lane = 0; lane < 4; ++lane) change[lane] = next[lane] - rows.accumulated[lane];
    rows.publish_impulses(record, next);
    reaction_a.apply_contact(rows, change, true);
    reaction_b.apply_contact(rows, change, false);
    reaction_a.write(a);
    reaction_b.write(b);
}

}  // namespace s3::physics::solver::contact
