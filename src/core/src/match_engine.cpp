#include "core/match_engine.hpp"



#include "core/ball.hpp"

#include "core/match_clock.hpp"



namespace at {



void MatchEngine::Reset(uint32_t seed) {

    state_ = MatchState{};

    state_.gameplay_rng.Seed(seed);

    state_.presentation_rng.Seed(seed ^ 0x00A20000u);

    state_.resolve_rng.Seed(seed ^ 0x00B10000u);

    // Match bootstrap runs on first Step (BeginMatchIfNeeded).

}



void MatchEngine::Step(const MatchInput& in) {

    (void)in;

    BeginMatchIfNeeded(state_);



    // Gameplay RNG draw every tick (A2 determinism gate + dice source).

    state_.last_roll = state_.gameplay_rng.Draw();



    // 1. Clock + period ends

    UpdateTime(state_);



    // 2. Controls stub — alternation phase only (ball walk removed in B3)

    ++state_.globals.team_switch_counter;



    // 3. UpdateBall

    UpdateBall(state_);



    // 4. MovePlayersStub — B4

    // 5. UpdateRefereeStub — B8



    // 6. Stats

    UpdateStats(state_);



    ++state_.tick;

}



} // namespace at

