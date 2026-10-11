// Port of skate-core/src/physics/constraint_batch.rs.
// Joint/drive batch eligibility: TU3 82AE39D0 and 82AE1778.
//
// Native clears the output counts, traverses the registered list in order,
// and compiles a row only when at least one endpoint has state bit 4. This
// does not register, wake, freeze or reorder bodies.
#pragma once
#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace s3::physics::constraint_batch {

template <class Range, class States, class Compile>
auto compile_active(const Range& registered, States&& endpoint_states, Compile&& compile) {
    using T = std::decay_t<decltype(*std::begin(registered))>;
    using R = std::decay_t<decltype(compile(std::declval<const T&>()))>;
    std::vector<R> out;
    for (const auto& constraint : registered) {
        const std::array<std::uint32_t, 2> s = endpoint_states(constraint);
        if (((s[0] | s[1]) & 4u) != 0) {
            out.push_back(compile(constraint));
        }
    }
    return out;
}

}  // namespace s3::physics::constraint_batch
