#include "core/match_engine.hpp"

#include "core/aftertouch.hpp"
#include "core/animation.hpp"
#include "core/ball.hpp"
#include "core/match_clock.hpp"
#include "core/movement.hpp"
#include "core/referee.hpp"
#include "core/shooting.hpp"
#include "core/tackling.hpp"

namespace at {

void PlacePlayersAtKickoff(MatchState& s) {
    for (int side = 0; side < 2; ++side) {
        if (s.sides[static_cast<size_t>(side)].control.team_number == 0)
            s.sides[static_cast<size_t>(side)].control.team_number =
                static_cast<uint8_t>(side + 1);
        if (s.sides[static_cast<size_t>(side)].control.controlled_slot < 0)
            s.sides[static_cast<size_t>(side)].control.controlled_slot =
                static_cast<int8_t>(side * 11);
        s.sides[static_cast<size_t>(side)].control.ball_can_be_controlled = 1;

        const bool defend_top = SideDefendsTop(s, side);
        for (int i = 0; i < 11; ++i) {
            const int slot = side * 11 + i;
            Entity& e = s.players[static_cast<size_t>(slot)];
            // Sent off, or never spawned (C1b): a kickoff does not put a player
            // back on the pitch. Park him clear and leave half_played alone.
            if (IsOffPitch(e)) {
                ParkOffPitch(e);
                continue;
            }
            e.team_number = static_cast<int16_t>(side + 1);
            e.player_ordinal = static_cast<int16_t>(i + 1);
            e.player_state = static_cast<uint8_t>(PlayerState::Normal);
            const Dest d = OffBallDestination(s, side, i + 1);
            e.pos.x = Fix::FromInt(d.x);
            e.pos.y = Fix::FromInt(d.y);
            e.pos.z = Fix{};
            e.dest_x = d.x;
            e.dest_y = d.y;
            e.delta = {};
            e.speed = 0;
            e.direction = defend_top ? 4 : 0;
            e.player_direction = e.direction;
            s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)]
                .half_played = 1;
        }
    }
    MarkBallLoose(s);
}

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

    // 4c. C3: step every sprite's animation. After the contest pass, so a tackle that
    // begins this tick shows its slide on this tick rather than the next.
    StepAnimations(state_);

    // 5. Referee card ceremony (B8)
    UpdateReferee(state_);

    // 6. Stats
    UpdateStats(state_);

    ++state_.tick;
}

} // namespace at
