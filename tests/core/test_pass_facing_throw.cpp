// Facing-aware pass candidate + throw-in/FK tap-pass / shortfall assist.
#include <doctest/doctest.h>

#include "core/ai.hpp"
#include "core/movement.hpp"
#include "core/set_pieces.hpp"
#include "core/shooting.hpp"

using namespace at;

TEST_CASE("pass candidate prefers teammate in facing cone") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.ball_in_play = 1;
    s.sides[0].control.controlled_slot = 5;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E);
    s.sides[0].control.player_switch_timer = 0;

    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.cards = 0;
        e.pos.x = Fix::FromInt(200);
        e.pos.y = Fix::FromInt(400);
        e.direction = static_cast<int16_t>(Dir::E);
    }
    // Owner at origin of cone.
    s.players[5].pos.x = Fix::FromInt(300);
    s.players[5].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(300);
    s.ball.pos.y = Fix::FromInt(400);

    // Nearer teammate behind (west) — must not win.
    s.players[4].pos.x = Fix::FromInt(280);
    s.players[4].pos.y = Fix::FromInt(400);
    // Farther teammate ahead (east) — facing cone winner.
    s.players[7].pos.x = Fix::FromInt(380);
    s.players[7].pos.y = Fix::FromInt(400);

    UpdatePlayerBeingPassedTo(s, 0);
    CHECK(s.sides[0].control.pass_to_slot == 7);
}

TEST_CASE("throw-in tap targets pass_to with lower loft than hold") {
    MatchState s{};
    BeginRestart(s, GameState::ThrowInCentreLeft, 70, 400, 0x1F, 4, 1);
    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.pass_to_slot = 3;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E);
    s.players[1].team_number = 1;
    s.players[1].pos.x = Fix::FromInt(67);
    s.players[1].pos.y = Fix::FromInt(400);
    s.players[1].player_state = static_cast<uint8_t>(PlayerState::ThrowIn);
    s.players[3].team_number = 1;
    s.players[3].pos.x = Fix::FromInt(150);
    s.players[3].pos.y = Fix::FromInt(420);

    s.sides[0].control.quick_fire = 1;
    REQUIRE(ApplyRestartTake(s, 0));
    CHECK(s.ball.dest_x == 150);
    CHECK(s.ball.dest_y == 420);
    CHECK(s.ball.speed == kRestartThrowPassSpeed);
    CHECK(s.ball.delta.z.Raw() == kRestartThrowPassDeltaZRaw);
    CHECK(s.sides[0].control.pass_in_progress == 1);

    MatchState long_throw{};
    BeginRestart(long_throw, GameState::ThrowInCentreLeft, 70, 400, 0x1F, 4, 1);
    long_throw.sides[0].control.controlled_slot = 1;
    long_throw.sides[0].control.normal_fire = 1;
    long_throw.sides[0].control.current_allowed_direction =
        static_cast<int16_t>(Dir::E);
    long_throw.players[1].team_number = 1;
    long_throw.players[1].pos.x = Fix::FromInt(67);
    long_throw.players[1].pos.y = Fix::FromInt(400);
    REQUIRE(ApplyRestartTake(long_throw, 0));
    CHECK(long_throw.ball.speed == kRestartThrowSpeed);
    CHECK(long_throw.ball.delta.z.Raw() == kRestartThrowDeltaZRaw);
    CHECK(long_throw.ball.delta.z.Raw() > kRestartThrowPassDeltaZRaw);
}

TEST_CASE("throw-in shortfall summons approach receiver") {
    MatchState s{};
    BeginRestart(s, GameState::ThrowInCentreLeft, 70, 400, 0x1F, 4, 1);
    CHECK(s.sides[0].control.long_pass == kRestartShortfallTicks);

    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E);
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.cards = 0;
        e.pos.x = Fix::FromInt(200 + i * 5);
        e.pos.y = Fix::FromInt(500);
    }
    PlaceThrowInTaker(s, 0);
    const int thrower = s.sides[0].control.controlled_slot;

    MatchInput in{};
    in.p1.dir = Dir::E;
    for (int i = 0; i < kRestartShortfallTicks + 4; ++i) {
        s.globals.team_switch_counter = 1;
        ApplyTeamControls(s, in);
    }

    CHECK(s.sides[0].control.long_pass == -1);
    CHECK(s.sides[0].control.pass_to_slot >= 0);
    CHECK(s.sides[0].control.pass_to_slot != thrower);
    const int recv = s.sides[0].control.pass_to_slot;
    const Entity& th = s.players[static_cast<size_t>(thrower)];
    const Entity& r = s.players[static_cast<size_t>(recv)];
    CHECK(r.dest_x > th.pos.x.Whole());
}

TEST_CASE("free kick tap targets pass_to") {
    MatchState s{};
    BeginRestart(s, GameState::FreeKickCentre, 336, 400, kTurnFlagsAll, 4, 1);
    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.pass_to_slot = 3;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.quick_fire = 1;
    s.players[1].team_number = 1;
    s.players[1].pos.x = Fix::FromInt(336);
    s.players[1].pos.y = Fix::FromInt(400);
    s.players[3].team_number = 1;
    s.players[3].pos.x = Fix::FromInt(340);
    s.players[3].pos.y = Fix::FromInt(360);

    REQUIRE(ApplyRestartTake(s, 0));
    CHECK(s.ball.dest_x == 340);
    CHECK(s.ball.dest_y == 360);
    CHECK(s.ball.speed == kRestartKickPassSpeed);
    CHECK(s.sides[0].control.pass_in_progress == 1);
}

TEST_CASE("free kick shortfall summons approach receiver") {
    MatchState s{};
    BeginRestart(s, GameState::FreeKickCentre, 336, 400, kTurnFlagsAll, 4, 1);
    CHECK(s.sides[0].control.long_pass == kRestartShortfallTicks);

    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.cards = 0;
        e.pos.x = Fix::FromInt(200 + i * 5);
        e.pos.y = Fix::FromInt(500);
    }
    PlaceTakerNearSpot(s, 0);
    const int taker = s.sides[0].control.controlled_slot;
    s.players[static_cast<size_t>(taker)].direction =
        static_cast<int16_t>(Dir::N);

    MatchInput in{};
    in.p1.dir = Dir::N;
    for (int i = 0; i < kRestartShortfallTicks + 4; ++i) {
        s.globals.team_switch_counter = 1;
        ApplyTeamControls(s, in);
    }

    CHECK(s.sides[0].control.long_pass == -1);
    CHECK(s.sides[0].control.pass_to_slot >= 0);
    CHECK(s.sides[0].control.pass_to_slot != taker);
    const int recv = s.sides[0].control.pass_to_slot;
    const Entity& th = s.players[static_cast<size_t>(taker)];
    const Entity& r = s.players[static_cast<size_t>(recv)];
    CHECK(r.dest_y < th.pos.y.Whole()); // approach stand north of taker
}
