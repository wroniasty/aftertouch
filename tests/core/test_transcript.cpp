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

// B6a §4 put a kick line in this transcript so a "shooting feels off" report
// arrives as a tick count instead of an adjective. It was never asserted, and
// the app's live MATCH/SANDBOX recorder — a separate writer — never emitted one
// at all, so the traces actually played and reported carried no telemetry.
// Both writers now share FormatKickLine/FormatCurlLine; this pins the format.
TEST_CASE("a scripted strike emits kick telemetry into the transcript") {
    Scenario s = ShotCurlScenario(80);
    std::string text;
    REQUIRE(WriteSparseTranscript(s, text));

    const size_t kick = text.find("kick: side=");
    REQUIRE(kick != std::string::npos);

    // Everything needed to tell a tap from a hold, and to see where it aimed.
    const std::string line = text.substr(kick, text.find('\n', kick) - kick);
    CHECK(line.find("press->strike=") != std::string::npos);
    CHECK(line.find("hold=") != std::string::npos);
    CHECK(line.find("dir=") != std::string::npos);
    CHECK(line.find("target=") != std::string::npos);
    CHECK(line.find("aim=(") != std::string::npos);
    CHECK((line.find(" shot ") != std::string::npos ||
           line.find(" pass ") != std::string::npos));

    // The curl line lands when the window closes, which is the first tick the
    // latch and vertical decision are final.
    const size_t curl = text.find("curl: side=");
    REQUIRE(curl != std::string::npos);
    const std::string cline = text.substr(curl, text.find('\n', curl) - curl);
    CHECK(cline.find("latch=") != std::string::npos);
    CHECK(cline.find("vert=") != std::string::npos);
    CHECK(curl > kick); // strike first, then the window closing
}

TEST_CASE("kick and curl lines are formatted from telemetry alone") {
    KickTelemetry k{};
    k.was_pass = true;
    k.kick_dir = 2; // E
    k.press_to_strike = 3;
    k.hold_ticks = 3;
    k.pass_target = 7;
    k.aim_x = 967;
    k.aim_y = 478;
    const std::string line = FormatKickLine(0, k);
    CHECK(line.find("side=1 pass dir=E") != std::string::npos);
    CHECK(line.find("target=slot7") != std::string::npos);
    CHECK(line.find("aim=(967,478)") != std::string::npos);

    KickTelemetry shot{};
    shot.was_pass = false;
    shot.kick_dir = 0;
    shot.hold_ticks = 14;
    CHECK(FormatKickLine(1, shot).find("side=2 shot dir=N") != std::string::npos);
    CHECK(FormatKickLine(1, shot).find("target=none") != std::string::npos);

    KickTelemetry c{};
    c.latch_side = -1;
    c.latch_spin = 2;
    c.vertical = VerticalDecision::Lob;
    CHECK(FormatCurlLine(0, c).find("latch=ccw@2") != std::string::npos);
    CHECK(FormatCurlLine(0, c).find("vert=lob") != std::string::npos);
}
