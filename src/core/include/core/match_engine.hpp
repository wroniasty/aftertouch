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
    // Replace live state (e.g. after ApplyKickoff). Caller owns seeding policy.
    void LoadState(const MatchState& s) { state_ = s; }

    const MatchState& State() const { return state_; }

    // Presentation-only draw (camera kick-off end, C2). Comes from the stream
    // HashState excludes, so consuming it cannot desynchronise a replay — which is
    // exactly why the camera is allowed a coin flip while living outside the tick.
    uint16_t DrawPresentationRng() { return state_.presentation_rng.Draw(); }

private:
    MatchState state_{};
};

} // namespace at
