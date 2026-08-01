#include <doctest/doctest.h>

#include "data/fictional.hpp"
#include "harness/season_runner.hpp"

using namespace at;
using namespace at::harness;

TEST_CASE("headless season runner plays every ordered pair") {
    const data::League league = data::MakeFictionalLeague();
    const SeasonReport report = RunRoundRobin(league, /*ticks*/ 25, /*seed*/ 7);

    const size_t n = league.teams.size();
    REQUIRE(report.matches.size() == n * (n - 1));
    for (const auto& m : report.matches) {
        CHECK(m.squad_ok);
        CHECK(m.ticks == 25u);
        CHECK(m.home_id != m.away_id);
    }
}
