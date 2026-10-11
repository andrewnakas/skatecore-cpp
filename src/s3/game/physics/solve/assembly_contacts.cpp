// Port of skate-game/src/physics/solve/assembly_contacts.rs.
#include "s3/game/physics/solve/assembly_contacts.hpp"

#include <array>

#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/world_contact.hpp"

namespace s3::game::physics::solve::assembly_contacts {

using sc::physics::board_step::BoardCollision;
using sc::physics::board_world::BoardWorldVolume;
using sc::physics::contact::RetailContactInput;

Status append(std::vector<BoardCollision>& contacts, const std::vector<BoardWorldVolume>& board,
              const std::vector<BoardWorldVolume>& rider, std::uint32_t board_group,
              const sc::physics::skeleton_body::SkeletonCollisionMode& collision) {
    //8277AF90 first culls the assembly pair;8277A6C0 then culls each
    //primitive pair using its part group, not Volume84's world-query group.
    bool part_out = false;
    for (const auto& part : collision.parts) part_out = part_out || part.part_group >= 21;
    if (board_group >= 21 || collision.assembly_group >= 21 || part_out) {
        return rs::make_err(std::string("Skater collision group is outside the original21x21 table"));
    }
    if (group_pair_allowed(board_group, collision.assembly_group)) {
        for (const auto& a : board) {
            for (const auto& b : rider) {
                if (!b.body.is_attached()) S3_PANIC("internal error: entered unreachable code");
                const std::size_t part = b.body.attached;
                //82DC3BD0/82DC4260 copy Part92 into VolumeData212
                //(assembly96 + volume212 = assembly308). Skate2's
                //82E0B690 names this producer part.m_userData. Biped assigns
                //hands5, legs17, torso18 and controller capsules20.
                if (!group_pair_allowed(board_group, collision.parts.at(part).part_group)) continue;
                append_pair(contacts, a, b);
            }
        }
    }
    //8277AE80 suppresses the reversed assembly pair. The same-assembly
    //branch8277B2C0 instead visits both directed primitive pairs and uses
    //the skeleton's part bitmap. Board82C0B560 culls all7x7 self pairs.
    for (const auto& a : rider) {
        if (!a.body.is_attached()) S3_PANIC("internal error: entered unreachable code");
        const std::size_t part_a = a.body.attached;
        for (const auto& b : rider) {
            if (!b.body.is_attached()) S3_PANIC("internal error: entered unreachable code");
            const std::size_t part_b = b.body.attached;
            if (part_a != part_b && !collision.self_culling.at(part_a).at(part_b)) {
                append_pair(contacts, a, b);
            }
        }
    }
    return ok();
}

bool group_pair_allowed(std::uint32_t a, std::uint32_t b) {
    //82765EF0..827679AC, Island.flags==3: inverted native culling bits.
    //Skate2 827B8C10 has19 groups and different pairs; do not import it.
    static constexpr std::array<std::uint32_t, 21> ALLOWED = {
        0x1876fd, 0, 0x120001, 1, 0x79d1, 0x1079e1, 0x1079f1, 0x279f1, 0x1676f0, 0x7101, 0x5101,
        0x1008f0, 0x1d77f1, 0x233f1, 0xa57f1, 0, 0x1000, 0x6184, 0x1100, 0x5001, 0x1965,
    };
    return b < 21 && a < ALLOWED.size() && (ALLOWED[a] & (1u << b)) != 0;
}

void append_pair(std::vector<BoardCollision>& contacts, const BoardWorldVolume& a, const BoardWorldVolume& b) {
    //8277B0F0 and8277B2C0 enter the same8277A508 primitive walker with
    //identical padding/triangle settings. Keep A/B orientation and point order.
    const auto manifold = sc::physics::world_contact::primitive_pair_contacts(
        a.primitive, b.primitive, sc::physics::world_contact::PrimitivePairSettings::skater_self_collision());
    if (!manifold) return;
    const auto material = sc::physics::contact::combine_contact_materials(a.material, b.material);
    for (std::size_t i = 0; i < manifold->count; ++i) {
        const auto& points = manifold->points[i];
        RetailContactInput contact{};
        contact.position_on_a = points.a;
        contact.position_on_b = points.b;
        contact.normal = manifold->normal;
        contact.restitution = material.restitution;
        contact.static_friction = material.static_friction;
        contact.dynamic_friction = material.dynamic_friction;
        //8277A7B4/B8 packs original Volume88 tags, both zero for
        //stock skater/board constructors (not the part indices).
        contact.tag = 0;
        contacts.push_back(BoardCollision{a.body, b.body, contact});
    }
}

}  // namespace s3::game::physics::solve::assembly_contacts
