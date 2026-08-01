#include <doctest/doctest.h>

#include "tracekit/hil2.hpp"

using namespace at::hil2;

TEST_CASE("hil2: minimal fixture parses; frame count and scene offsets agree") {
    const std::vector<uint8_t> bytes = MakeMinimalFixture();
    File f;
    REQUIRE(Parse(bytes, f));
    CHECK(f.header.version_major == 2);
    CHECK(f.header.scene_count == 1);
    REQUIRE(f.scenes.size() == 1);
    CHECK(f.scenes[0].start == 8);
    CHECK(f.scenes[0].end == 32);
    REQUIRE(f.frames.size() == 1);
    CHECK(f.frames[0].camera_x == 100);
    CHECK(f.frames[0].camera_y == 200);
    CHECK(std::string(f.header.game_name).find("FRIENDLY") == 0);
}

TEST_CASE("hil2: rejects bad magic and truncated buffers") {
    File f;
    std::vector<uint8_t> bad(100, 0);
    CHECK_FALSE(Parse(bad, f));
    auto ok = MakeMinimalFixture();
    ok[0] = 'X';
    CHECK_FALSE(Parse(ok, f));
}
