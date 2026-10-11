// Port of skate-core/src/physics/contact_feedback/reports.rs.
#include "s3/core/physics/contact_feedback/reports.hpp"

#include "s3/core/rs.hpp"

namespace s3::physics::contact_feedback::reports {

namespace {
void put(std::array<std::uint8_t, 96>& record, std::size_t offset, std::uint32_t value) {
    const auto b = rs::to_be_bytes_u32(value);
    for (std::size_t i = 0; i < 4; ++i) record[offset + i] = b[i];
}
}  // namespace

std::size_t get_contact_reports(std::uint32_t body, ContactReportBackend& backend) {
    backend.lock();
    const std::size_t handle = backend.get_or_insert_reports(body);
    backend.unlock();
    return handle;
}

void resolve_contact_reports(ContactReportDispatchBackend& backend) {
    std::optional<std::size_t> entry = backend.first_report_entry();
    while (entry) {
        const std::size_t handle = *entry;
        if (backend.report_count(handle) != 0) {
            backend.dispatch_reports(handle);
        }
        entry = backend.next_report_entry(handle);
    }
}

void create_contact_reports(const std::vector<std::array<std::uint32_t, 28>>& spies, std::uint32_t spy_base,
                            ContactReportBackend& backend) {
    backend.lock();
    backend.clear_report_counts();
    for (std::size_t index = 0; index < spies.size(); ++index) {
        const auto& spy = spies[index];
        const std::optional<ContactTarget> a = backend.lookup_target(spy[24]);
        const std::optional<ContactTarget> b = backend.lookup_target(spy[25]);
        const std::uint32_t a_body = a ? a->body : 0;
        const std::uint32_t b_body = b ? b->body : 0;
        const std::uint32_t a_owner = a ? a->owner : 0;
        const std::uint32_t b_owner = b ? b->owner : 0;
        const bool same_body = a.has_value() && b.has_value() && a_body == b_body;
        const bool excluded = same_body || (a_owner != 0 && a_owner == b_owner);
        const std::pair<std::optional<ContactTarget>, bool> sides[2] = {{a, true}, {b, false}};
        for (const auto& [maybe_target, is_a] : sides) {
            if (!maybe_target) {
                continue;
            }
            const ContactTarget target = *maybe_target;
            // Native operator[] runs even for a subsequently excluded pair.
            const std::size_t handle = backend.get_or_insert_reports(target.body);
            const std::uint32_t count = backend.reports(handle).count;
            if (count >= 16 || excluded) {
                continue;
            }
            const std::uint32_t other = is_a ? spy[25] : spy[24];
            const std::uint32_t self = is_a ? spy[24] : spy[25];
            const auto other_velocity = rs::from_bits(backend.linear_velocity(other));
            const auto this_velocity = rs::from_bits(backend.linear_velocity(self));
            std::array<std::uint32_t, 4> normal{};
            for (std::size_t i = 0; i < 4; ++i) {
                normal[i] = rs::to_bits(rs::from_bits(spy[i]) * (is_a ? 1.0f : -1.0f));
            }
            std::array<std::uint32_t, 4> relative{};
            for (std::size_t i = 0; i < 4; ++i) relative[i] = rs::to_bits(this_velocity[i] - other_velocity[i]);
            ContactReportBuffer& buffer = backend.reports(handle);
            auto& record = buffer.records.at(count);
            put(record, 0, spy_base + static_cast<std::uint32_t>(index) * 112u);
            for (std::size_t i = 0; i < 4; ++i) {
                put(record, 16 + i * 4, normal[i]);
                put(record, 32 + i * 4, spy[12 + i]);
                put(record, 48 + i * 4, relative[i]);
            }
            put(record, 64, target.tag);
            put(record, 68, is_a ? (spy[26] & 0xffffu) : (spy[26] >> 16));
            put(record, 72, target.body);
            put(record, 76, is_a ? b_body : a_body);
            record[80] = 0;
            record[81] = static_cast<std::uint8_t>(is_a);
            buffer.count += 1;
        }
    }
    backend.unlock();
}

}  // namespace s3::physics::contact_feedback::reports
