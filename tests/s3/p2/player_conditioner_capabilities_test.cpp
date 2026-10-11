// Port of skate-core/src/player/conditioner_capabilities/tests.rs.
#include "harness.hpp"
#include "s3/core/player/conditioner_capabilities.hpp"

namespace {
using s3::player::conditioner_capabilities::ConditionerCapabilityContext;

S3_TEST(complete_source_capability_branch_table) {
    // Literal results from 8275F4D8..8275F558, indexed as FE/HoM/query/config.
    // In particular, active challenge configuration overrides the HoM setting;
    // an inactive query does not consult that configuration. FE wins over both.
    const std::uint32_t source_words[16] = {
        0x000001c0, 0x000001c0, 0x000001c0, 0xfffffff7, 0xfffff7f7, 0xfffff7f7, 0x000001c0, 0xfffffff7,
        0,          0,          0,          0,          0,          0,          0,          0,
    };
    for (unsigned combination = 0; combination < 16; ++combination) {
        const ConditionerCapabilityContext context{
            (combination & 8) != 0,
            (combination & 4) != 0,
            (combination & 2) != 0,
            (combination & 1) != 0,
        };
        CHECK_MSG(context.capabilities() == source_words[combination], "combination " << combination);
    }
}
}  // namespace
