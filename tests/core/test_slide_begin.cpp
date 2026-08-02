// B7: fire without ball starts a slide.
#include <doctest/doctest.h>

#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/tackling.hpp"

using namespace at;

TEST_CASE("fire without ball begins slide at tackling speed") {
    MatchEngine eng;
    eng.Reset(0xB7000001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.player_has_ball = 0;
    PlacePlayersAtKickoff(s);
    // Ball far but still in play (y>=129) so OOP does not stop the match;
    // out of header proximity → slide.
    s.ball.pos.x = Fix::FromInt(200);
    s.ball.pos.y = Fix::FromInt(200);
    s.ball.pos.z = Fix{};
    s.players[9].pos.x = Fix::FromInt(336);
    s.players[9].pos.y = Fix::FromInt(449);
    s.players[9].direction = static_cast<int16_t>(Dir::N);
    s.players[9].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[9].team_number = 1;
    s.players[9].player_ordinal = 10;
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    in.p1.dir = Dir::N;
    in.p1.fire = true;
    // Need a side-0 control tick after fire edge.
    for (int i = 0; i < 4; ++i) eng.Step(in);

    const Entity& e = eng.State().players[9];
    CHECK(static_cast<PlayerState>(e.player_state) == PlayerState::Tackling);
    // Speed decays under ground friction after entry; must still be sliding.
    CHECK(e.speed > 0);
    CHECK(e.speed <= kPlayerTacklingSpeed);
    CHECK(e.direction == static_cast<int16_t>(Dir::N));
    CHECK(e.tackle_state >= kTackleStateNone);
    CHECK(e.tackle_state <= kTackleStateGood);
}

TEST_CASE("BeginSlide helper locks dest ahead") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E);
    s.players[0].pos.x = Fix::FromInt(200);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].team_number = 1;
    BeginSlide(s, 0);
    CHECK(s.players[0].speed == kPlayerTacklingSpeed);
    CHECK(s.players[0].direction == static_cast<int16_t>(Dir::E));
    CHECK(s.players[0].dest_x == static_cast<int16_t>(200 + 1000));
    CHECK(s.sides[0].control.header_or_tackle == 1);
}
