// A6 acceptance: committed golden matches regeneration; a physics mutate fails it.
#include <doctest/doctest.h>

#include <string>

#include "core/fixed.hpp"
#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

namespace {

std::string GoldenPath() {
#ifdef AT_GOLDEN_DIR
    return std::string(AT_GOLDEN_DIR) + "/kickoff.attr";
#else
    return "tests/golden/kickoff.attr";
#endif
}

void BumpBallX(MatchState& s) {
    s.ball.pos.x = Fix::FromRaw(s.ball.pos.x.Raw() + 1);
}

} // namespace

TEST_CASE("kickoff golden matches a freshly generated engine trace") {
    const Scenario s = KickoffScenario(100);
    std::vector<uint8_t> generated;
    REQUIRE(Generate(s, generated));

    std::vector<uint8_t> golden;
    const std::string path = GoldenPath();
    REQUIRE(ReadFile(path.c_str(), golden));

    const DiffResult r = Diff(generated, golden);
    CAPTURE(path);
    if (r.reason) {
        CAPTURE(r.reason);
    }
    CHECK(r.identical);
}

TEST_CASE("a deliberate one-raw-unit physics change fails the golden") {
    const Scenario s = KickoffScenario(100);
    std::vector<uint8_t> golden;
    REQUIRE(ReadFile(GoldenPath().c_str(), golden));

    std::vector<uint8_t> buggy;
    REQUIRE(Generate(s, buggy, BumpBallX));

    const DiffResult r = Diff(buggy, golden);
    CHECK_FALSE(r.identical);
    CHECK(r.reason != nullptr);
    // First tick the ball is written (tick 1 after first Step) must already differ.
    CHECK(r.tick >= 1u);
}
