// Port of skate-core/src/physics/contact_feedback/board_reports.rs.
#include "s3/core/physics/contact_feedback/board_reports.hpp"

namespace s3::physics::contact_feedback::board_reports {

namespace {
constexpr std::uint32_t U32_MAX = 0xFFFFFFFFu;

CollisionBody body_id(std::uint32_t value) {
    if (value == U32_MAX) {
        return CollisionBody::StaticWorld();
    }
    return CollisionBody::Board(board::BodyId_ORDER.at(value));
}

Vector3 center(const CollisionBody& body, const std::array<BodySnapshot, BODY_COUNT>& bodies) {
    switch (body.kind) {
        case CollisionBody::Kind::Board:
            return bodies[board::index(body.board)].rates.position;
        case CollisionBody::Kind::StaticWorld:
            return Vector3::ZERO;
        case CollisionBody::Kind::Attached:
            break;
    }
    S3_PANIC("collector selects board/world contacts above");
}

Vector3 velocity(const CollisionBody& body, const std::array<BodySnapshot, BODY_COUNT>& bodies) {
    switch (body.kind) {
        case CollisionBody::Kind::Board:
            return bodies[board::index(body.board)].rates.linear_velocity;
        case CollisionBody::Kind::StaticWorld:
            return Vector3::ZERO;
        case CollisionBody::Kind::Attached:
            break;
    }
    S3_PANIC("collector selects board/world contacts above");
}

Vector3 vector(const std::array<std::uint32_t, 64>& words, std::size_t offset) {
    return Vector3{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]), rs::from_bits(words[offset + 2])};
}
Vector3 scale(Vector3 v, float scale) { return Vector3{v.x * scale, v.y * scale, v.z * scale}; }
Vector3 add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 subtract(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 combine(Vector3 a, float weight, Vector3 b) {
    return Vector3{rs::mul_add(a.x, weight, b.x), rs::mul_add(a.y, weight, b.y), rs::mul_add(a.z, weight, b.z)};
}
}  // namespace

void collect(std::vector<BoardContactReport>& output, Slice<const RetailContactJacobian> contacts,
             const std::array<BodySnapshot, BODY_COUNT>& bodies, float frequency) {
    output.clear();
    const float frequency_squared = frequency * frequency;
    for (const RetailContactJacobian& contact : contacts) {
        const auto& words = contact.words_();
        const auto impulse = contact.accumulated_impulse();
        // 82AE1668..168C: enabled spy and strictly positive normal response.
        if ((words[11] & 8u) == 0 || !(impulse[0] > 0.0f)) {
            continue;
        }
        // Other assemblies share this solve. Their own output owners consume
        // their spies; only board/world records are routed to this collector.
        const std::uint32_t body_count = static_cast<std::uint32_t>(BODY_COUNT);
        if (!((words[31] < body_count && words[43] == U32_MAX) || (words[43] < body_count && words[31] == U32_MAX))) {
            continue;
        }
        const CollisionBody a = body_id(words[31]);
        const CollisionBody b = body_id(words[43]);
        BodyId part;
        CollisionBody other;
        bool is_body_a;
        if (a.is_board() && b.is_static_world()) {
            part = a.board;
            other = b;
            is_body_a = true;
        } else if (a.is_static_world() && b.is_board()) {
            part = b.board;
            other = a;
            is_body_a = false;
        } else {
            // 82768418: same board/owner exclusion; no world report subscriber.
            continue;
        }
        if (output.size() == 16) {
            break;
        }
        const Vector3 normal = vector(words, 28);
        const std::array<Vector3, 2> tangents{vector(words, 40), vector(words, 52)};
        const Vector3 a_position = add(center(a, bodies), vector(words, 0));
        const Vector3 b_position = add(center(b, bodies), vector(words, 4));
        const float a_weight = rs::from_bits(words[35]);
        const float b_weight = rs::from_bits(words[39]);
        const float inverse_weight = 1.0f / (b_weight + a_weight);
        const Vector3 position = combine(a_position, a_weight, scale(b_position, b_weight));
        const Vector3 friction = combine(tangents[0], impulse[1], scale(tangents[1], impulse[2]));
        const float sign = is_body_a ? 1.0f : -1.0f;
        const CollisionBody self = CollisionBody::Board(part);
        BoardContactReport report;
        report.part = part;
        report.other = other;
        report.is_body_a = is_body_a;
        report.normal = scale(normal, sign);
        report.position = scale(position, inverse_weight);
        report.relative_linear_velocity = subtract(velocity(self, bodies), velocity(other, bodies));
        report.other_surface =
            is_body_a ? static_cast<std::uint16_t>(words[55]) : static_cast<std::uint16_t>(words[55] >> 16);
        report.normal_force_on_a = scale(scale(normal, impulse[0]), frequency_squared);
        report.friction_force_on_a = scale(friction, frequency_squared);
        report.tangents = tangents;
        output.push_back(report);
    }
}

}  // namespace s3::physics::contact_feedback::board_reports
