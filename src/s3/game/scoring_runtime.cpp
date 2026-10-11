// Port of skate-game/src/scoring_runtime.rs.
//! ScoreModule integration. The simulation owns recognition and accounting;
//! the APT movie only consumes the resulting publication.
// SIMREF: HUD coupling (apt_vm::Value, hud_runtime::Input, Runtime::hud_input) removed.
#include "s3/game/scoring_runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <tuple>

namespace s3::game::scoring_runtime {

namespace catalog = ::s3::scoring::catalog;
using ::s3::scoring::Scorable;
using ::s3::scoring::carrier::delay_ticks;
using ::s3::scoring::conversions::LINKS;

namespace {

/// skate-data keeps its own copy of `skate_core::scoring::Scorable`.
Scorable to_core(const sd::Scorable& s) {
    Scorable out;
    out.id = s.id;
    out.class_ = s.class_;
    out.score_type = s.score_type;
    return out;
}

::s3::scoring::session::Rules to_core(const sd::SessionRules& r) {
    ::s3::scoring::session::Rules out;
    out.combo_capacity = r.combo_capacity;
    out.combo_levels = r.combo_levels;
    out.combo_refresh_threshold = r.combo_refresh_threshold;
    out.line_capacity = r.line_capacity;
    out.bail_factor = r.bail_factor;
    return out;
}

std::uint32_t saturating_add(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t r = a + b;
    return r < a ? UINT32_MAX : r;
}

// ---- trace formatting (diagnostics only) ----
std::string fixed(float x, int precision) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", precision, static_cast<double>(x));
    return buf;
}
std::string debug_str(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}
const char* collector_name(Collector c) {
    switch (c) {
        case Collector::None: return "None";
        case Collector::Ground: return "Ground";
        case Collector::Air: return "Air";
        case Collector::Grind: return "Grind";
        case Collector::Offboard: return "Offboard";
        case Collector::Handplant: return "Handplant";
    }
    return "?";
}
const char* category_name(FilteredCategory c) {
    switch (c) {
        case FilteredCategory::Invalid: return "Invalid";
        case FilteredCategory::Ground: return "Ground";
        case FilteredCategory::Air: return "Air";
        case FilteredCategory::Grind: return "Grind";
        case FilteredCategory::Wipeout: return "Wipeout";
        case FilteredCategory::Teleport: return "Teleport";
        case FilteredCategory::Offboard: return "Offboard";
        case FilteredCategory::OffboardAir: return "OffboardAir";
    }
    return "?";
}
std::string hex8(std::uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08x", v);
    return buf;
}

using Descriptor = std::tuple<std::size_t, std::uint32_t, std::size_t>;

}  // namespace

bool decorates_spin(std::uint32_t trick_type) {
    switch (trick_type) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 7:
        case 10:
        case 12:
        case 13:
            return true;
        default:
            return false;
    }
}

std::string compose_trick_name(const char* base, std::int32_t spin_turns, std::int32_t flip_direction, bool switch_,
                               bool goofy, bool decorate_spin) {
    // 0x825E53C4: a spin's side is the parity of switch, the spin's own sign and stance.
    const bool backside = switch_ ^ (spin_turns < 0) ^ goofy;
    std::string name = "#";
    if (base != nullptr) {
        name += base;
    } else if (spin_turns == 0) {
        name += trick_tokens::AIR;
    } else if (backside) {
        name += trick_tokens::BS_SPIN;
    } else {
        name += trick_tokens::FS_SPIN;
    }
    // 82DA8BE0 keeps the rotation as signed half-turns; the display prints degrees --
    // but only for a trick class that is allowed to carry one (0x825E5700).
    if (spin_turns != 0 && decorate_spin) {
        name.push_back(' ');
        const std::int64_t magnitude = spin_turns < 0 ? -std::int64_t(spin_turns) : std::int64_t(spin_turns);
        name += std::to_string(std::int32_t(std::uint32_t(magnitude) * 180u));
    }
    // 0x825E57A8: the suffix is chosen by the sign of the air collector's +2348, and a
    // zero -- no flip, or a flip with no grab to record its side -- appends nothing.
    if (flip_direction > 0) {
        name.push_back(' ');
        name += trick_tokens::FRONTFLIP;
    } else if (flip_direction < 0) {
        name.push_back(' ');
        name += trick_tokens::BACKFLIP;
    }
    return name;
}

void ContextRun::advance(std::array<float, 3> position, bool active) {
    if (active && !running) {
        current = 0.0f;
        running = true;
        start = position;
    }
    if (!running) {
        return;
    }
    // The lane mask before the length drops Y, as the air's own distance metric does.
    const float dx = position[0] - start[0];
    const float dz = position[2] - start[2];
    const float distance = rs::hypot(dx, dz);
    if (active) {
        current = distance;
        return;
    }
    banked += distance;
    current = 0.0f;
    running = false;
}

float dot3(std::array<float, 3> a, std::array<float, 3> b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

std::array<float, 3> cross3(std::array<float, 3> a, std::array<float, 3> b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    };
}

std::optional<std::array<float, 3>> normalize3(std::array<float, 3> v) {
    const float squared = dot3(v, v);
    // 8296EC98 rejects either input below 1e-4 (0x8209BE90) and returns a zero angle.
    if (!(squared > 1e-4f)) return std::nullopt;
    const float inverse = rs::recip(rs::sqrt(squared));
    return std::array<float, 3>{v[0] * inverse, v[1] * inverse, v[2] * inverse};
}

float spin_angle(std::array<float, 3> reference_up, std::array<float, 3> reference_forward,
                 std::array<float, 3> forward) {
    const auto normal = normalize3(cross3(reference_up, forward));
    if (!normal) return 0.0f;
    const auto flattened = normalize3(cross3(*normal, reference_up));
    if (!flattened) return 0.0f;
    const auto previous = normalize3(reference_forward);
    if (!previous) return 0.0f;
    const float cosine = dot3(*flattened, *previous);
    // The discontinuity guard: a jump of more than 90 degrees scores nothing rather than a
    // bogus angle.
    if (cosine < 0.0f) {
        return 0.0f;
    }
    float angle = rs::acos(rs::clamp(cosine, -1.0f, 1.0f));
    if (dot3(cross3(*flattened, *previous), reference_up) < 0.0f) {
        angle = rs::f32::consts::TAU - angle;
    }
    // 82DAC880's wrap into (-pi, pi].
    if (angle > rs::f32::consts::PI) {
        return angle - rs::f32::consts::TAU;
    }
    return angle;
}

bool trace_enabled() {
    static const bool enabled = std::getenv("SKATE_SCORING_TRACE") != nullptr;
    return enabled;
}

rs::Result<Runtime, std::string> Runtime::load(const sd::Collections& data) {
    S3G_TRY(ScoringData scoring, ScoringData::load(data));
    Runtime out;
    out.data = std::move(scoring);
    return out;
}

std::pair<bool, bool> Runtime::classify_landing_audio(::s3::animation::landing_quality::Output landing) const {
    if (!landing.landing_data_167) {
        return {false, false};
    }
    const bool clean = landing.landing_type_96 == 0;
    const bool sketchy = landing.landing_type_96 == 1 && landing.sideways_speed_84 > data.sketchy_side_speed &&
                         rs::abs(landing.spin_92) > 0.5f;
    return {clean, sketchy};
}

float Runtime::penalty(std::size_t id) const {
    const sd::Definition* d = data.by_id(id);
    if (d == nullptr) {
        return 1.0f;
    }
    const Scorable metadata = to_core(d->metadata);
    if (metadata.repetition_applies()) {
        const auto count = session.holder.repetition_count(metadata);
        return data.repetition.evaluate(float(count.value_or(std::int8_t(0))));
    }
    return 1.0f;
}

void Runtime::finish(std::size_t slot, bool complete, bool keep_metric) {
    if (carriers[slot].has_value()) {
        Carrier carrier = *carriers[slot];
        carriers[slot].reset();
        if (complete) {
            carrier.complete(data.unannounced_factor);
            if (collector == Collector::Air) {
                if (trace_enabled()) {
                    log::eprintln(fmt::cat("SCORE_TRICK air slot=", slot, " id=", carrier.scorable.id,
                                           " class=", carrier.scorable.class_, " type=", carrier.scorable.score_type,
                                           " points=", carrier.points, " factor=", fixed(carrier.factor, 3),
                                           " reward=", fixed(carrier.reward, 1), " announced=", carrier.announced,
                                           " bonus_paid=", flip_bonus_paid));
                }
                session.holder.end_trick(carrier.scorable, carrier.reward);
            } else {
                const bool metric =
                    collector == Collector::Grind || (collector == Collector::Ground && slot < 3);
                if (!(keep_metric && collector == Collector::Ground)) {
                    float reward = 0.0f;
                    if (keep_metric) {
                        reward = 0.0f;
                    } else if (metric) {
                        if (carrier.scorable.score_type == 9) {
                            reward = rs::max(carrier.reward, metric_rewards[slot]);
                        } else {
                            reward = rs::max(metric_rewards[slot], 0.0f);
                        }
                    } else {
                        reward = carrier.reward;
                    }
                    if (trace_enabled()) {
                        const auto rep = session.holder.repetition_count(carrier.scorable);
                        const std::string rep_s =
                            rep ? fmt::cat("Some(", int(*rep), ")") : std::string("None");
                        log::eprintln(fmt::cat(
                            "SCORE_TRICK slot=", slot, " id=", carrier.scorable.id, " class=", carrier.scorable.class_,
                            " type=", carrier.scorable.score_type, " points=", carrier.points,
                            " factor=", fixed(carrier.factor, 3), " carrier=", fixed(carrier.reward, 1),
                            " metric=", fixed(metric_rewards[slot], 1), " credited=", fixed(reward, 1), " rep=", rep_s,
                            " announced=", carrier.announced, " held=", fixed(held[slot], 2),
                            " dist=", fixed(distance[slot], 2)));
                    }
                    session.holder.credit_trick(carrier.scorable, reward);
                }
            }
        }
    }
    if (!keep_metric) {
        metric_started[slot] = false;
        held[slot] = 0.0f;
        distance[slot] = 0.0f;
        metric_rewards[slot] = 0.0f;
    }
}

Status Runtime::carrier(std::size_t slot, std::optional<std::size_t> id, const Frame& f) {
    const std::optional<std::size_t> current_id =
        carriers[slot] ? std::optional<std::size_t>(carriers[slot]->scorable.id) : std::nullopt;
    if (current_id != id) {
        std::optional<std::size_t> conversion;
        if (id && collector == Collector::Air && carriers[slot].has_value() && LINKS.at(*id).first >= 0) {
            conversion = id;
        }
        bool chained_grab = false;
        if (collector == Collector::Air && id) {
            const sd::Definition* d = data.by_id(*id);
            if (d != nullptr && d->metadata.class_ == 2) {
                chained_grab = carriers[slot].has_value() && carriers[slot]->scorable.class_ == 2 &&
                               !carriers[slot]->announced;
            }
        }
        const std::optional<std::uint32_t> previous_tick =
            carriers[slot] ? std::optional<std::uint32_t>(carriers[slot]->start_tick) : std::nullopt;
        const bool keep_metric =
            id.has_value() && (collector == Collector::Grind || (collector == Collector::Ground && slot == 0));
        if (!conversion) {
            if (chained_grab) {
                carriers[slot].reset();
                grab_chain += 1;
            } else {
                finish(slot, true, keep_metric);
                grab_chain = 0;
            }
        }
        if (id) {
            const sd::Definition* d = data.by_id(*id);
            if (d == nullptr) {
                return rs::make_err(fmt::cat("Missing native scorable ", *id));
            }
            const float factor = penalty(*id) * (collector == Collector::Air ? air_factor : 1.0f);
            const float announcement_threshold = data.announcement.evaluate(float(d->points));
            const std::uint32_t start_tick = chained_grab ? previous_tick.value_or(f.tick) : f.tick;
            const std::uint32_t delay =
                delay_ticks(d->completion_delay, grab_chain > 1 ? data.collector.scalar(0x698) : 0.0f);
            Carrier next = Carrier::new_(to_core(d->metadata), d->points, factor, announcement_threshold, start_tick,
                                         delay, f.switch_, f.fakie);
            if (carriers[slot].has_value()) {
                carriers[slot]->convert_to(next, data.unannounced_factor);
                modified_trick = true;
            }
            if (conversion) {
                base_trick_label = d->label;
                base_trick_type = d->trick_type;
            }
            carriers[slot] = next;
            sequence_active = true;
            idle_ticks = 0;
        }
    }
    const float carrier_penalty = carriers[slot] ? penalty(carriers[slot]->scorable.id) : 1.0f;
    if (carriers[slot].has_value()) {
        Carrier& c = *carriers[slot];
        if (c.announce(f.tick, data.unannounced_factor)) {
            const sd::Definition* d = data.by_id(c.scorable.id);
            if (d == nullptr) {
                return rs::make_err(std::string("Missing announced scorable"));
            }
            base_trick_label = d->label;
            // 825E51A0 resolves the *named* scorable's record and reads its TrickType
            // from desc+120 to decide whether the name may carry a spin. Setting the
            // label here without the type left the type at whatever a previous flip
            // ladder conversion had put there -- 0 for any ordinary trick, which
            // `decorates_spin` refuses, so no spin ever reached a trick name.
            base_trick_type = d->trick_type;
            stance = {f.switch_, f.fakie, f.nollie, false};
            new_trick = true;
            if (collector == Collector::Air && !air_repetition_set) {
                air_repetition = carrier_penalty;
                air_repetition_set = true;
            }
        }
        const bool distance_metric =
            collector == Collector::Grind || (collector == Collector::Ground && slot < 3);
        const bool was_active = metric_started[slot];
        metric_started[slot] = true;
        if (c.announced && !(collector == Collector::Air && f.suspend_air)) {
            // 82DB0588 starts a distance collector at time/distance zero.
            // Every active frame still updates its reference position.
            if (!distance_metric || was_active) {
                held[slot] += f.dt;
            }
            const float previous_distance = distance[slot];
            std::array<float, 3> displacement{};
            for (std::size_t i = 0; i < 3; ++i) displacement[i] = f.position[i] - previous[i];
            float delta = 0.0f;
            if (collector == Collector::Ground && slot == 1) {
                float s = -0.0f;
                for (std::size_t i = 0; i < 3; ++i) s += displacement[i] * f.forward[i];
                delta = s;
            } else {
                float s = -0.0f;
                for (std::size_t i = 0; i < 3; ++i) s += displacement[i] * displacement[i];
                delta = rs::sqrt(s);
            }
            if (was_active && delta > rs::from_bits(0x3ba3d70au)) {
                distance[slot] += delta;
            }
            std::optional<std::uint16_t> curve;
            if (collector == Collector::Air && c.scorable.class_ == 2) {
                curve = 0x460;
            } else if (collector == Collector::Handplant) {
                curve = 0x140;
            }
            if (curve) {
                c.reward += (data.collector.curve(*curve, held[slot]) * f.dt) * c.announcement_threshold;
            }
            std::optional<std::pair<std::uint16_t, std::uint16_t>> metric_curve;
            if (collector == Collector::Ground && c.scorable.score_type == 9) {
                metric_curve = std::make_pair(std::uint16_t(0x000), std::uint16_t(0x050));
            } else if (collector == Collector::Ground && c.scorable.score_type == 8) {
                metric_curve = std::make_pair(std::uint16_t(0x0f0), std::uint16_t(0x0a0));
            } else if (collector == Collector::Ground && slot == 2) {
                metric_curve = std::make_pair(std::uint16_t(0x230), std::uint16_t(0x1e0));
            } else if (collector == Collector::Grind) {
                metric_curve = std::make_pair(std::uint16_t(0x2d0), std::uint16_t(0x280));
            }
            if (metric_curve) {
                const auto [distance_curve, time_curve] = *metric_curve;
                const float gained = data.collector.curve(distance_curve, distance[slot]);
                const float before = data.collector.curve(distance_curve, previous_distance);
                const float distance_delta = rs::max(gained - before, 0.0f);
                const float time_value = data.collector.curve(time_curve, held[slot]);
                metric_rewards[slot] = rs::mul_add(rs::mul_add(time_value, f.dt, distance_delta),
                                                   c.announcement_threshold, metric_rewards[slot]);
            }
        }
    }
    return ok();
}

void Runtime::advance_context_runs(const Frame& f) {
    const std::uint32_t tag = f.hips_surface;
    std::array<bool, 4> gates{};
    if (f.hips_ground) {
        const std::array<float, 3> contact = *f.hips_ground;
        const float reference = rs::min(takeoff_hips_height, f.hips_position[1]);
        const float drop = reference - contact[1];
        gates = {
            (tag & 0x7fu) == 10u,
            ((tag >> 7) & 0x1fu) == 8u,
            drop > data.collector.scalar(0x680),
            drop > data.collector.scalar(0x67c),
        };
    }
    for (std::size_t i = 0; i < 4; ++i) {
        context_runs[i].advance(f.hips_position, gates[i]);
    }
}

float Runtime::context_reward() const {
    float sum = -0.0f;
    for (std::size_t i = 0; i < 4; ++i) {
        sum += data.collector.curve(CONTEXT_CURVES[i], context_runs[i].total());
    }
    return sum;
}

void Runtime::pay_flip_bonus(const Frame& f) {
    if (flip_bonus_paid) {
        return;
    }
    if (!carriers[0].has_value()) {
        return;
    }
    const Carrier& carrier = *carriers[0];
    // 82DA93D8 reads the carrier's presence byte +124, not its announcement: an
    // unannounced carrier has not credited its points yet, and the bonus goes onto the
    // same reward those points will land in.
    if (carrier.scorable.class_ != 3) {
        return;
    }
    if (!f.hips_ground) {
        return;
    }
    const std::array<float, 3> contact = *f.hips_ground;
    std::array<float, 3> to_deck{};
    std::array<float, 3> to_hips{};
    for (std::size_t i = 0; i < 3; ++i) to_deck[i] = f.position[i] - contact[i];
    for (std::size_t i = 0; i < 3; ++i) to_hips[i] = f.hips_position[i] - contact[i];
    auto dot = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        float s = -0.0f;
        for (std::size_t i = 0; i < 3; ++i) s += a[i] * b[i];
        return s;
    };
    // 820D71E8 = 0.01, compared against the squared lengths rather than the lengths.
    const float epsilon = rs::from_bits(0x3C23D70Au);
    if (!(dot(to_deck, to_deck) > epsilon && dot(to_hips, to_hips) > epsilon)) {
        return;
    }
    if (!(dot(to_deck, to_hips) < 0.0f)) {
        return;
    }
    const float bonus = (data.collector.scalar(0x600) * float(carrier.points)) * carrier.factor;
    carriers[0]->reward += bonus;
    flip_bonus_paid = true;
}

bool Runtime::sequence_continues(const Frame& f) const {
    // The scoring-trick bit is a pulse, not a latch: over a measured session it was set
    // on 4.8% of frames and never for more than 13 grounded frames in a row. That pulse
    // is what bridges the gap between a manual's carrier ending and the air beginning.
    const bool announced = (f.flags & 0x01000000u) != 0;
    bool carried = false;
    for (const auto& c : carriers) carried = carried || c.has_value();
    switch (collector) {
        case Collector::None:
            return false;
        case Collector::Grind:
        case Collector::Handplant:
            return true;
        case Collector::Air:
            return collector_ticks > 5 || announced;
        case Collector::Offboard:
            return carried;
        case Collector::Ground:
            return announced || carried || (f.flags & 0x30000000u) != 0 ||
                   (manual_block_ticks > 0 && manual_block_ticks <= 6);
    }
    return false;
}

Status Runtime::advance(const Frame& f) {
    new_trick = false;
    modified_trick = false;
    close_tricks = false;
    std::optional<Descriptor> descriptor;
    if (f.descriptor) {
        if (const sd::Definition* d = data.by_name(f.descriptor->_0)) {
            const std::size_t did = d->metadata.id;
            if (!((f.flags & 0x02000000u) != 0 && (did == 67 || did == 68 || did == 69))) {
                descriptor = Descriptor{d->metadata.id, d->metadata.class_, d->metadata.score_type};
            }
        }
    }
    Collector next = Collector::None;
    switch (f.category) {
        case FilteredCategory::Ground: next = Collector::Ground; break;
        case FilteredCategory::Air: next = Collector::Air; break;
        case FilteredCategory::Grind: next = Collector::Grind; break;
        case FilteredCategory::Offboard:
        case FilteredCategory::OffboardAir: next = Collector::Offboard; break;
        default: next = Collector::None; break;
    }
    if (f.state == 600 && (next == Collector::Ground || next == Collector::Air)) {
        next = Collector::Handplant;
    }
    if (next == Collector::Air && collector == Collector::Ground && descriptor &&
        std::get<1>(*descriptor) == 0) {
        next = Collector::Ground;
    }
    if (next == Collector::Ground && collector == Collector::Air && descriptor &&
        std::get<1>(*descriptor) == 3) {
        next = Collector::Air;
    }
    if (f.teleported) {
        next = Collector::None;
    }
    if (trace_enabled() && (next != collector || descriptor.has_value())) {
        const std::string desc_s =
            descriptor ? fmt::cat("Some((", std::get<0>(*descriptor), ", ", std::get<1>(*descriptor), ", ",
                                  std::get<2>(*descriptor), "))")
                       : std::string("None");
        log::eprintln(fmt::cat("SCORE_STATE tick=", f.tick, " cat=", category_name(f.category), " state=", f.state,
                               " collector=", collector_name(collector), "->", collector_name(next),
                               " descriptor=", desc_s, " grind=", f.grind_id, " flags=", hex8(f.flags),
                               " switch=", f.switch_, " fakie=", f.fakie, " flip=", f.body_flip));
    }
    if (next != collector) {
        const bool complete = next != Collector::None;
        std::optional<std::size_t> previous_type;
        for (const auto& c : carriers) {
            if (c) previous_type = c->scorable.score_type;
        }
        for (std::size_t slot = 0; slot < 4; ++slot) {
            finish(slot, complete, false);
        }
        if (collector == Collector::Air) {
            if (complete) {
                // 82DA8550 banks these by bare id (`li r4,129` / `bl 0x82da6260`).
                // They have no authored record, so resolving them through the
                // authored table returned nothing and every air metric reward --
                // distance, peak height, height gain, spin and flip -- was
                // discarded at the landing that was supposed to bank it.
                const std::array<float, 5> metrics = air_metrics;
                for (std::size_t i = 0; i < metrics.size(); ++i) {
                    if (const auto metric = catalog::metadata(AIR_METRIC_BASE + i)) {
                        session.holder.end_trick(*metric, metrics[i]);
                    }
                }
                // 82DA8550 closes the gap runs and banks their total the same way:
                // `bl 0x82da88e8` / `li r4,237` / `bl 0x82da6260`.
                if (const auto metric = catalog::metadata(CONTEXT_METRIC)) {
                    session.holder.end_trick(*metric, context_reward());
                }
                if (trace_enabled()) {
                    const auto m = air_metrics;
                    log::eprintln(fmt::cat(
                        "SCORE_AIR spin_deg=", fixed(rs::to_degrees(spin), 0), " turns=", spin_turns,
                        " flip=", flip_direction, " dist=", fixed(m[0], 0), " peak=", fixed(m[1], 0),
                        " gain=", fixed(m[2], 0), " spin=", fixed(m[3], 0), " flipmetric=", fixed(m[4], 0),
                        " gaps=[", fixed(context_runs[0].total(), 1), ",", fixed(context_runs[1].total(), 1), ",",
                        fixed(context_runs[2].total(), 1), ",", fixed(context_runs[3].total(), 1),
                        "] gap_reward=", fixed(context_reward(), 0), " factor=", fixed(air_factor, 3),
                        " rep=", fixed(air_repetition, 3)));
                }
            }
            session.holder.finish_collector();
            landing_countdown = 2;
        }
        collector = next;
        grab_chain = 0;
        collector_ticks = 0;
        manual_block_ticks = 0;
        revert_id = std::nullopt;
        // Everything below belongs to the *air* collector and is cleared by its own
        // Enter, 82DA8078 -- not by leaving it. Clearing on every transition wiped the
        // spin and the flip on the landing tick, before the publication that had to
        // name them, so a 540 published as its bare label.
        if (next == Collector::Air) {
            start = f.position;
            peak = f.position[1];
            spin = 0.0f;
            spin_reference = {f.rider_up, f.rider_forward};
            air_metrics = {};
            spin_turns = 0;
            flip_direction = 0;
            context_runs = {};
            takeoff_hips_height = f.hips_position[1];
            flip_bonus_paid = false;
            flip_seen = false;
            air_repetition = 1.0f;
            air_repetition_set = false;
            air_factor = 1.0f;
            session.holder.reward_sequence(1.0f);
            landing_countdown = 0;
            if (previous_type == std::optional<std::size_t>(8)) {
                air_factor *= data.collector.scalar(0x68c);
            }
            if (previous_type == std::optional<std::size_t>(5)) {
                air_factor *= data.collector.scalar(0x690);
            }
            if (f.velocity[0] * f.velocity[0] + f.velocity[2] * f.velocity[2] <
                rs::powi(data.collector.scalar(0x66c), 2)) {
                air_factor *= data.collector.scalar(0x688);
            }
            if (f.switch_ && !f.fakie) {
                air_factor *= data.collector.scalar(0x684);
            }
            if (f.fakie && !f.switch_) {
                air_factor *= data.collector.scalar(0x694);
            }
        }
    }
    std::array<std::optional<std::size_t>, 4> ids{};
    if (!(collector == Collector::Air && f.suspend_air)) {
        collector_ticks = saturating_add(collector_ticks, 1);
    }
    switch (collector) {
        case Collector::Ground: {
            if ((f.flags & 0x80000000u) != 0) {
                ids[0] = 0;
            } else if ((f.flags & 0x40000000u) != 0) {
                ids[0] = 1;
            }
            if ((f.flags & 0x08000000u) != 0) {
                ids[1] = 2;
            } else if ((f.flags & 0x04000000u) != 0) {
                ids[1] = 3;
            }
            // 82DAA8E0, the ground collector's manual slot:
            //
            // ```text
            // r9  = [this+104]               ; the scoring flags word
            // r10 = [[[this+4]+28]+70]       ; PhysOut -> State, byte 70
            // r4  = bit4(r9) || bit5(r9)     ; nose or tail manual
            // if (r4 && r10) { [this+932]++ ; r4 = 0 }
            // else if (!bit30([holder+1832])) [this+932] = 0
            // if (r4) r4 &= ([this+932] <= 6)   ; the subfic/subfe pair
            // ```
            //
            // The gate is State+**70**. This port read State+66 instead -- the byte
            // 82D43B10, RevertGround's Fill, sets for the revert's whole active
            // lifetime -- so the manual carrier was dropped on every frame of every
            // revert. With no carrier the sequence went idle, published itself, and
            // the line then drained the multiplier back to x1: the reported "the score
            // resets if you connect in a manual". Reading the byte retail reads leaves
            // the gate clear, which is also what retail does whenever it is clear.
            //
            // The reset condition is retail's one deliberate deviation: it resets on
            // `bit30([holder+1832]) == 0`, the display-live bit this host does not
            // publish, and an empty line is the nearest state it does.
            if (ids[1].has_value() && f.manual_block_70) {
                manual_block_ticks = saturating_add(manual_block_ticks, 1);
                ids[1] = std::nullopt;
            } else if (session.line.points <= 0.0f) {
                manual_block_ticks = 0;
            }
            if (manual_block_ticks > 6) {
                ids[1] = std::nullopt;
            }
            if (ids[1].has_value()) {
                if (carriers[1]) {
                    ids[1] = carriers[1]->scorable.id;
                }
            }
            if (descriptor && std::get<0>(*descriptor) == 60 && (f.flags & 0x02000000u) != 0) {
                ids[2] = std::get<0>(*descriptor);
            }
            if ((f.flags & 0x20000000u) != 0) {
                revert_id = 4;
            } else if ((f.flags & 0x10000000u) != 0) {
                revert_id = 5;
            }
            ids[3] = f.reverting ? revert_id : std::nullopt;
            break;
        }
        case Collector::Air:
            if (descriptor && std::get<2>(*descriptor) != 5) ids[0] = std::get<0>(*descriptor);
            break;
        case Collector::Grind:
            if (f.grind_id >= 0) ids[0] = std::size_t(f.grind_id);
            break;
        // 82DABFB0, the Offboard collector: `lwz r4,52(r3)` / `cmpwi cr6,r4,234`.
        // Retail also admits it from physical state 503, which is not ported.
        case Collector::Offboard:
            if (descriptor && std::get<0>(*descriptor) == 234) ids[0] = std::get<0>(*descriptor);
            break;
        // 82DABA10, the Handplant collector: `addi r9,r27,16` / `cmpwi cr6,r6,6`,
        // i.e. any scorable whose class is Handplant_CLASS.
        case Collector::Handplant:
            if (descriptor && std::get<2>(*descriptor) == HANDPLANT_CLASS) ids[0] = std::get<0>(*descriptor);
            break;
        case Collector::None:
            break;
    }
    for (std::size_t slot = 0; slot < 4; ++slot) {
        S3G_CHECK(carrier(slot, ids[slot], f));
    }
    if (collector == Collector::Air && !f.suspend_air) {
        if (collector_ticks > 5 || (f.flags & 0x01000000u) != 0) {
            sequence_active = true;
        }
        const auto [reference_up, reference_forward] = spin_reference;
        spin += spin_angle(reference_up, reference_forward, f.rider_forward);
        // 82DAC880 stores the new matrix as the reference on every update.
        spin_reference = {f.rider_up, f.rider_forward};
        peak = rs::max(peak, f.position[1]);
        const float dx = f.position[0] - start[0];
        const float dz = f.position[2] - start[2];
        const float scale = air_factor * air_repetition;
        air_metrics[0] = data.collector.curve(0x3c0, rs::sqrt(dx * dx + dz * dz)) * scale;
        air_metrics[1] = data.collector.curve(0x410, peak - start[1]) * scale;
        air_metrics[2] = data.collector.curve(0x320, f.position[1] - start[1]) * scale;
        // 82DA8BE0 signs the turn count by the rotation's own direction and keeps it
        // at +2344; the reward takes its magnitude.
        const float degrees = rs::to_degrees(spin);
        const std::int32_t turns = rs::as_i32((rs::abs(degrees) + data.collector.scalar(0x63c)) / 180.0f);
        spin_turns = degrees < 0.0f ? std::int32_t(0u - std::uint32_t(turns)) : turns;
        air_metrics[3] =
            data.collector.curve(0x370, float(std::int32_t(std::uint32_t(turns) * 180u))) * scale;
        // 82DA8EB8. The +2396 latch, and with it scoring output byte 14651, is set on
        // the first flipping frame whatever the current carrier is -- that is what the
        // trick display names a flip from. Only +2348, and so the reward, waits for an
        // announced grab carrier.
        if (f.body_flip) {
            flip_seen = true;
            if (carriers[0] && carriers[0]->announced && carriers[0]->scorable.class_ == 2) {
                // 82DA8EB8: `subfic r9,r11,0` sets CA = (side == 0); `subfe r6,r9,r9`
                // makes 0 or -1; `rlwinm r11,r6,0,30,30` makes 0 or 2; `addi r9,r11,-1`
                // lands on -1 for side == 0 and +1 for side != 0. Getting this the
                // wrong way round named every front flip a back flip.
                flip_direction = f.body_flip_side ? 1 : -1;
            }
        }
        // Recomputed from the latched +2348 every frame, flipping or not, so the
        // reward survives the end of the grab that authorised it.
        const std::int32_t flip_abs = flip_direction < 0 ? -flip_direction : flip_direction;
        air_metrics[4] = float(flip_abs) * data.collector.scalar(0x640) * scale;
        pay_flip_bonus(f);
        advance_context_runs(f);
    }
    bool any_carrier = false;
    for (const auto& c : carriers) any_carrier = any_carrier || c.has_value();
    const bool active = any_carrier || collector == Collector::Air;
    idle_ticks = active ? 0 : saturating_add(idle_ticks, 1);
    if (session.holder.has_pending_sequence()) {
        if (f.landing.landing_data_167) {
            clean = f.landing.landing_type_96 == 0;
            sketchy = f.landing.landing_type_96 == 1 && f.landing.sideways_speed_84 > data.sketchy_side_speed &&
                      rs::abs(f.landing.spin_92) > 0.5f;
        }
        if (landing_countdown == 0) {
            float factor = 1.0f;
            if (f.switch_ && !f.fakie) {
                factor *= data.collector.scalar(0x61c);
            }
            if (f.fakie && !f.switch_) {
                factor *= data.collector.scalar(0x62c);
            }
            if (collector == Collector::Grind) {
                factor *= data.collector.scalar(0x628);
            }
            if (clean) {
                factor *= data.collector.scalar(0x630);
            }
            if (sketchy) {
                factor *= data.collector.scalar(0x620);
            }
            session.holder.reward_sequence(factor);
        } else {
            landing_countdown -= 1;
        }
    }
    std::pair<std::uint16_t, std::uint16_t> scales;
    switch (collector) {
        case Collector::Air: scales = {0x668, 0x664}; break;
        case Collector::Grind: scales = {0x618, 0x614}; break;
        case Collector::Handplant: scales = {0x608, 0x604}; break;
        default: scales = {0x610, 0x60c}; break;
    }
    const float line_scale = active ? data.collector.scalar(scales.first) : 1.0f;
    const float combo_scale = active ? data.collector.scalar(scales.second) : 1.0f;
    // 82DA33E0 calls 82DA4C28 twice, and the two calls do *not* share a hold flag.
    //
    // ```text
    // r29 = ([module+4] == [module+16])   ; the active collector is the ground one
    // r5  = bit30 of [holder+1832]        ; the trick display is live
    // if (!r5)          r30 = 0           ; no hold at all
    // else if (!r29)    r30 = 1           ; any other collector holds without a bound
    // else              r30 = [module+112] < (int)([module+64]+1532 * 60.0)
    // bl 0x82da4c28     ; r3 = module+40, the line timer, r5 = r30
    // [module+112] = (returned hold && r29) ? [module+112] + 1 : 0
    // li r5,0
    // bl 0x82da4c28     ; r3 = module+44, the combo timer -- never held
    // ```
    //
    // The collector at module+16 is the ground one: 82DA3C68 writes its byte+945 every
    // frame, and 82DAA8E0, which reads that same byte's object, is what tests the
    // nose/tail manual bits 0x08000000/0x04000000. Its authored bound is
    // `0x5fc` = 5.0 seconds times the 60.0 at 0x8303745C, so 300 frames.
    //
    // Passing "a collector is active" to both calls, as this did, held the line just
    // above one point for as long as *any* ground carrier existed -- a manual, a
    // powerslide, a revert -- so the line never expired and the score and trick name
    // stayed on screen indefinitely. It also held the multiplier timer, which retail
    // never does.
    //
    // `bit30 of [holder+1832]` is the gate, and it is not a constant: 82DA48B8 sets it
    // when it republishes the line to the display, 82DA37B0 clears it every frame, and
    // 82DA37B0 only calls 82DA48B8 while a line is open -- when the active collector
    // says the sequence has stopped, or while `[module+129]` holds. So once a sequence
    // has been banked and nothing further is happening, the bit goes clear and the line
    // gets **no hold at all**: it drains out and the display fades. That is the retail
    // behaviour of a score left alone.
    //
    // This host publishes no display-live bit, so `sequence_active` stands in for it --
    // the hold exists to stop a line expiring underneath a trick in progress, and that
    // is exactly when a sequence is open. Taking the bit as always set, as this first
    // did, kept re-arming the hold long after the last trick and the score sat there.
    const bool grounded = collector == Collector::Ground;
    const std::uint32_t hold_limit = rs::as_u32(data.collector.scalar(0x5fc) * 60.0f);
    const bool hold_line = sequence_active && (!grounded || ground_hold_ticks < hold_limit);
    const bool line_held = session.line.advance(f.dt, data.line_drain, line_scale, hold_line);
    ground_hold_ticks = (line_held && grounded) ? saturating_add(ground_hold_ticks, 1) : 0;
    session.combo.timer.advance(f.dt, data.combo_drain, combo_scale, false);
    const bool bailout = collector == Collector::None && sequence_active;
    // 82DA37B0 asks the *active collector* whether the sequence continues, through its
    // vtable+20, and publishes when the answer is no. This port used to publish after
    // three carrier-less frames instead, which cut a sequence apart at the moment you
    // popped out of a manual and gave a grind no continuation at all.
    const bool continues = sequence_continues(f);
    if (sequence_active && (bailout || (!continues && landing_countdown == 0))) {
        if (bailout) {
            session.holder.cancel_pending();
        }
        sequence_score = session.publish_sequence(to_core(data.session_rules()), 1.0f, bailout, true);
        // 827743A0 runs every frame and, on the frame a trick finishes without a bail
        // (O[14626] set, O[14640]/O[14630] clear), calls [B]+92 = 825019B8, which zeroes
        // M+12 and M+16 and drops the module back to state 0. That is what stops the
        // next trick inheriting this one's spin and flip.
        if (!bailout) {
            published_spin_turns = 0;
            published_flip_direction = 0;
        }
        if (trace_enabled()) {
            const auto& s = session.holder.snapshot;
            log::eprintln(fmt::cat("SCORE_PUBLISH reward=", fixed(sequence_score, 0),
                                   " mult=", fixed(session.combo.multiplier, 2), " line=", fixed(s.line, 0),
                                   " lifetime=", fixed(s.completed_lines, 0), " bail=", bailout,
                                   " name=", debug_str(trick_name)));
        }
        sequence_active = false;
        // 82775328 -> 82774E88 closes only for ScoreModule reset/bail
        // output 14630 (82DA4010/82DA4238), not a banked landing.
        close_tricks = bailout;
    } else if (sequence_active) {
        const auto& s = session.holder.snapshot;
        float carried = -0.0f;
        for (std::size_t i = 0; i < 4; ++i) {
            if (!carriers[i]) continue;
            const Carrier& c = *carriers[i];
            float v = 0.0f;
            if (collector == Collector::Grind || (collector == Collector::Ground && i < 3)) {
                if (c.scorable.score_type == 9) {
                    v = rs::max(c.reward, metric_rewards[i]);
                } else {
                    v = metric_rewards[i];
                }
            } else {
                v = c.reward;
            }
            carried += v;
        }
        float metrics = -0.0f;
        for (const float m : air_metrics) metrics += m;
        sequence_score = (s.accumulated + s.general_pending + s.fingerflip_pending + carried + metrics +
                          context_reward()) *
                         session.combo.multiplier;
    }
    if (session.line.expired || f.teleported || bailout) {
        sequence_score = 0.0f;
    }
    session.settle_line(f.teleported || bailout, active);
    previous = f.position;
    // The multiplier poll, after settlement so a line that drops it back to x1 is seen as
    // the change it is. The cached copy is updated whether or not anything reacted, which
    // is 82666BC0's unconditional `stfs f31,52(r31)`.
    const float multiplier = session.combo.multiplier;
    multiplier_changed = multiplier != published_multiplier ? std::optional<float>(multiplier) : std::nullopt;
    published_multiplier = multiplier;
    // The name is rebuilt rather than assigned once, because the spin and the flip that
    // go into it keep accumulating after the record that named the trick was announced.
    // 82666BC0's counterpart recomposes whenever its dirty flag is set, for the same
    // reason. A closed display drops the base label so the next air starts clean.
    if (close_tricks) {
        base_trick_label = std::nullopt;
        base_trick_type = 0;
    }
    // Only the Air collector's slot 9 (82DA9A18) ever publishes the spin and the flip,
    // and only while it is the active collector. Every other collector leaves them
    // untouched, so they must not be refreshed from the air's live values here.
    const std::int32_t previous_spin = published_spin_turns;
    const std::int32_t previous_flip = published_flip_direction;
    if (collector == Collector::Air) {
        published_spin_turns = spin_turns;
        published_flip_direction = flip_direction;
    }
    trick_name = compose_trick_name(base_trick_label ? base_trick_label->c_str() : nullptr, published_spin_turns,
                                    published_flip_direction, f.switch_,
                                    // M+187 is the negation of PlayerStance_IsRegular. This engine publishes no
                                    // stance, so the parity is that of a regular skater; a goofy skater's spins
                                    // will read FS for BS until one is published.
                                    false, decorates_spin(base_trick_type));
    // 825E4F40 compares M+12 and M+16 against the module's cached copies and sets the
    // dirty byte 159 only when one of those two differs; 825E51A0 then recomposes and
    // re-fires the display event. Firing on any change to the whole composed string
    // also caught switch-parity flips, which re-triggered the display for no reason.
    if (published_spin_turns != previous_spin || published_flip_direction != previous_flip) {
        modified_trick = true;
    }
    // Every trace above is edge-triggered, and a line timer pinned just above one
    // point by the near-one hold looks exactly like a healthy line unless it is
    // sampled on every tick. 82DA33E0 bounds that hold with the counter at
    // module+112; this is how we see whether it is running at all.
    if (trace_enabled()) {
        log::eprintln(fmt::cat(
            "SCORE_TICK tick=", f.tick, " col=", collector_name(collector), " flags=", hex8(f.flags),
            " rev=", f.reverting, " mblock=", f.manual_block_70, " active=", active, " idle=", idle_ticks,
            " land=", landing_countdown, " hold=", hold_line, " groundhold=", ground_hold_ticks,
            " line=", fixed(session.line.points, 2), " line_s=", fixed(session.line.points / data.line_drain, 2),
            " combo=", fixed(session.combo.timer.points, 2), " mult=", fixed(session.combo.multiplier, 2),
            " seq=", sequence_active, " bail=", bailout, " close=", close_tricks, " name=", debug_str(trick_name)));
    }
    return ok();
}

}  // namespace s3::game::scoring_runtime
