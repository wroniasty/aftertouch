// B4: tactics-grid off-ball destinations and bottom mirror.
#include <doctest/doctest.h>

#include "core/movement.hpp"

using namespace at;

TEST_CASE("tactics cell maps to quadrant coordinates") {
    // Cell (x=7,y=8) → (336, 469) before nudge; top nudge −4 → 332.
    const uint8_t cell = static_cast<uint8_t>((7u << 4) | 8u);
    Dest d = TacticsDestForCell(cell, 0);
    CHECK(d.x == 332);
    CHECK(d.y == 469);
}

TEST_CASE("bottom side mirrors quadrant index and cell") {
    MatchState s{};
    PlaceBallAtCentre(s);
    // Centre ≈ col mid, row mid → some index; set role 0 cells so top≠bottom.
    for (int q = 0; q < kMatchBallQuadrants; ++q) {
        s.sides[0].tactics.cells[0][static_cast<size_t>(q)] =
            static_cast<uint8_t>((3u << 4) | 4u);
        s.sides[1].tactics.cells[0][static_cast<size_t>(q)] =
            static_cast<uint8_t>((3u << 4) | 4u);
    }
    const Dest top = OffBallDestination(s, 0, 2); // ordinal 2 → role 0
    const Dest bot = OffBallDestination(s, 1, 2);
    // Mirror of cell 0x34 is 0xEF-0x34 = 0xBB → x=11,y=11
    CHECK(top.x != bot.x);
    CHECK(bot.x == TacticsDestForCell(static_cast<uint8_t>(0xEF - 0x34), 1).x);
}

TEST_CASE("off-ball dest is clamped to playable") {
    Dest d = TacticsDestForCell(0x00, 0);
    CHECK(d.x >= kOffBallMinX);
    CHECK(d.y >= kOffBallMinY);
    Dest far = TacticsDestForCell(0xEF, 1);
    CHECK(far.x <= kOffBallMaxX);
    CHECK(far.y <= kOffBallMaxY);
}
