// Declarations of the private stage functions in skate-core/src/player/offboard/contact_toolkit/
// {collection,generation,infill,publication}.rs (each a single pub(super) fn set).
#pragma once
#include <vector>

#include "s3/core/player/offboard/contact_toolkit/candidate.hpp"
#include "s3/core/player/offboard/contact_toolkit/classification.hpp"

namespace s3::player::offboard::contact_toolkit {
struct QueryResults;
}

namespace s3::player::offboard::contact_toolkit::collection {
/// TU3 82D81610: authored-edge/secondary-line merge, then primary lines.
Samples collect(const Batch& batch, const ProbeLayout& layout, const QueryResults& results);
}
namespace s3::player::offboard::contact_toolkit::generation {
/// TU3 82D837C8: ordered rising/falling profile candidate production.
std::vector<candidate::Candidate> generate(const Input& input, const profile::Profile& profile,
                                           const ContactPrefix& prefix, float obstruction,
                                           std::uint32_t retained_flags);
}
namespace s3::player::offboard::contact_toolkit::infill {
/// Contact-list infill82D82498, normal correction82D82940/82D82AF0.
void insert_obstacles(const Input& input, Samples& s);
void correct_normals(const Input& input, Samples& s);
}
namespace s3::player::offboard::contact_toolkit::publication {
/// TU3 82D84060: profile lookahead, candidate selection and prefix publication.
void publish(const Input& input, const profile::Profile& profile, const Samples& samples,
             std::vector<candidate::Candidate>& candidates, float obstruction, candidate::Candidate& retained,
             classification::History& history, ContactPrefix& prefix);
}
