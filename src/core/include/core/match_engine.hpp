#pragma once
#include "core/match_input.hpp"
#include "core/match_state.hpp"

namespace at {

// Deterministic, headless, fixed-step. Given the same seed and the same input
// sequence, produces the same state sequence on every platform and every run.
//
// This class is the reason the whole project is laid out the way it is. It has
// no dependency on SDL, ImGui, the filesystem, the clock, or the platform.
class MatchEngine {
public:
    static constexpr int kTickHz = 50;

    void Reset(uint32_t seed);
    void Step(const MatchInput& in);

    const MatchState& State() const { return state_; }

private:
    MatchState state_{};
    uint32_t   rng_ = 1;
};

} // namespace at
