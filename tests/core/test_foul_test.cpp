// B7: foul ladder, keeper exemption, good-tackle skip.
#include <doctest/doctest.h>

#include "core/tackling.hpp"

using namespace at;

namespace {

MatchState MakeFoulScene(int16_t tackle_state, int16_t victim_dir,
                         int16_t victim_ball_dist, int victim_ordinal = 2) {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[1].control.controlled_slot = 11;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].speed = 800;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Tackling);
    s.players[0].tackle_state = tackle_state;
    s.players[11].team_number = 2;
    s.players[11].player_ordinal = static_cast<int16_t>(victim_ordinal);
    s.players[11].pos.x = Fix::FromInt(301);
    s.players[11].pos.y = Fix::FromInt(401);
    s.players[11].direction = victim_dir;
    s.players[11].ball_distance = victim_ball_dist;
    s.players[11].player_state = static_cast<uint8_t>(PlayerState::Normal);
    return s;
}

} // namespace

TEST_CASE("foul when never touched ball from behind") {
    MatchState s = MakeFoulScene(kTackleStateNone, static_cast<int16_t>(Dir::N), 10);
    SetPl(s, GameStatePl::InProgress);
    PlayerTacklingTestFoul(s, 0, 0);
    CHECK(static_cast<PlayerState>(s.players[11].player_state) == PlayerState::Tackled);
    CHECK(s.sides[0].stats.fouls_conceded == 1);
    CHECK(s.globals.foul_x == 301);
    CHECK(GetPl(s) == GameStatePl::Stopped); // B8 restart
}

TEST_CASE("good tackle skips foul") {
    MatchState s = MakeFoulScene(kTackleStateGood, static_cast<int16_t>(Dir::N), 10);
    PlayerTacklingTestFoul(s, 0, 0);
    CHECK(static_cast<PlayerState>(s.players[11].player_state) == PlayerState::Tackled);
    CHECK(s.sides[0].stats.fouls_conceded == 0);
}

TEST_CASE("keeper absorbs tackle without foul") {
    MatchState s = MakeFoulScene(kTackleStateNone, static_cast<int16_t>(Dir::N), 10, 1);
    const int16_t before = s.players[0].speed;
    PlayerTacklingTestFoul(s, 0, 0);
    CHECK(static_cast<PlayerState>(s.players[11].player_state) == PlayerState::Normal);
    CHECK(s.sides[0].stats.fouls_conceded == 0);
    CHECK(s.players[0].speed == static_cast<int16_t>((before >> 2) | 1));
}

TEST_CASE("recovery table uses tackling attr") {
    MatchState s{};
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.sides[0].squad[1].attrs.tackling = 0;
    s.players[0].tackling_timer = 5;
    SetPlayerDowntimeAfterTackle(s, s.players[0]);
    CHECK(s.players[0].player_down_timer == 30);

    s.players[0].tackling_timer = -1;
    SetPlayerDowntimeAfterTackle(s, s.players[0]);
    CHECK(s.players[0].player_down_timer == 3);
}
