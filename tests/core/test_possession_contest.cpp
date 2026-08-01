// B7: possession contest near coin-flip; table clamp at gap 7.
#include <doctest/doctest.h>

#include "core/rng.hpp"
#include "core/tackling.hpp"

using namespace at;

TEST_CASE("equal attrs win about half over many rolls") {
    MatchState s{};
    s.resolve_rng.Seed(0xB700C001u);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[11].team_number = 2;
    s.players[11].player_ordinal = 2;
    s.sides[0].squad[1].attrs.tackling = 4;
    s.sides[0].squad[1].attrs.ball_control = 4;
    s.sides[1].squad[1].attrs.tackling = 4;
    s.sides[1].squad[1].attrs.ball_control = 4;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    int wins0 = 0;
    constexpr int N = 256;
    for (int i = 0; i < N; ++i) {
        s.sides[0].control.won_the_ball_timer = 0;
        s.sides[1].control.won_the_ball_timer = 0;
        s.ball.speed = 500;
        const int w = ResolvePossessionContest(s, 0, 11);
        if (w == 0) ++wins0;
        CHECK(s.ball.speed == 0);
        CHECK(s.sides[static_cast<size_t>(w)].control.won_the_ball_timer ==
              kWonTheBallTicks);
    }
    // 50% expected; allow wide band for 256 samples.
    CHECK(wins0 > 80);
    CHECK(wins0 < 176);
}

TEST_CASE("attribute gap clamps at 7") {
    MatchState s{};
    s.resolve_rng.Seed(1);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[11].team_number = 2;
    s.players[11].player_ordinal = 2;
    s.sides[0].squad[1].attrs.tackling = 15;
    s.sides[0].squad[1].attrs.ball_control = 15;
    s.sides[1].squad[1].attrs.tackling = 0;
    s.sides[1].squad[1].attrs.ball_control = 0;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    int wins0 = 0;
    constexpr int N = 256;
    for (int i = 0; i < N; ++i) {
        s.sides[0].control.won_the_ball_timer = 0;
        s.sides[1].control.won_the_ball_timer = 0;
        if (ResolvePossessionContest(s, 0, 11) == 0) ++wins0;
    }
    // Threshold 23/32 ≈ 72% for favoured after clamp.
    CHECK(wins0 > 140);
    CHECK(wins0 < 230);
}
