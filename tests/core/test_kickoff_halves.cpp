// Kickoff placement stays in own halves (fictional tactics + team_playing_up).
#include <doctest/doctest.h>

#include "core/match_engine.hpp"
#include "core/match_state.hpp"
#include "core/movement.hpp"
#include "data/fictional.hpp"
#include "data/game_data.hpp"

using namespace at;

static void FillTacticsFromLeague(MatchState& s) {
    const data::League league = data::MakeFictionalLeague();
    REQUIRE(data::ApplyKickoff(league, 1, 2, s));
}

TEST_CASE("kickoff outfielders stay in defending halves") {
    MatchState s{};
    FillTacticsFromLeague(s);
    s.globals.team_playing_up = 1; // home defends top
    PlaceBallAtCentre(s);
    PlacePlayersAtKickoff(s);

    for (int i = 1; i < 11; ++i) { // home outfield
        const int16_t y = s.players[static_cast<size_t>(i)].pos.y.Whole();
        CHECK(y < kCentreSpotY);
    }
    for (int i = 12; i < 22; ++i) { // away outfield
        const int16_t y = s.players[static_cast<size_t>(i)].pos.y.Whole();
        CHECK(y > kCentreSpotY);
    }
    // Keepers in their boxes.
    CHECK(s.players[0].pos.y.Whole() <= 161);
    CHECK(s.players[11].pos.y.Whole() >= 737);
}

TEST_CASE("kickoff respects flipped team_playing_up") {
    MatchState s{};
    FillTacticsFromLeague(s);
    s.globals.team_playing_up = 2; // away defends top
    PlaceBallAtCentre(s);
    PlacePlayersAtKickoff(s);

    for (int i = 1; i < 11; ++i) {
        CHECK(s.players[static_cast<size_t>(i)].pos.y.Whole() > kCentreSpotY);
    }
    for (int i = 12; i < 22; ++i) {
        CHECK(s.players[static_cast<size_t>(i)].pos.y.Whole() < kCentreSpotY);
    }
}

TEST_CASE("engine boot places both sides in halves") {
    MatchEngine eng;
    eng.Reset(0xC0FFEE02u);
    MatchState sheet = eng.State();
    FillTacticsFromLeague(sheet);
    sheet.gameplay_rng = eng.State().gameplay_rng;
    sheet.presentation_rng = eng.State().presentation_rng;
    sheet.resolve_rng = eng.State().resolve_rng;
    eng.LoadState(sheet);
    eng.Step(MatchInput{}); // place + roll ends

    const MatchState& st = eng.State();
    const bool home_top = st.globals.team_playing_up == 1;
    for (int i = 1; i < 11; ++i) {
        const int16_t y = st.players[static_cast<size_t>(i)].pos.y.Whole();
        if (home_top)
            CHECK(y < kCentreSpotY);
        else
            CHECK(y > kCentreSpotY);
    }
}
