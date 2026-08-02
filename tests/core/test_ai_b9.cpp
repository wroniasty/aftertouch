// B9: selection, CPU brain, GK, restart taker, CPU-vs-CPU acceptance.
#include <doctest/doctest.h>

#include "core/ai.hpp"
#include "core/goalkeeper.hpp"
#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/set_pieces.hpp"

using namespace at;

namespace {

void FillTactics(MatchState& s) {
    for (int side = 0; side < 2; ++side) {
        for (int r = 0; r < kMatchTacticRoles; ++r)
            for (int q = 0; q < kMatchBallQuadrants; ++q)
                s.sides[static_cast<size_t>(side)]
                    .tactics.cells[static_cast<size_t>(r)][static_cast<size_t>(q)] =
                    static_cast<uint8_t>(((r % 15) << 4) | (q % 16));
    }
}

MatchState BootCpuState(uint32_t seed) {
    MatchEngine eng;
    eng.Reset(seed);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    s.sides[0].control.player_number = 0;
    s.sides[1].control.player_number = 0;
    s.globals.team_playing_up = 1;
    FillTactics(s);
    PlacePlayersAtKickoff(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.phase = MatchPhase::InPlay;
    s.clock.stoppage_event_timer = 0;
    for (int i = 0; i < 2; ++i)
        s.sides[static_cast<size_t>(i)].control.ball_in_play = 1;
    return s;
}

uint64_t RunCpuScriptHash() {
    MatchEngine eng;
    eng.Reset(0xB9000001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 0;
    s.sides[1].control.player_number = 0;
    s.sides[0].control.controlled_slot = 9;
    s.globals.team_playing_up = 1;
    FillTactics(s);
    PlacePlayersAtKickoff(s);

    // Ball near home striker; facing goal; capture then shoot path.
    Entity& p = s.players[9];
    p.pos.x = Fix::FromInt(336);
    p.pos.y = Fix::FromInt(700);
    p.direction = static_cast<int16_t>(Dir::S);
    p.team_number = 1;
    p.player_ordinal = 10;
    GiveBallForTest(s, 0, 9);
    s.sides[0].control.ball_in_play = 1;
    s.sides[0].control.ball_out_of_play = 1; // allow selection refresh

    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.phase = MatchPhase::InPlay;
    s.clock.stoppage_event_timer = 0;
    eng.LoadState(s);

    for (int i = 0; i < 80; ++i) eng.Step(MatchInput{});
    return HashState(eng.State());
}

} // namespace

TEST_CASE("pass target excludes controlled and kicker") {
    MatchState s = BootCpuState(0xB90000AAu);
    s.sides[0].control.controlled_slot = 5;
    s.sides[0].control.passing_kicking_slot = 6;
    s.sides[0].control.player_switch_timer = 0;
    s.sides[0].control.ball_in_play = 1;

    // Put slot 7 closest to ball among eligible.
    s.ball.pos.x = Fix::FromInt(400);
    s.ball.pos.y = Fix::FromInt(400);
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.pos.x = Fix::FromInt(100);
        e.pos.y = Fix::FromInt(100);
        e.cards = 0;
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.team_number = 1;
    }
    s.players[7].pos.x = Fix::FromInt(401);
    s.players[7].pos.y = Fix::FromInt(401);
    s.players[5].pos.x = Fix::FromInt(400);
    s.players[5].pos.y = Fix::FromInt(400);
    s.players[6].pos.x = Fix::FromInt(402);
    s.players[6].pos.y = Fix::FromInt(400);

    UpdatePlayerBeingPassedTo(s, 0);
    CHECK(s.sides[0].control.pass_to_slot == 7);
}

TEST_CASE("cpu shoot sets fire when near goal with ball") {
    MatchState s = BootCpuState(0xB90000BBu);
    // Team 1 playing up → attacks bottom (high y); face south near that goal.
    s.sides[0].control.controlled_slot = 9;
    Entity& p = s.players[9];
    p.pos.x = Fix::FromInt(336);
    p.pos.y = Fix::FromInt(700);
    p.direction = static_cast<int16_t>(Dir::S);
    p.player_ordinal = 10;
    p.team_number = 1;
    GiveBallForTest(s, 0, 9);
    UpdateProximityBands(s, 0);

    AI_SetControlsDirection(s, 0);
    CHECK(s.sides[0].control.normal_fire == 1);
    CHECK(s.sides[0].control.current_allowed_direction >= 0);
}

TEST_CASE("cpu chase aims at ball") {
    MatchState s = BootCpuState(0xB90000CCu);
    s.sides[0].control.controlled_slot = 8;
    s.sides[0].control.player_has_ball = 0;
    Entity& p = s.players[8];
    p.pos.x = Fix::FromInt(200);
    p.pos.y = Fix::FromInt(400);
    p.direction = static_cast<int16_t>(Dir::S);
    p.player_ordinal = 9;
    p.team_number = 1;
    p.ball_distance = 10000;
    s.ball.pos.x = Fix::FromInt(300);
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.pos.z = Fix{};

    AI_SetControlsDirection(s, 0);
    CHECK(s.sides[0].control.current_allowed_direction ==
          static_cast<int16_t>(Dir::E));
}

TEST_CASE("goalkeeper claims landing in own box") {
    MatchState s = BootCpuState(0xB90000DDu);
    s.sides[0].squad[0].goalie_skill = 7;
    Entity& gk = s.players[0];
    gk.pos.x = Fix::FromInt(336);
    gk.pos.y = Fix::FromInt(150);
    gk.player_ordinal = 1;
    gk.team_number = 1;
    gk.cards = 0;
    gk.player_state = static_cast<uint8_t>(PlayerState::Normal);
    gk.ball_distance = 100;

    // Top goal — team 1 playing up means they attack bottom, defend top.
    s.globals.team_playing_up = 1;
    s.globals.ball_next_x = 336;
    s.globals.ball_next_y_ground_y = 180;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(300);

    ApplyGoalkeeperAI(s, 0);
    CHECK(gk.dest_x == 336);
    CHECK(gk.dest_y == 180);
}

TEST_CASE("cpu restart taker fires toward goal") {
    MatchEngine eng;
    eng.Reset(0xB90000EEu);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    s.sides[0].control.player_number = 0;
    s.sides[0].control.controlled_slot = 4;
    s.globals.team_playing_up = 1;
    FillTactics(s);
    PlacePlayersAtKickoff(s);
    BeginRestart(s, GameState::FreeKickCentre, 336, 250, kTurnFlagsAll, 4, 1);
    PlaceTakerNearSpot(s, 0);
    eng.LoadState(s);

    bool fired = false;
    for (int i = 0; i < 20; ++i) {
        eng.Step(MatchInput{});
        if (GetPl(eng.State()) == GameStatePl::InProgress) {
            fired = true;
            break;
        }
    }
    CHECK(fired);
}

TEST_CASE("cpu vs cpu short match produces activity") {
    MatchEngine eng;
    eng.Reset(0xB90000FFu);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    s.sides[0].control.player_number = 0;
    s.sides[1].control.player_number = 0;
    s.globals.team_playing_up = 1;
    FillTactics(s);
    PlacePlayersAtKickoff(s);
    // Kick off into open play with ball free near centre.
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.phase = MatchPhase::InPlay;
    s.clock.stoppage_event_timer = 0;
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);
    s.ball.speed = 0;
    for (int i = 0; i < 2; ++i) {
        s.sides[static_cast<size_t>(i)].control.ball_in_play = 1;
        s.sides[static_cast<size_t>(i)].control.ball_out_of_play = 1;
        s.sides[static_cast<size_t>(i)].control.ball_can_be_controlled = 1;
    }
    eng.LoadState(s);

    int16_t min_x = s.ball.pos.x.Whole();
    int16_t max_x = min_x;
    int16_t min_y = s.ball.pos.y.Whole();
    int16_t max_y = min_y;
    for (int i = 0; i < 400; ++i) {
        eng.Step(MatchInput{});
        const auto& b = eng.State().ball;
        const int16_t x = b.pos.x.Whole();
        const int16_t y = b.pos.y.Whole();
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    const int span = (max_x - min_x) + (max_y - min_y);
    CAPTURE(span);
    CAPTURE(eng.State().score[0]);
    CAPTURE(eng.State().score[1]);
    // Ball should move or a restart should fire — not a frozen pitch.
    CHECK((span > 20 || GetPl(eng.State()) != GameStatePl::InProgress ||
           eng.State().score[0] + eng.State().score[1] > 0));
}

TEST_CASE("scripted cpu chase/shoot hash is stable") {
    const uint64_t a = RunCpuScriptHash();
    CHECK(a == RunCpuScriptHash());
    constexpr uint64_t kExpected = 0xee75dd97a9402537ull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
