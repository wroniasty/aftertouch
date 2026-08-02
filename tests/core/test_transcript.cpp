// Sparse ATTR → text transcript (agent-readable).
#include <doctest/doctest.h>

#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

TEST_CASE("sparse transcript contains header and input tokens") {
    Scenario s = KickoffScenario(30);
    std::string text;
    REQUIRE(WriteSparseTranscript(s, text));
    CHECK(text.find("# ATTR transcript") != std::string::npos);
    CHECK(text.find("seed=0xA5A50001") != std::string::npos);
    CHECK(text.find("t=") != std::string::npos);
    CHECK(text.find("in: P1=") != std::string::npos);
    CHECK(text.find("Hpos:") != std::string::npos);
    CHECK(text.find("Apos:") != std::string::npos);
    CHECK(text.find("0@(") != std::string::npos);
    CHECK(text.find("11@(") != std::string::npos);
    // KickoffScenario fires at tick 10.
    CHECK(text.find("+fire") != std::string::npos);
}

TEST_CASE("DirToken covers octants") {
    CHECK(std::string(DirToken(Dir::None)) == "-");
    CHECK(std::string(DirToken(Dir::N)) == "N");
    CHECK(std::string(DirToken(Dir::SE)) == "SE");
}
