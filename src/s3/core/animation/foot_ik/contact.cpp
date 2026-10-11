// Port of skate-core/src/animation/foot_ik/contact.rs.
#include "s3/core/animation/foot_ik/contact.hpp"

namespace s3::animation::foot_ik::contact {

using physics::skeleton_animation_record::transform_point;
using rs::from_bits;
using status::Mode;
using transforms::LimbFrames;

namespace {

std::array<float, 4> contact_offset(const Transform& board, float offset) {
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = board[0][i] * 0.0f;
        const float y = rs::mul_add(board[1][i], offset, x);
        out[i] = rs::mul_add(board[2][i], 0.0f, y);
    }
    return out;
}

///82BF0558: compare both positions in inverse-board space, retaining the
///separate point transforms and the .025 foot-surface distance adjustment.
float distance_to_contact(const FootContact& contact, const LimbFrames& frame, const Transform& board,
                          const Transform& inverse) {
    const std::array<float, 4> local_contact = transform_point(inverse, contact.position);
    const std::array<float, 4> delta = contact_offset(board, contact.offset);
    std::array<float, 4> foot{};
    for (std::size_t i = 0; i < 4; ++i) foot[i] = frame.world[3][i] + delta[i];
    const std::array<float, 4> local_foot = transform_point(inverse, foot);
    return (local_foot[1] - from_bits(0x3CCCCCCDu)) - local_contact[1];
}

void add_offset(LimbFrames& frame, const Transform& board, float offset) {
    const std::array<float, 4> delta = contact_offset(board, offset);
    for (std::size_t i = 0; i < 4; ++i) {
        frame.world[3][i] += delta[i];
        frame.parent_world[3][i] += delta[i];
    }
}
float select(float test, float nonnegative, float negative) { return test >= 0.0f ? nonnegative : negative; }
float limit_offset(float value) {
    const float min = from_bits(0xBDCCCCCDu);
    const float max = from_bits(0x3DCCCCCDu);
    const float lower = select(min - value, min, value);
    return select(max - lower, lower, max);
}
void approach(FootContact& contact) {
    const float min = from_bits(0xBC75C28Fu);
    const float max = from_bits(0x3C75C28Fu);
    const float delta = contact.desired_offset - contact.offset;
    const float lower = select(min - delta, min, delta);
    contact.offset = select(max - lower, lower, max) + contact.offset;
}

}  // namespace

void ContactState::update(const std::array<status::LimbStatus, 4>& statuses, std::array<LimbFrames, 4>& frames,
                          const ContactInput& input) {
    support_failed_this_update = false;
    const bool requires_support = (input.flags_2468 & 0x42000000u) != 0;
    for (std::size_t foot = 0; foot < 2; ++foot) {
        FootContact& contact = feet[foot];
        LimbFrames& frame = frames[foot];
        bool failed = false;
        if (statuses[foot].mode == Mode::OnDeck && input.contact_bone == input.foot_bones[foot]) {
            switch (contact.query_state) {
                case 1: {
                    const std::array<float, 4> local = transform_point(input.inverse_board, contact.position);
                    const float height = local[1] + from_bits(0x3DA8F5C3u);
                    if (height <= from_bits(0x3E99999Au)) {
                        contact.desired_offset = (input.flags_2468 & 0x01800000u) == 0 ? limit_offset(height) : 0.0f;
                        approach(contact);
                        const float distance = distance_to_contact(contact, frame, input.board, input.inverse_board);
                        //82BEFFB4..FFCC: fix modest penetration immediately;
                        //larger separations retain the bounded transition.
                        if (distance < 0.0f && distance > from_bits(0xBE99999Au)) {
                            contact.offset -= distance;
                        }
                        add_offset(frame, input.board, contact.offset);
                        failed = requires_support && distance > from_bits(0x3CF5C28Fu) &&
                                 contact.offset < from_bits(0xBC23D70Au);
                    } else {
                        contact.desired_offset = limit_offset(contact.desired_offset);
                        approach(contact);
                        add_offset(frame, input.board, contact.offset);
                        failed = requires_support || input.hips_world_position[1] > from_bits(0x3F333333u);
                    }
                    break;
                }
                case 0:
                case 2:
                    contact.desired_offset = limit_offset(contact.desired_offset);
                    approach(contact);
                    add_offset(frame, input.board, contact.offset);
                    failed = contact.query_state == 2 && requires_support;
                    break;
                default: break;
            }
            if (input.current_contacts[foot]) {
                contact.position = *input.current_contacts[foot];
                contact.query_state = 1;
            } else {
                contact.query_state = 2;
            }
        } else {
            contact.query_state = 0;
            contact.desired_offset = 0.0f;
            approach(contact);
            add_offset(frame, input.board, contact.offset);
        }
        if (failed) {
            support_failed = true;
            support_failed_this_update = true;
        }
    }
}

}  // namespace s3::animation::foot_ik::contact
