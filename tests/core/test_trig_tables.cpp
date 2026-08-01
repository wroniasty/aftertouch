#include <doctest/doctest.h>

#include <cmath>

#include "core/trig_tables.hpp"

using namespace at;

TEST_CASE("kAngleTangent matches round(32*y/x) with the fold") {
    CHECK(kAngleTangent[0][0] == -1);
    for (int x = 1; x < 32; ++x) {
        CHECK(kAngleTangent[0][x] == 0);
        CHECK(kAngleTangent[x][0] == 64);
    }
    for (int y = 1; y < 32; ++y) {
        for (int x = 1; x < 32; ++x) {
            if (x >= y) {
                const int expect = (64 * y + x) / (2 * x);
                CHECK(kAngleTangent[y][x] == expect);
            } else {
                CHECK(kAngleTangent[y][x] == 64 - kAngleTangent[x][y]);
            }
        }
    }
}

TEST_CASE("kSineCosineTable matches round(32767*sin) and >>8 is stable") {
    int hi_mismatches = 0;
    for (int i = 0; i < 256; ++i) {
        const int expect =
            static_cast<int>(std::round(32767.0 * std::sin(2.0 * 3.14159265358979323846 * i / 256.0)));
        CHECK(kSineCosineTable[static_cast<size_t>(i)] == expect);
        // The consumption path is (v >> 8) * speed. Documented ±1 transcription
        // drift in the reference is unobservable here because we generate the
        // mathematical table; this assertion pins the lane the engine uses.
        CHECK((kSineCosineTable[static_cast<size_t>(i)] >> 8) == (expect >> 8));
        (void)hi_mismatches;
    }
    // Cardinals
    CHECK(kSineCosineTable[0] == 0);
    CHECK(kSineCosineTable[64] == 32767);
    CHECK(kSineCosineTable[128] == 0);
    CHECK(kSineCosineTable[192] == -32767);
}
