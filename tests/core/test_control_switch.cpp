// Auto-select when ball is loose (ball_out_of_play gate).
#include <doctest/doctest.h>

#include "core/match_engine.hpp"
#include "core/match_state.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"

using namespace at;

TEST_CASE("kickoff open play raises ball_out_of_play for both sides") {
    MatchEngine eng;
    eng.Reset(0xC0FFEE01u);
    eng.Step(MatchInput{}); // BeginMatchIfNeeded + PlacePlayersAtKickoff
    // Drain stoppage so InProgress + MarkBallLoose from UpdateTime.
    for (int i = 0; i < 4; ++i) eng.Step(MatchInput{});
    CHECK(GetPl(eng.State()) == GameStatePl::InProgress);
    CHECK(eng.State().sides[0].control.ball_out_of_play == 1);
    CHECK(eng.State().sides[1].control.ball_out_of_play == 1);
}

TEST_CASE("loose ball switches controlled to nearer outfielder") {
    MatchState s{};
    s.globals.team_playing_up = 1;
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    PlaceBallAtCentre(s);
    MarkBallLoose(s);

    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9; // far striker slot
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.pass_to_slot = -1;
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(100);
        e.pos.y = Fix::FromInt(100);
        e.ball_distance = 0x7fffffff;
    }
    // Only slot 3 is next to the ball.
    s.players[3].pos.x = Fix::FromInt(kCentreSpotX + 2);
    s.players[3].pos.y = Fix::FromInt(kCentreSpotY + 2);

    // Direct selection (ApplyTeamControls also assigns pass_to and can exclude).
    UpdateControlledPlayer(s, 0);
    CHECK(s.sides[0].control.controlled_slot == 3);
}

TEST_CASE("holding ball does not switch controlled player") {
    MatchState s{};
    s.globals.team_playing_up = 1;
    SetPl(s, GameStatePl::InProgress);
    PlaceBallAtCentre(s);

    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 5;
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.ball_out_of_play = 0;
    s.sides[0].control.ball_can_be_controlled = 1;
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(300);
        e.pos.y = Fix::FromInt(300);
    }
    s.players[5].pos.x = Fix::FromInt(kCentreSpotX);
    s.players[5].pos.y = Fix::FromInt(kCentreSpotY);
    // Nearer teammate at ball — must not steal control while 5 holds.
    s.players[3].pos.x = Fix::FromInt(kCentreSpotX + 1);
    s.players[3].pos.y = Fix::FromInt(kCentreSpotY + 1);

    UpdatePossessionForSide(s, 0);
    UpdateControlledPlayer(s, 0);
    CHECK(s.sides[0].control.controlled_slot == 5);
    CHECK(s.sides[0].control.ball_out_of_play == 0);
}
