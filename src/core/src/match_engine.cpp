#include "core/match_engine.hpp"

namespace at {

void MatchEngine::Reset(uint32_t seed) {
    state_ = MatchState{};
    rng_   = seed ? seed : 1;
}

void MatchEngine::Step(const MatchInput& in) {
    (void)in;
    ++state_.tick;
    // Everything else comes later. The tick counter alone is enough to prove
    // the fixed-step loop in the shell is wired correctly.
}

} // namespace at
