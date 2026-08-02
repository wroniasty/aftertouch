#include "core/match_engine.hpp"

#include "core/aftertouch.hpp"
#include "core/ball.hpp"
#include "core/match_clock.hpp"
#include "core/movement.hpp"
#include "core/referee.hpp"
#include "core/shooting.hpp"
#include "core/tackling.hpp"

namespace at {

void MatchEngine::Reset(uint32_t seed) {
    state_ = MatchState{};
    state_.gameplay_rng.Seed(seed);
    state_.presentation_rng.Seed(seed ^ 0x00A20000u);
    state_.resolve_rng.Seed(seed ^ 0x00B10000u);
    // Match bootstrap runs on first Step (BeginMatchIfNeeded).
}

void MatchEngine::Step(const MatchInput& in) {
    const bool boot = !state_.clock.match_started;
    BeginMatchIfNeeded(state_);
    if (boot) PlacePlayersAtKickoff(state_);

    // Gameplay RNG draw every tick (A2 determinism gate + dice source).
    state_.last_roll = state_.gameplay_rng.Draw();

    // 1. Clock + period ends
    UpdateTime(state_);

    // 1b. Human fire every tick (tap/hold threshold is wall-clock frames).
    RefreshHumanFire(state_, in);

    // 2. Team controls (one side / tick) — writes dest/speed/delta
    ApplyTeamControls(state_, in);

    // 2b. Aftertouch stick every tick (window is only ~10 frames).
    RefreshAftertouchInput(state_, in);

    // 3. UpdateBall (aftertouch apply + physics)
    UpdateBall(state_);

    // 4. MovePlayers — integrate all 22
    MovePlayers(state_);

    // 4b. B7: slide/header contact, foul test, recovery timers
    ProcessContestContacts(state_);

    // 5. Referee card ceremony (B8)
    UpdateReferee(state_);

    // 6. Stats
    UpdateStats(state_);

    ++state_.tick;
}

} // namespace at
