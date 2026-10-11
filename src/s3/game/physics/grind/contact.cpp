// Port of skate-game/src/physics/grind/contact.rs.
#include "s3/game/physics/grind/contact.hpp"

#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/grind_forces/noise.hpp"
#include "s3/core/physics/grind_forces/orientation.hpp"
#include "s3/core/physics/grind_forces/slide.hpp"
#include "s3/core/physics/grind_forces/support.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::contact {

using V = std::array<float, 4>;
using arithmetic::dot3;
using board::apply_world_force;
namespace gf = ::s3::physics::grind_forces;

namespace {

/// Rust `enum Op`.
struct Op {
    enum class Kind { Orient, Translate, Pin, Friction, Coping, ExitLean, Wheels, Align, TipStability };
    Kind kind;
    std::array<float, 3> args{};
};
Op op(Op::Kind k) { return Op{k, {}}; }
Op friction_op(float a, float b, float c) { return Op{Op::Kind::Friction, {a, b, c}}; }
Op align_op(float a, float b, float c) { return Op{Op::Kind::Align, {a, b, c}}; }

V scale(V v, float s) { return {v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
V sub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
V cross(V a, V b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}

}  // namespace

Status advance(GamePhysics& physics, SkaterRuntime& skater, const ManagerObservation& manager) {
    S3G_TRY_OPT(const Family family, skater.grind.active, "Grind contact requires active family");
    if (!skater.player_input.toolkit) return rs::make_err(std::string("Grind contact requires board toolkit"));
    const auto toolkit = *skater.player_input.toolkit;
    const auto board_frame = toolkit.deck;
    const auto& p = skater.player_input.processed;
    const V velocity = rs::from_bits(p.vectors_400_416[0]);
    auto state = skater.grind.states[index(family)];
    const V position = board_frame[3];
    const V normal = state.output.normal;
    const V across = state.output.across;
    const V direction = state.output.direction;
    std::vector<Op> operations;
    operations.reserve(8);
    // Preserve the original order, including force calls surrounding orientation.
    using K = Op::Kind;
    switch (family) {
        case Family::Darkslide:
        case Family::Boardslide:
            operations = {op(K::Orient), op(K::Translate), op(K::Pin), friction_op(55.0f, 55.0f, 60.0f),
                          op(K::Coping), op(K::ExitLean), op(K::Wheels)};
            break;
        case Family::FiftyFifty:
            operations = {friction_op(50.0f, 40.0f, 40.0f), align_op(800.0f, 0.0f, 0.07f), op(K::Coping),
                          op(K::Pin), op(K::ExitLean), op(K::Orient)};
            break;
        case Family::Tipslide:
            operations = {op(K::Orient), op(K::TipStability), friction_op(60.0f, 60.0f, 60.0f), op(K::Coping),
                          op(K::Pin), op(K::ExitLean), op(K::Wheels)};
            break;
        case Family::FiveO:
            operations = {op(K::Orient), align_op(3000.0f, 0.234f, 0.07f), op(K::Pin),
                          friction_op(50.0f, 40.0f, 40.0f), op(K::Coping), op(K::ExitLean)};
            break;
        case Family::Backslash:
            operations = {op(K::Orient), align_op(3000.0f, 0.39f, 0.025f), friction_op(70.0f, 80.0f, 80.0f),
                          op(K::Coping), op(K::Pin), op(K::ExitLean), op(K::Wheels)};
            break;
    }
    for (const Op& operation : operations) {
        switch (operation.kind) {
            case K::Orient: {
                gf::orientation::Input oi;
                oi.board = board_frame;
                oi.direction = direction;
                oi.normal = normal;
                oi.point = manager.geometry.point_1120;
                oi.yaw_1504 = p.grind.yaw_1504;
                oi.pitch_1508 = p.grind.pitch_1508;
                oi.switched = (p.flags_2468 & 0x00100000u) != 0;
                const auto target_result = gf::orientation::target(static_cast<std::uint32_t>(family), oi);
                if (target_result.is_err()) return rs::make_err(std::string(target_result.unwrap_err()));
                const auto target = target_result.unwrap();
                //82D40890 blends from the current physical board, not the
                //preceding target;82D40A98 adds noise after that blend.
                auto target_frame = gf::orientation::blend(board_frame, target.frame);
                if (target.noise_amount > 0.0f) {
                    const auto draws = skater.grind.take_orientation_noise();
                    target_frame = gf::noise::apply(target_frame, p.scalar_2652, target.noise_amount, draws);
                }
                state.frame = target_frame;
                ::s3::physics::drive_frames::RetailAffineTransform hook;
                for (std::size_t i = 0; i < 3; ++i) {
                    hook.basis.columns[i] = {state.frame[i][0], state.frame[i][1], state.frame[i][2]};
                }
                hook.translation = ::s3::math::Vector3::new_(state.frame[3][0], state.frame[3][1], state.frame[3][2]);
                physics.board.set_hook_transform(hook);
                physics.board.hook_mut().drive.enable_angular_only(skater.ground_lifecycle.board_animated_290);
                if (family == Family::FiveO || family == Family::FiftyFifty) {
                    apply_world_force(physics.board, gf::support::truck_compensation(normal, p.grind.pitch_1508),
                                      position);
                }
                break;
            }
            case K::Friction:
                forces::friction(physics.board, manager, position, velocity, normal, operation.args);
                break;
            case K::Align:
                forces::lateral_pin(physics.board, manager, board_frame, across, normal, velocity, operation.args[0],
                                    operation.args[1], operation.args[2], skater.grind.settings.pin_vs_slope);
                break;
            case K::Pin:
                if (const auto f = gf::support::pin(normal, manager.surface.gravity_relief_1512)) {
                    apply_world_force(physics.board, *f, position);
                }
                break;
            case K::Coping:
                if (const auto f = gf::support::coping(manager.geometry.upmost_normal_1408, velocity,
                                                       manager.surface.gravity_relief_1512)) {
                    apply_world_force(physics.board, *f, position);
                }
                break;
            case K::ExitLean:
                if (const auto f = gf::support::exit_lean(manager.geometry.high_side_1440,
                                                          manager.surface.reckon_blend_selector_1500,
                                                          skater.grind.settings.exit_assist)) {
                    apply_world_force(physics.board, *f, position);
                }
                break;
            case K::Wheels: {
                std::array<bool, 4> contact{};
                for (std::size_t i = 0; i < 4; ++i) contact[i] = physics.riding.ground.parts[i].in_contact;
                lifecycle::manage_wheel_spin(physics.board, contact);
                break;
            }
            case K::Translate: {
                state.preparing_jump |= (p.flags_2468 & 0x00200000u) != 0;
                gf::slide::Input si;
                si.position = position;
                si.point = manager.geometry.point_1120;
                si.direction = direction;
                si.across = across;
                si.velocity = velocity;
                si.translation_2796 = manager.control.translation_2796;
                si.total_mass_2660 = toolkit.total_mass;
                si.update_frequency = physics.settings.step.simulation.frequency;
                si.preparing_jump = state.preparing_jump;
                si.geometry_kind = manager.geometry.kind_1464;
                forces::slide(physics.board,
                              family == Family::Darkslide ? gf::slide::Slide::Darkslide : gf::slide::Slide::Boardslide,
                              si);
                break;
            }
            case K::TipStability: {
                const V offset = sub(position, manager.geometry.point_1120);
                const V inward = scale(across, dot3(across, offset) > 0.0f ? -1.0f : 1.0f);
                float amount;
                if ((manager.control.flags_1516 & 0x02000000u) != 0) {
                    amount = 39.0f;
                } else {
                    amount = (dot3(normal, cross(direction, offset)) > 0.0f ? manager.control.balance_2800
                                                                            : -manager.control.balance_2800) *
                             37.0f;
                }
                apply_world_force(physics.board, scale(inward, amount), position);
                break;
            }
        }
    }
    if (family == Family::Darkslide) {
        // Darkslide contact82D41194..1234 alone writes classification104 here.
        if (manager.geometry.kind_1464 == 2) {
            state.output.classification_104 = dot3(manager.geometry.high_side_1440, toolkit.effective[2]) > 0.0f ? 2 : 1;
        } else {
            const V perpendicular = scale(across, dot3(sub(position, manager.geometry.point_1120), across));
            state.output.classification_104 = dot3(perpendicular, toolkit.effective[2]) > 0.0f ? 1 : 2;
        }
    }
    skater.grind.states[index(family)] = state;
    return ok();
}

}  // namespace s3::game::physics::grind::contact
