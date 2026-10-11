// Part of skate-core/src/physics/board_step.rs: `CollisionBody` and
// `ATTACHED_REACTION_BASE`, split into their own header only to break the C++
// include cycle board_step <-> contact_feedback::board_reports. Namespace is
// still s3::physics::board_step (include board_step.hpp normally).
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>

#include "s3/core/physics/board.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::board_step {

using board::BODY_COUNT;
using board::BodyId;

/// Board parts0..6 and their separate hook7 precede the physical skater.
inline constexpr std::size_t ATTACHED_REACTION_BASE = BODY_COUNT + 1;

/// Rust `enum CollisionBody { Board(BodyId), Attached(usize), StaticWorld }`.
struct CollisionBody {
    enum class Kind : std::uint8_t { Board, Attached, StaticWorld };
    Kind kind = Kind::StaticWorld;
    /// Payload of `Board`.
    BodyId board = BodyId::RightFrontWheel;
    /// Payload of `Attached`: index in AttachedStep::bodies, referencing the
    /// actual physical body.
    std::size_t attached = 0;

    static constexpr CollisionBody Board(BodyId id) { return CollisionBody{Kind::Board, id, 0}; }
    static constexpr CollisionBody Attached(std::size_t index) {
        return CollisionBody{Kind::Attached, BodyId::RightFrontWheel, index};
    }
    static constexpr CollisionBody StaticWorld() { return CollisionBody{}; }

    bool is_board() const { return kind == Kind::Board; }
    bool is_attached() const { return kind == Kind::Attached; }
    bool is_static_world() const { return kind == Kind::StaticWorld; }

    std::uint32_t contact_id() const {
        switch (kind) {
            case Kind::Board:
                return static_cast<std::uint32_t>(board::index(board));
            case Kind::Attached: {
                const std::size_t id = ATTACHED_REACTION_BASE + attached;
                if (id > std::numeric_limits<std::uint32_t>::max()) {
                    S3_PANIC("attached contact body index exceeds host storage");
                }
                return static_cast<std::uint32_t>(id);
            }
            case Kind::StaticWorld:
                break;
        }
        return std::numeric_limits<std::uint32_t>::max();
    }

    static CollisionBody from_contact_id(std::uint32_t id) {
        if (id == std::numeric_limits<std::uint32_t>::max()) {
            return StaticWorld();
        } else if (id < static_cast<std::uint32_t>(BODY_COUNT)) {
            return Board(board::BodyId_ORDER[id]);
        }
        if (!(id >= static_cast<std::uint32_t>(ATTACHED_REACTION_BASE))) {
            S3_PANIC("board target has no collision volume");
        }
        return Attached(static_cast<std::size_t>(id) - ATTACHED_REACTION_BASE);
    }
};
inline bool operator==(const CollisionBody& a, const CollisionBody& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case CollisionBody::Kind::Board:
            return a.board == b.board;
        case CollisionBody::Kind::Attached:
            return a.attached == b.attached;
        case CollisionBody::Kind::StaticWorld:
            return true;
    }
    return true;
}
inline bool operator!=(const CollisionBody& a, const CollisionBody& b) { return !(a == b); }
inline std::ostream& operator<<(std::ostream& os, const CollisionBody& b) {
    switch (b.kind) {
        case CollisionBody::Kind::Board:
            return os << "Board(" << board::index(b.board) << ")";
        case CollisionBody::Kind::Attached:
            return os << "Attached(" << b.attached << ")";
        case CollisionBody::Kind::StaticWorld:
            break;
    }
    return os << "StaticWorld";
}

}  // namespace s3::physics::board_step
