// B12: ratings from chronicle + match_stats; no gameplay coupling.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/performance_rating.hpp"

using namespace at;

namespace {

MatchState BasePlayedSheet() {
    MatchState s{};
    s.score[0] = 1;
    s.score[1] = 1; // no clean sheet
    for (int i = 0; i < 11; ++i) {
        s.sides[0].squad[static_cast<size_t>(i)].half_played = 1;
        s.sides[1].squad[static_cast<size_t>(i)].half_played = 1;
    }
    return s;
}

} // namespace

TEST_CASE("rating goals and cards move the 1-10 score") {
    MatchState s = BasePlayedSheet();
    s.sides[0].squad[9].position = 7; // A
    s.sides[0].squad[5].position = 6; // M
    AppendChronicle(s, MatchEventKind::Goal, 0, 9);
    AppendChronicle(s, MatchEventKind::Goal, 0, 9);
    AppendChronicle(s, MatchEventKind::Yellow, 0, 5);

    const PlayerRating striker = ComputePlayerRating(s, 0, 9);
    const PlayerRating mid = ComputePlayerRating(s, 0, 5);
    // Att: 2 goals * 3 = 6 → sat 5 → base 5 + 5 = 10
    CHECK(striker.rating == 10);
    CHECK(striker.breakdown.goals == 2);
    CHECK(mid.rating == 4); // 5 - 1 yellow
    CHECK(mid.breakdown.yellows == 1);
}

TEST_CASE("same volume stats rate DEF and ATT differently") {
    MatchState s = BasePlayedSheet();
    s.sides[0].squad[3].position = 3; // D
    s.sides[0].squad[9].position = 7; // A
    PlayerMatchStats vol{};
    vol.tackles = 1;
    vol.headers = 1;
    s.sides[0].match_stats[3] = vol;
    s.sides[0].match_stats[9] = vol;

    const PlayerRating def = ComputePlayerRating(s, 0, 3);
    const PlayerRating att = ComputePlayerRating(s, 0, 9);
    // Def: 1*2 + 1*2 = 4 → 9; Att: 1*1 header = 1 → 6
    CHECK(def.rating > att.rating);
    CHECK(def.breakdown.tackles == 1);
    CHECK(att.breakdown.tackles == 1);
}

TEST_CASE("pass completion moves MID more than ATT") {
    MatchState s = BasePlayedSheet();
    s.sides[0].squad[6].position = 6; // M
    s.sides[0].squad[9].position = 7; // A
    PlayerMatchStats vol{};
    vol.passes_attempted = 8;
    vol.passes_completed = 6;
    s.sides[0].match_stats[6] = vol;
    s.sides[0].match_stats[9] = vol;

    const uint8_t mid_r = ComputePlayerRating(s, 0, 6).rating;
    const uint8_t att_r = ComputePlayerRating(s, 0, 9).rating;
    // Mid: 6*2/3 + 8*1/8 = 5 → 10; Att: 6*1/3 = 2 → 7
    CHECK(mid_r > att_r);
}

TEST_CASE("saves move GK rating") {
    MatchState s = BasePlayedSheet();
    s.sides[0].squad[0].position = 0; // GK
    s.sides[0].match_stats[0].saves = 4;

    const PlayerRating gk = ComputePlayerRating(s, 0, 0);
    CHECK(gk.breakdown.saves == 4);
    CHECK(gk.rating >= 7); // 5 + sat(4*2=8→5) = 10, or with concede still +saves
}

TEST_CASE("fouls drag DEF more than ATT") {
    MatchState s = BasePlayedSheet();
    s.sides[0].squad[3].position = 3; // D
    s.sides[0].squad[9].position = 7; // A
    s.sides[0].match_stats[3].fouls_conceded = 3;
    s.sides[0].match_stats[9].fouls_conceded = 3;

    const PlayerRating def = ComputePlayerRating(s, 0, 3);
    const PlayerRating att = ComputePlayerRating(s, 0, 9);
    CHECK(def.rating < att.rating);
}

TEST_CASE("ComputeMatchRatings does not change HashState") {
    MatchEngine eng;
    eng.Reset(0xB1200001u);
    for (int i = 0; i < 40; ++i) eng.Step(MatchInput{});
    const uint64_t before = HashState(eng.State());
    MatchRatings ignored = ComputeMatchRatings(eng.State());
    (void)ignored;
    CHECK(HashState(eng.State()) == before);
}

TEST_CASE("start XI get base ratings") {
    MatchState s{};
    s.score[0] = 1;
    s.score[1] = 1;
    for (int i = 0; i < 11; ++i) {
        s.sides[0].squad[static_cast<size_t>(i)].half_played = 1;
        // Explicit band so clean-sheet GK bonus is not the only mover.
        s.sides[0].squad[static_cast<size_t>(i)].position =
            (i == 0) ? 0 : (i <= 4 ? 3 : (i <= 8 ? 6 : 7));
    }
    const MatchRatings r = ComputeMatchRatings(s);
    for (int i = 0; i < 11; ++i)
        CHECK(r.by_side[0][static_cast<size_t>(i)].rating == 5);
}
