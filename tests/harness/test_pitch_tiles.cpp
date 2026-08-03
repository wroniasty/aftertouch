// C1: pure pitch tile helpers (no SDL).
#include <doctest/doctest.h>

#include "render/asset_source.hpp"

#include <array>
#include <vector>

using namespace at;

TEST_CASE("PitchDrawableRows: original draws all 55, legacy clamps to 53") {
    PitchTiles pt{};
    pt.grid_w = kPitchGridW;
    pt.grid_h = kPitchGridH;

    pt.from_original = true;
    CHECK(PitchDrawableRows(pt) == kPitchWorldRows); // 55 — stands included
    pt.from_original = false;
    CHECK(PitchDrawableRows(pt) == kPitchLegacyRows); // 53 — pad excluded

    pt.grid_h = 40;
    CHECK(PitchDrawableRows(pt) == 40);
}

TEST_CASE("PitchMatrixRow: original is 1:1, legacy skips the leading pad") {
    PitchTiles pt{};
    pt.grid_h = kPitchGridH; // 55

    // C3 §1 Finding 3: measured against the painted markings, not inferred.
    pt.from_original = true;
    CHECK(PitchMatrixRow(pt, 0) == 0);
    CHECK(PitchMatrixRow(pt, 54) == 54);

    pt.from_original = false;
    CHECK(PitchMatrixRow(pt, 0) == 1);
    CHECK(PitchMatrixRow(pt, 52) == 53);
    pt.grid_h = kPitchLegacyRows; // exactly 53 — no pad
    CHECK(PitchMatrixRow(pt, 0) == 0);
    CHECK(PitchMatrixRow(pt, 52) == 52);
}

TEST_CASE("PitchGridIndex reads LE u16 and empty outside") {
    // 2×2 grid: 0, 1 / 0xFFFF, 2
    const uint8_t idx[] = {0, 0, 1, 0, 0xFF, 0xFF, 2, 0};
    PitchTiles pt{};
    pt.grid_w  = 2;
    pt.grid_h  = 2;
    pt.indices = idx;
    CHECK(PitchGridIndex(pt, 0, 0) == 0);
    CHECK(PitchGridIndex(pt, 1, 0) == 1);
    CHECK(PitchGridIndex(pt, 0, 1) == kPitchEmptyCell);
    CHECK(PitchGridIndex(pt, 1, 1) == 2);
    CHECK(PitchGridIndex(pt, -1, 0) == kPitchEmptyCell);
    CHECK(PitchGridIndex(pt, 2, 0) == kPitchEmptyCell);
}

TEST_CASE("ExpandIndexed applies palette and default ramp") {
    const uint8_t indexed[] = {0, 1, 2, 1};
    const uint8_t pal[]     = {
        0,   0,   0,   0,    // 0 transparent black
        10,  20,  30,  255,  // 1
        40,  50,  60,  255,  // 2
    };
    std::array<uint8_t, 16> out{};
    CHECK(ExpandIndexed(indexed, 2, 2, pal, 3, out));
    CHECK(out[0] == 0);
    CHECK(out[3] == 0);
    CHECK(out[4] == 10);
    CHECK(out[5] == 20);
    CHECK(out[6] == 30);
    CHECK(out[7] == 255);
    CHECK(out[8] == 40);

    std::array<uint8_t, 16> out2{};
    CHECK(ExpandIndexed(indexed, 2, 2, {}, 0, out2));
    CHECK(out2[3] == 0);   // index 0 → a=0 in default ramp
    CHECK(out2[7] == 255); // index 1 opaque
}

TEST_CASE("world size matches the original's own matrix") {
    CHECK(kPitchWorldW == 672);
    CHECK(kPitchWorldH == 880);
    CHECK(kPitchWorldCols * kPitchTileSize == kPitchWorldW);
    CHECK(kPitchWorldRows * kPitchTileSize == kPitchWorldH);
    // CAMERA.md §9's hard limits are the independent check: a 320×200 view of an
    // 880-tall world has its top-left corner in [0, 680], which is kCameraMaxY.
    CHECK(kPitchWorldH - 200 == 680);
    CHECK(kPitchWorldW - 320 == 352);
}
