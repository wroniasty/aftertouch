// Goal-frame post strips, dead-ball squad freeze, post-goal kickoff take.
#include <doctest/doctest.h>

#include "core/ball.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"
#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("clear byline exit outside post strip stops play") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.globals.team_playing_up = 1;
    s.clock.last_team_played = 1;
    s.clock.match_started = 1;
    // Past top byline, well left of posts (mouth 303–367).
    s.ball.pos.x = Fix::FromInt(250);
    s.ball.pos.y = Fix::FromInt(120);
    s.ball.pos.z = Fix{};
    s.ball.dest_x = 250;
    s.ball.dest_y = 100;
    s.ball.speed = 2000;
    s.ball.delta = {};

    WireOutOfPlay(s);
    CHECK(GetPl(s) == GameStatePl::Stopped);
    CHECK((IsCornerState(GetGameState(s)) || IsGoalKickState(GetGameState(s))));
    CHECK(GetGameState(s) != GameState::StartingGame);
}

TEST_CASE("post strip bounce keeps InProgress") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.match_started = 1;
    const Vec3 saved{Fix::FromInt(300), Fix::FromInt(130), Fix{}};
    s.ball.pos.x = Fix::FromInt(300); // within 8u of mouth min 303
    s.ball.pos.y = Fix::FromInt(120);
    s.ball.pos.z = Fix{};
    s.ball.dest_x = 300;
    s.ball.dest_y = 80;
    s.ball.speed = 2500;

    Vec3 saved_mut = saved;
    ApplyGoalFrame(s, saved_mut);
    CHECK(GetPl(s) == GameStatePl::InProgress);
    // Restored toward pre-integrate y.
    CHECK(s.ball.pos.y.Whole() == saved.y.Whole());
}

TEST_CASE("dead-ball freezes off-ball teammates") {
    MatchState s{};
    BeginRestart(s, GameState::ThrowInCentreLeft, 70, 400, 0x1F, 4, 1);
    s.sides[0].control.player_number = 1;
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(200 + i * 10);
        e.pos.y = Fix::FromInt(500);
        e.dest_x = 400;
        e.dest_y = 400;
    }
    PlaceThrowInTaker(s, 0);
    const int taker = s.sides[0].control.controlled_slot;
    const int other = (taker == 2) ? 3 : 2;
    const int16_t ox = s.players[static_cast<size_t>(other)].pos.x.Whole();
    const int16_t oy = s.players[static_cast<size_t>(other)].pos.y.Whole();

    MatchInput in{};
    s.globals.team_switch_counter = 1;
    ApplyTeamControls(s, in);

    CHECK(s.players[static_cast<size_t>(other)].dest_x == ox);
    CHECK(s.players[static_cast<size_t>(other)].dest_y == oy);
    CHECK(s.players[static_cast<size_t>(other)].speed == 0);
}

TEST_CASE("goal sets kickoff take for conceding side") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    s.clock.last_team_played = 2;
    s.clock.match_started = 1;
    for (int i = 0; i < kPitchPlayers; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = static_cast<int16_t>(i < 11 ? 1 : 2);
        e.player_ordinal = static_cast<int16_t>((i % 11) + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(100 + i);
        e.pos.y = Fix::FromInt(200 + i);
    }
    // Top goal. team_playing_up (1) *defends* the top line, so the scorer is 2
    // and team 1 concedes and restarts.
    s.globals.foul_x = 336;
    s.globals.foul_y = 120;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(120);
    s.ball.pos.z = Fix{};

    CompleteOopRestart(s, GameState::StartingGame, true);

    CHECK(GetPl(s) == GameStatePl::Stopped);
    CHECK(GetGameState(s) == GameState::StartingGame);
    CHECK(s.globals.last_team_played_before_break == 1);
    CHECK(s.ball.pos.x.Whole() == kCentreSpotX);
    CHECK(s.ball.pos.y.Whole() == kCentreSpotY);
    CHECK(s.clock.stoppage_event_timer == 0);
    // Players returned to kickoff shape (not still at 100+i, 200+i).
    CHECK(s.players[1].pos.x.Whole() != 101);

    s.sides[0].control.normal_fire = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    const int taker = s.sides[0].control.controlled_slot;
    REQUIRE(taker >= 0);
    s.players[static_cast<size_t>(taker)].pos.x = Fix::FromInt(kCentreSpotX);
    s.players[static_cast<size_t>(taker)].pos.y = Fix::FromInt(kCentreSpotY);
    REQUIRE(ApplyRestartTake(s, 0));
    CHECK(GetPl(s) == GameStatePl::InProgress);
}
