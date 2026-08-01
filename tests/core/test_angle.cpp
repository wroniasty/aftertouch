#include <doctest/doctest.h>

#include "core/angle.hpp"

using namespace at;

TEST_CASE("Dir numbering matches MOVEMENT.md section 3.1") {
    CHECK(static_cast<int>(Dir::None) == -1);
    CHECK(static_cast<int>(Dir::N) == 0);
    CHECK(static_cast<int>(Dir::NE) == 1);
    CHECK(static_cast<int>(Dir::E) == 2);
    CHECK(static_cast<int>(Dir::SE) == 3);
    CHECK(static_cast<int>(Dir::S) == 4);
    CHECK(static_cast<int>(Dir::SW) == 5);
    CHECK(static_cast<int>(Dir::W) == 6);
    CHECK(static_cast<int>(Dir::NW) == 7);
}

TEST_CASE("kDefaultDestinations are the ±1000 compass offsets") {
    CHECK(kDefaultDestinations[0].x == 0);
    CHECK(kDefaultDestinations[0].y == -1000);
    CHECK(kDefaultDestinations[2].x == 1000);
    CHECK(kDefaultDestinations[2].y == 0);
    CHECK(kDefaultDestinations[4].x == 0);
    CHECK(kDefaultDestinations[4].y == 1000);
    CHECK(kDefaultDestinations[6].x == -1000);
    CHECK(kDefaultDestinations[6].y == 0);
    // Diagonals are exact 45°
    CHECK(kDefaultDestinations[1].x == 1000);
    CHECK(kDefaultDestinations[1].y == -1000);
}

TEST_CASE("OctantOf boundaries sit on every multiple of 16") {
    // (h + 16) >> 5 — the +16 bias is where an off-by-one hides.
    CHECK(OctantOf(MakeHeading(0)) == MakeOctant(0));     // 16>>5 = 0
    CHECK(OctantOf(MakeHeading(16)) == MakeOctant(1));    // 32>>5 = 1
    CHECK(OctantOf(MakeHeading(32)) == MakeOctant(1));
    CHECK(OctantOf(MakeHeading(48)) == MakeOctant(2));
    CHECK(OctantOf(MakeHeading(240)) == MakeOctant(0));   // (240+16)=256→0
    CHECK(OctantOf(MakeHeading(255)) == MakeOctant(0));
    for (int h = 0; h < 256; h += 16) {
        const auto o = OctantOf(MakeHeading(static_cast<uint8_t>(h)));
        CHECK(Value(o) <= 7);
    }
}

TEST_CASE("TrigIndex and Heading round-trip") {
    for (int i = 0; i < 256; ++i) {
        const auto t = MakeTrigIndex(static_cast<uint8_t>(i));
        const auto h = HeadingFromTrig(t);
        CHECK(TrigFromHeading(h) == t);
    }
}
