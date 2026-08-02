// B7 acceptance: scripted slide approach → HashState pin.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"

using namespace at;

namespace {

uint64_t RunContestSequence() {
    MatchEngine eng;
    eng.Reset(0xB7000001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.player_has_ball = 0;
    s.globals.team_playing_up = 1;
    for (int r = 0; r < kMatchTacticRoles; ++r)
        for (int q = 0; q < kMatchBallQuadrants; ++q)
            s.sides[0].tactics.cells[static_cast<size_t>(r)][static_cast<size_t>(q)] =
                static_cast<uint8_t>(((r % 15) << 4) | (q % 16));
    s.sides[0].squad[9].attrs.speed = 5;
    s.sides[0].squad[9].attrs.tackling = 5;
    s.sides[0].squad[9].attrs.ball_control = 4;
    s.sides[0].squad[9].attrs.heading = 8;

    PlacePlayersAtKickoff(s);
    // Place ball ahead of player 9, out of very-close/not-far so first fire slides.
    Entity& p = s.players[9];
    p.pos.x = Fix::FromInt(336);
    p.pos.y = Fix::FromInt(500);
    p.direction = static_cast<int16_t>(Dir::N);
    p.team_number = 1;
    p.player_ordinal = 10;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(420);
    s.ball.pos.z = Fix{};
    s.ball.speed = 0;
    s.ball.dest_x = 336;
    s.ball.dest_y = 420;

    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    // Hold fire north through the slide.
    for (int i = 0; i < 60; ++i) {
        in.p1.dir = Dir::N;
        in.p1.fire = true;
        eng.Step(in);
    }
    // Release and let recovery settle.
    in.p1.fire = false;
    for (int i = 0; i < 40; ++i) eng.Step(in);

    return HashState(eng.State());
}

} // namespace

TEST_CASE("scripted contest sequence hash is stable") {
    const uint64_t a = RunContestSequence();
    CHECK(a == RunContestSequence());
    constexpr uint64_t kExpected = 0xceabcbb0b0f0fb43ull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
