// Port of skate-game/src/physics/biped_ground/grab_runtime.rs
//! PlayerGrabSpline82D73FC0/74270/740F8/749D0, distinct from possession.
//! Scene execution is concrete; retained results become visible only at Sync.
#include "s3/game/physics/biped_ground/grab_runtime.hpp"

#include <utility>

#include "s3/game/physics/offboard/grab_scene.hpp"

namespace s3::game::physics::biped_ground::grab_runtime {

void Owner::query(Query query) {
    query_position = query.position;
    query_result.reset();  //8275FA78 clears ready before enqueue.
    queries.push_back(query);
}

void Owner::request_primary(Descriptor descriptor) {
    requests[0] = descriptor;
    data[0].reset();
    data_ready[0] = false;
}

///8275FD00 captures five actual forward lines; query execution is deferred.
void Owner::request_interactable(Frame frame, QueryContext context) {
    std::array<Line, 5> lines{};
    for (std::size_t i = 0; i < 5; ++i) {
        Vector start = frame[3];
        start[1] += static_cast<float>(i) * rs::from_bits(0x3eb851ebu);
        Vector end{};
        for (std::size_t axis = 0; axis < 4; ++axis) end[axis] = rs::mul_add(frame[2][axis], 2.0f, start[axis]);
        Line line{};
        line.start = start;
        line.end = end;
        line.radius = 0.2f;
        line.group = context.matching_id_2952;
        line.reject_flags = 0;
        line.source_pool_mask = 2;
        line.selection_flags = context.selection_flags_2948;
        lines[i] = line;
    }
    //The native manager keeps only its first outstanding interactable batch.
    if (!interactable_request) {
        interactable_request = lines;
    }
    interactable_result.reset();
}

///82760060: bounded queries, requested data, then interactable results.
Status Owner::execute_queries(const Scene& scene) {
    for (const auto& q : queries) {
        S3G_TRY(auto records, scene.query(q));
        query_result = std::move(records);
    }
    queries.clear();
    for (std::size_t i = 0; i < 2; ++i) {
        if (requests[i]) {
            const Descriptor descriptor = *requests[i];
            requests[i].reset();
            S3G_TRY(auto resolved, scene.resolve(descriptor));
            data[i] = std::move(resolved);
            data_ready[i] = true;
        }
    }
    if (interactable_request) {
        const auto lines = *interactable_request;
        interactable_request.reset();
        float nearest = rs::f32::MAX;
        std::optional<std::uint32_t> object;
        for (const auto& line : lines) {
            S3G_TRY(auto hit, scene.line(line));
            if (hit) {
                if (hit->fraction != 0.0f && hit->fraction < nearest && hit->assembly.has_value()) {
                    if (auto id = scene.eligible_object(*hit)) {
                        nearest = hit->fraction;
                        object = *id;
                    }
                }
            }
        }
        interactable_result = object;
    }
    return ok();
}

///82D74270: old validation is consumed BEFORE new query results spawn lines.
Status Owner::sync(const Scene& scene, QueryContext context) {
    flags_12836 &= static_cast<std::uint8_t>(~0x40u);
    if (validation) {
        const auto hits = std::move(*validation);
        validation.reset();
        if ((flags_12836 & 0x80u) != 0) {
            validate(pending, hits);
            //82D74888 refreshes geometry by descriptor from newest results.
            if (query_result) {
                const auto& latest = *query_result;
                for (auto& old : pending) {
                    for (const auto& r : latest) {
                        if (r.descriptor().kind == old.descriptor().kind && r.descriptor().id == old.descriptor().id) {
                            old = r;
                            break;
                        }
                    }
                }
            }
        }
        validated = pending;
        flags_12836 = static_cast<std::uint8_t>(((flags_12836 >> 1) & 0x40u) | (flags_12836 & 0x3fu));
        validation_position = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (query_result) {
        auto results = std::move(*query_result);
        query_result.reset();
        if (results.size() > 5) results.resize(5);
        pending = std::move(results);
        validation_position = query_position;
        query_position = {0.0f, 0.0f, 0.0f, 0.0f};
        flags_12836 |= 0x80u;
        std::vector<std::optional<Hit>> hits;
        hits.reserve(pending.size() * 3);
        for (const auto& record : pending) {
            Vector end = closest_point(validation_position, record.endpoints());
            Vector start = validation_position;
            float height = start[1] - rs::from_bits(0x3ecccccdu);
            for (int k = 0; k < 3; ++k) {
                start[1] = height;
                end[1] = height;
                Line line{};
                line.start = start;
                line.end = end;
                line.radius = 0.2f;
                line.group = context.matching_id_2952;
                line.reject_flags = 0;
                line.source_pool_mask = 7;
                line.selection_flags = context.selection_flags_2948;
                S3G_TRY(auto hit, scene.line(line));
                hits.push_back(hit);
                height += rs::from_bits(0x3ecccccdu);
            }
        }
        //Even an empty real query creates a completed empty batch.
        validation = std::move(hits);
    }
    return ok();
}

std::optional<Record> Owner::best(Vector position) const { return selection::best(validated, position); }

void Owner::invalidate() {
    queries.clear();
    query_result.reset();
    validation.reset();
    data_ready = {false, false};
    flags_12836 &= 0x3fu;
}

void Owner::enter_reset() {
    invalidate();
    interactable_result.reset();
}

Publication Owner::publish() {
    Publication out{};
    for (std::size_t i = 0; i < 2; ++i) {
        std::optional<Record> record;
        if (data_ready[i]) {
            if (data[i] && data[i]->valid()) record = *data[i];
        }
        data_ready[i] = false;
        out.records[i] = record;
    }
    const auto object = interactable_result;
    interactable_result.reset();
    if (object) {
        interactable_latched = object->has_value();
    }
    out.object = object;
    return out;
}

///82D74740, raw747B8/747F4/7481C..74874. S2 has an extra outer hit-index
///increment at82DCC0FC; S3 does not. Do not group hits into chunks of three.
void validate(std::vector<Record>& records, const std::vector<std::optional<Hit>>& hits) {
    std::size_t record_index = 0;
    std::size_t hit_index = 0;
    while (record_index < records.size()) {
        const auto assembly = records[record_index].assembly();
        bool obstructed = false;
        for (int k = 0; k < 3; ++k) {
            S3_ASSERT(hit_index < hits.size());
            if (const auto& hit = hits[hit_index]) {
                obstructed = !hit->assembly.has_value() || (assembly.has_value() && hit->assembly != assembly);
            }
            hit_index += 1;
            if (obstructed) break;
        }
        if (obstructed) {
            // Vec::swap_remove
            if (record_index + 1 != records.size()) records[record_index] = std::move(records.back());
            records.pop_back();
        } else {
            record_index += 1;
        }
    }
}

}  // namespace s3::game::physics::biped_ground::grab_runtime
