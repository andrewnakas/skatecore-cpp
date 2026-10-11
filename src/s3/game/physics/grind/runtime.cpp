// Port of skate-game/src/physics/grind/runtime.rs.
#include "s3/game/physics/grind/runtime.hpp"

namespace s3::game::physics::grind::runtime {

rs::Result<Runtime, std::string> Runtime::load(const sd::Collections& data) {
    Runtime r;
    for (auto& s : r.states) s = state::State::new_();
    r.active = std::nullopt;
    r.nonspecific_active = false;
    r.nonspecific_jumped = false;
    r.nonspecific_jump_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    r.orientation_random = rng::OrientationRandom::new_();
    S3G_TRY(r.settings, settings::Settings::load(data));
    r.manager = std::nullopt;
    r.pending_wipeout_impulse = std::nullopt;
    r.chromosome = grind_chromosome::Chromosome::uninitialized();
    return r;
}

std::array<std::uint32_t, 3> Runtime::take_orientation_noise() { return orientation_random.take_three(); }

/// Call immediately after the real input manager finishes its Post phase.
void Runtime::observe(const ManagerObservation& manager_) { manager = manager_; }

std::optional<output::SideEffects> Runtime::fill_physical(const ManagerObservation& manager_,
                                                          GrindOutputFields& out) const {
    if (!active) return std::nullopt;
    const Family family = *active;
    return output::fill_fields(family, states[index(family)].output, manager_, out);
}

Status Runtime::condition_outputs(const grind_chromosome::Input& input, GrindOutputFields& out) {
    //Native leaves saved-fakie144 unwritten. Host lifecycle explicitly seeds
    //only that byte from the first completed packet; later history wins.
    chromosome.initialize_host_fakie(input.fakie_155);
    if (const auto publication = chromosome.update(input)) {
        grind_chromosome::publish(*publication, out);
    }
    return ok();
}

rs::Result<GrindState, std::string> Runtime::filtered_output(const GrindOutputFields& out) {
    if (!out.animation_name_156) return rs::make_err(std::string("Missing native grind name reset/publication"));
    if (!out.scoring_name_176) {
        return rs::make_err(std::string("Missing native scoring grind name reset/publication"));
    }
    GrindState s;
    s.kind = static_cast<std::int32_t>(out.words_136_140[0]);
    s.scorable_id = static_cast<std::int32_t>(out.scorable_id_152);
    s.name = *out.animation_name_156;
    s.scoring_name = *out.scoring_name_176;
    s.on_front = out.flag_318 != 0;
    s.crouch = out.crouch_132;
    s.pathed_guid = out.spline_guids_224_232[0];
    s.local_guid = out.spline_guids_224_232[1];
    return s;
}

}  // namespace s3::game::physics::grind::runtime
