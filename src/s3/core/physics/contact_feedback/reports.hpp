// Port of skate-core/src/physics/contact_feedback/reports.rs.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace s3::physics::contact_feedback::reports {

/// Registered target for a rigid-body pointer (Island+80 lookup).
struct ContactTarget {
    std::uint32_t body = 0;
    std::uint32_t owner = 0;
    std::uint32_t tag = 0;
};

/// Supplied storage preserves every unwritten byte when records are reused.
struct ContactReportBuffer {
    std::uint32_t count = 0;
    std::array<std::array<std::uint8_t, 96>, 16> records{};
};

/// Container/locking boundary of 827682B0. The map operation inserts an absent
/// Body key; clearing resets existing vector ends without erasing keys/storage.
/// Allocated report slots and their padding are supplied by the backend.
class ContactReportBackend {
public:
    virtual ~ContactReportBackend() = default;
    virtual void lock() = 0;
    virtual void clear_report_counts() = 0;
    virtual std::optional<ContactTarget> lookup_target(std::uint32_t rigid_body) = 0;
    virtual std::size_t get_or_insert_reports(std::uint32_t body) = 0;
    virtual ContactReportBuffer& reports(std::size_t handle) = 0;
    virtual std::array<std::uint32_t, 4> linear_velocity(std::uint32_t rigid_body) = 0;
    virtual void unlock() = 0;
};

/// Complete 82768250 lookup lifetime: lock, operator[] (possibly inserts),
/// unlock, return the live vector handle. No detached report copy is produced.
std::size_t get_contact_reports(std::uint32_t body, ContactReportBackend& backend);

/// Live tree traversal and Body virtual+8 dispatch boundary of 827685C8.
/// The backend retains native map order and resolves next after each callback.
class ContactReportDispatchBackend {
public:
    virtual ~ContactReportDispatchBackend() = default;
    virtual std::optional<std::size_t> first_report_entry() = 0;
    virtual std::optional<std::size_t> next_report_entry(std::size_t handle) = 0;
    virtual std::uint32_t report_count(std::size_t handle) = 0;
    virtual void dispatch_reports(std::size_t handle) = 0;
};

/// Complete 827685C8. Visits every map entry, invokes only nonempty live vectors,
/// and performs no list clearing or local locking around callbacks.
void resolve_contact_reports(ContactReportDispatchBackend& backend);

/// Complete 827682B0 report construction/filter/order, with its native map and
/// lock operations supplied by the backend. `spy_base` retains native pointer
/// identity; reports borrow that simulation storage through the consuming phase.
void create_contact_reports(const std::vector<std::array<std::uint32_t, 28>>& spies, std::uint32_t spy_base,
                            ContactReportBackend& backend);

}  // namespace s3::physics::contact_feedback::reports

namespace s3::physics::contact_feedback {
using reports::ContactReportBackend;
using reports::ContactReportBuffer;
using reports::ContactReportDispatchBackend;
using reports::ContactTarget;
using reports::create_contact_reports;
using reports::get_contact_reports;
using reports::resolve_contact_reports;
}  // namespace s3::physics::contact_feedback
