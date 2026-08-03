// A side reduced to its keeper — nine sendings off, or a C1b sandbox side —
// must still be able to take a restart, and its keeper must stay a keeper.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"
#include "core/set_pieces.hpp"

using namespace at;

namespace {

// Side 0 full, side 1 keeper only (slot 11).
void SeedKeeperOnlyAway(MatchState& s) {
    s.globals.team_playing_up = 1;
    s.clock.match_started = 1;
    for (int i = 0; i < kPitchPlayers; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = static_cast<int16_t>(i < 11 ? 1 : 2);
        e.player_ordinal = static_cast<int16_t>((i % 11) + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(static_cast<int16_t>(200 + (i % 11) * 20));
        e.pos.y = Fix::FromInt(static_cast<int16_t>(i < 11 ? 400 : 600));
        e.dest_x = e.pos.x.Whole();
        e.dest_y = e.pos.y.Whole();
    }
    for (int i = 12; i < kPitchPlayers; ++i) ParkOffPitch(s.players[static_cast<size_t>(i)]);
    s.sides[0].control.team_number = 1;
    s.sides[0].control.controlled_slot = 1;
    s.sides[1].control.team_number = 2;
    s.sides[1].control.controlled_slot = -1;
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);
}

} // namespace

TEST_CASE("keeper takes the throw-in when he is the last man") {
    MatchState s{};
    SeedKeeperOnlyAway(s);

    BeginRestart(s, GameState::ThrowInCentreRight, 600, 449, 0xF1, 4, 2);
    PickRestartTaker(s, 1);
    PlaceThrowInTaker(s, 1);

    const int taker = s.sides[1].control.controlled_slot;
    CHECK(taker == 11);
    CHECK_FALSE(IsOffPitch(s.players[static_cast<size_t>(taker)]));
}

TEST_CASE("keeper-only side resumes play from a free kick") {
    MatchEngine eng;
    eng.Reset(0xC1B00001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    SeedKeeperOnlyAway(s);
    s.sides[1].control.player_number = 2; // human, so the take is scripted
    BeginRestart(s, GameState::FreeKickCentre, 336, 600, kTurnFlagsAll, 0, 2);
    PickRestartTaker(s, 1);
    PlaceTakerNearSpot(s, 1);
    REQUIRE(s.sides[1].control.controlled_slot == 11);
    eng.LoadState(s);

    MatchInput in{};
    in.p2.dir = Dir::N;
    in.p2.fire = true;
    for (int i = 0; i < 16; ++i) eng.Step(in);

    CHECK(GetPl(eng.State()) == GameStatePl::InProgress);
}

TEST_CASE("open play releases the control slot of a keeper-only side") {
    MatchState s{};
    SeedKeeperOnlyAway(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);

    // A restart has just handed the keeper the slot.
    TeamControl& tc = s.sides[1].control;
    tc.controlled_slot = 11;
    tc.ball_out_of_play = 1;

    UpdateControlledPlayer(s, 1);

    CHECK(tc.controlled_slot == -1);
}

TEST_CASE("keeper-only side keeps its keeper in goal") {
    MatchState s{};
    SeedKeeperOnlyAway(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.sides[1].control.player_number = 0; // CPU
    // Ball parked in the far corner of the other half: an outfield brain would
    // chase it; the keeper AI stays home.
    s.ball.pos.x = Fix::FromInt(120);
    s.ball.pos.y = Fix::FromInt(200);

    MatchInput in{};
    for (int t = 0; t < 400; ++t) {
        s.globals.team_switch_counter = 0; // side 1's turn every iteration
        ApplyTeamControls(s, in);
        MovePlayers(s);
    }

    // team_playing_up == 1 defends the top, so side 1 defends the bottom line.
    CHECK(s.players[11].pos.y.Whole() > kCentreSpotY);
    CHECK(s.sides[1].control.controlled_slot == -1);
}
