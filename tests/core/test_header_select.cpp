// B7: band → static/jump; direction switch lob/drive.
#include <doctest/doctest.h>

#include "core/heading.hpp"
#include "core/possession.hpp"
#include "core/tackling.hpp"

using namespace at;

TEST_CASE("high band selects jump header") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.player_has_ball = 0;
    s.sides[0].control.fire_this_frame = 1;
    s.sides[0].control.pl_not_far_from_ball = 1;
    s.sides[0].control.ball_8_to_12 = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    CHECK(TryBeginSlideOrHeader(s, 0));
    CHECK(static_cast<PlayerState>(s.players[0].player_state) ==
          PlayerState::JumpHeader);
    CHECK(s.players[0].speed == kJumpHeaderSpeed);
}

TEST_CASE("near low ball selects static header") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.player_has_ball = 0;
    s.sides[0].control.fire_this_frame = 1;
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.pl_not_far_from_ball = 1;
    s.sides[0].control.ball_less_equal_4 = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::S);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    CHECK(TryBeginSlideOrHeader(s, 0));
    CHECK(static_cast<PlayerState>(s.players[0].player_state) ==
          PlayerState::StaticHeader);
    CHECK(s.players[0].speed == kStaticHeaderPlayerSpeed);
    CHECK(s.players[0].player_down_timer == kStaticHeaderDownTime);
}

TEST_CASE("jump header relative stick selects lob and flying") {
    CHECK(JumpHeaderTrajectory(4) == HeaderTrajectory::Lob);
    CHECK(JumpHeaderTrajectory(2) == HeaderTrajectory::Flying);
    CHECK(JumpHeaderTrajectory(0) == HeaderTrajectory::Base);
    CHECK(JumpHeaderAimDir(0, 7, 1) == 7); // facing N, hold NW → aim −1
}
