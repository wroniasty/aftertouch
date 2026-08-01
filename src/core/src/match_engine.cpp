#include "core/match_engine.hpp"

#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/trig.hpp"

namespace at {

void MatchEngine::Reset(uint32_t seed) {
    state_ = MatchState{};
    state_.gameplay_rng.Seed(seed);
    state_.presentation_rng.Seed(seed ^ 0x00A20000u);
    state_.resolve_rng.Seed(seed ^ 0x00B10000u);
    // Match bootstrap runs on first Step (BeginMatchIfNeeded).
}

void MatchEngine::Step(const MatchInput& in) {
    BeginMatchIfNeeded(state_);

    // Gameplay RNG draw every tick (A2 determinism gate + dice source).
    state_.last_roll = state_.gameplay_rng.Draw();

    // 1. Clock + period ends
    UpdateTime(state_);

    // 2. Controls stub — alternation phase; A2 ball walk only while live
    ++state_.globals.team_switch_counter;
    if (GetPl(state_) == GameStatePl::InProgress && in.p1.dir != Dir::None) {
        const Dest from{state_.ball.pos.x.Whole(), state_.ball.pos.y.Whole()};
        const Dest offset = DestinationFor(in.p1.dir);
        const Dest dest{static_cast<int16_t>(from.x + offset.x),
                        static_cast<int16_t>(from.y + offset.y)};
        const DeltaResult d = CalculateDeltaXAndY(1024, from, dest);
        state_.ball.delta.x = d.dx;
        state_.ball.delta.y = d.dy;
        state_.ball.pos.x += d.dx;
        state_.ball.pos.y += d.dy;
        if (d.heading >= 0)
            state_.ball.full_direction = static_cast<int16_t>(d.heading);
    }

    // 3. UpdateBallStub — B3
    // 4. MovePlayersStub — B4
    // 5. UpdateRefereeStub — B8

    // 6. Stats
    UpdateStats(state_);

    ++state_.tick;
}

} // namespace at
