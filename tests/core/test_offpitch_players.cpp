// Off-pitch slots — sent off (B8) or never spawned (C1b sandbox mode).
// A marked slot must stay off the pitch: no kickoff placement, no tactics
// recall, and no selection as controlled player or pass target.
#include <doctest/doctest.h>

#include "core/ai.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"
#include "core/set_pieces.hpp"

using namespace at;

namespace {

// Two full sides, everyone Normal, sensible spread around the halfway line.
void SeedSides(MatchState& s) {
    s.globals.team_playing_up = 1;
    s.clock.match_started = 1;
    for (int i = 0; i < kPitchPlayers; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = static_cast<int16_t>(i < 11 ? 1 : 2);
        e.player_ordinal = static_cast<int16_t>((i % 11) + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(static_cast<int16_t>(200 + (i % 11) * 20));
        e.pos.y = Fix::FromInt(static_cast<int16_t>(i < 11 ? 400 : 500));
        e.dest_x = e.pos.x.Whole();
        e.dest_y = e.pos.y.Whole();
    }
    for (int side = 0; side < 2; ++side) {
        TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
        tc.team_number = static_cast<uint8_t>(side + 1);
        tc.controlled_slot = static_cast<int8_t>(side * 11 + 1);
    }
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);
}

} // namespace

TEST_CASE("kickoff placement leaves off-pitch players off") {
    MatchState s{};
    SeedSides(s);
    ParkOffPitch(s.players[5]); // slot 5 → squad index 5 (ordinal 6)

    PlacePlayersAtKickoff(s);

    CHECK(s.players[5].pos.x.Whole() == kOffPitchParkX);
    CHECK(s.players[5].speed == 0);
    // Never took the field, so no appearance is recorded for him.
    CHECK(s.sides[0].squad[5].half_played == 0);
    // A teammate on the pitch is placed normally.
    CHECK(s.players[6].pos.x.Whole() >= kPlayableMinX);
    CHECK(s.players[6].pos.x.Whole() <= kPlayableMaxX);
    CHECK(s.sides[0].squad[6].half_played == 1);
}

TEST_CASE("off-pitch player is not recalled by tactics") {
    MatchState s{};
    SeedSides(s);
    ParkOffPitch(s.players[5]);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);

    const int16_t x = s.players[5].pos.x.Whole();
    const int16_t y = s.players[5].pos.y.Whole();

    MatchInput in{};
    for (int t = 0; t < 200; ++t) {
        s.globals.team_switch_counter = 1; // side 0's turn every iteration
        ApplyTeamControls(s, in);
        MovePlayers(s);
    }

    CHECK(s.players[5].pos.x.Whole() == x);
    CHECK(s.players[5].pos.y.Whole() == y);
    CHECK(s.players[5].speed == 0);
}

TEST_CASE("off-pitch player is never selected or passed to") {
    MatchState s{};
    SeedSides(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    TeamControl& tc = s.sides[0].control;

    // Park the nearest man to the ball off the pitch; he would otherwise win
    // both the control scan and the pass scan.
    s.players[3].pos.x = Fix::FromInt(kCentreSpotX);
    s.players[3].pos.y = Fix::FromInt(kCentreSpotY);
    ParkOffPitch(s.players[3]);

    tc.ball_out_of_play = 1;
    tc.pass_to_slot = -1;
    tc.passing_kicking_slot = -1;
    UpdateControlledPlayer(s, 0);
    CHECK(tc.controlled_slot != 3);

    UpdatePlayerBeingPassedTo(s, 0);
    CHECK(tc.pass_to_slot != 3);

    CHECK(FindPassConeTeammate(s, 0, tc.controlled_slot, 4) != 3);
}

TEST_CASE("a walk-off in progress still completes") {
    MatchState s{};
    SeedSides(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);

    // Sending-off ceremony state: marked, but still out on the grass walking.
    Entity& off = s.players[5];
    off.cards = -1;
    off.dest_x = -20;
    off.dest_y = kCentreSpotY;
    const int16_t start_x = off.pos.x.Whole();

    MatchInput in{};
    for (int t = 0; t < 300; ++t) {
        s.globals.team_switch_counter = 1;
        ApplyTeamControls(s, in);
        MovePlayers(s);
    }

    CHECK(off.pos.x.Whole() < start_x);
    CHECK(off.pos.x.Whole() <= kPlayableMinX);
}
