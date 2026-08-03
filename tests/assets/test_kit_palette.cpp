// C3 — a kit is a palette. Pure: no SDL, no window.
#include <doctest/doctest.h>

#include "render/kit_palette.hpp"

#include <array>
#include <vector>

using namespace at;
using namespace at::render;

namespace {

// A stand-in game palette where entry i is (i, i, i) — so "which palette index did
// this region end up pointing at" is readable straight off the output byte.
struct FakePalette {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(256 * 4);
    FakePalette() {
        for (int i = 0; i < 256; ++i) {
            bytes[static_cast<size_t>(i) * 4 + 0] = static_cast<uint8_t>(i);
            bytes[static_cast<size_t>(i) * 4 + 1] = static_cast<uint8_t>(i);
            bytes[static_cast<size_t>(i) * 4 + 2] = static_cast<uint8_t>(i);
            bytes[static_cast<size_t>(i) * 4 + 3] = 255;
        }
    }
    GamePalette View() const { return GamePalette{bytes, 256}; }
};

uint8_t RedAt(const KitPalette& p, uint8_t index) {
    return p.rgba[static_cast<size_t>(index) * 4];
}

KitSpec Kit(uint8_t type, uint8_t shirt, uint8_t stripes, uint8_t shorts, uint8_t socks) {
    KitSpec k{};
    k.shirt_type = type;
    k.shirt = shirt;
    k.stripes = stripes;
    k.shorts = shorts;
    k.socks = socks;
    return k;
}

} // namespace

TEST_CASE("kit colours land on the four tintable indices and nothing else moves") {
    FakePalette game;
    KitPalette out;
    // Kit colours 0,1,2,3 → palette ordinals 1,2,3,6 (RENDERING.md §5).
    BuildKitPalette(game.View(), {}, Kit(2, 0, 1, 2, 3), 0, out);

    CHECK(RedAt(out, kIdxShirt) == 1);
    CHECK(RedAt(out, kIdxStripes) == 2);
    CHECK(RedAt(out, kIdxShorts) == 3);
    CHECK(RedAt(out, kIdxSocks) == 6);

    // Face 0 keeps the art's own skin and hair, and every other index is untouched.
    for (uint8_t i : kIdxSkin) CHECK(RedAt(out, i) == i);
    for (uint8_t i : kIdxHair) CHECK(RedAt(out, i) == i);
    for (int i = 0; i < 256; ++i) {
        if (i == kIdxShirt || i == kIdxStripes || i == kIdxShorts || i == kIdxSocks)
            continue;
        CHECK(RedAt(out, static_cast<uint8_t>(i)) == static_cast<uint8_t>(i));
    }
}

TEST_CASE("a plain shirt paints the stripe index with the base colour") {
    FakePalette game;
    KitPalette plain, striped;
    BuildKitPalette(game.View(), {}, Kit(0, 4, 9, 0, 0), 0, plain);
    BuildKitPalette(game.View(), {}, Kit(2, 4, 9, 0, 0), 0, striped);

    CHECK(RedAt(plain, kIdxShirt) == RedAt(plain, kIdxStripes));
    CHECK(RedAt(striped, kIdxShirt) != RedAt(striped, kIdxStripes));
}

TEST_CASE("faces change skin and hair, never the kit") {
    FakePalette game;
    KitPalette f0, f1, f2;
    const KitSpec k = Kit(2, 0, 1, 2, 3);
    BuildKitPalette(game.View(), {}, k, 0, f0);
    BuildKitPalette(game.View(), {}, k, 1, f1);
    BuildKitPalette(game.View(), {}, k, 2, f2);

    // Ginger changes hair only.
    CHECK(RedAt(f1, kIdxHair[1]) != RedAt(f0, kIdxHair[1]));
    for (uint8_t i : kIdxSkin) CHECK(RedAt(f1, i) == RedAt(f0, i));
    // The dark face changes both.
    CHECK(RedAt(f2, kIdxSkin[1]) != RedAt(f0, kIdxSkin[1]));

    for (const KitPalette* p : {&f1, &f2}) {
        CHECK(RedAt(*p, kIdxShirt) == RedAt(f0, kIdxShirt));
        CHECK(RedAt(*p, kIdxShorts) == RedAt(f0, kIdxShorts));
        CHECK(RedAt(*p, kIdxSocks) == RedAt(f0, kIdxSocks));
    }
}

TEST_CASE("the pack's own ordinal table wins over the fallback") {
    FakePalette game;
    const std::array<uint8_t, kKitColourCount> odd = {200, 201, 202, 203, 204,
                                                      205, 206, 207, 208, 209};
    KitPalette out;
    BuildKitPalette(game.View(), odd, Kit(2, 0, 1, 2, 3), 0, out);
    CHECK(RedAt(out, kIdxShirt) == 200);
    CHECK(RedAt(out, kIdxStripes) == 201);
}

TEST_CASE("ShirtType selects the geometry bank; plain shares vertical stripes") {
    CHECK(GeometryForShirtType(0) == ShirtGeometry::VerticalStripes);
    CHECK(GeometryForShirtType(2) == ShirtGeometry::VerticalStripes);
    CHECK(GeometryForShirtType(3) == ShirtGeometry::HorizontalStripes);
    CHECK(GeometryForShirtType(1) == ShirtGeometry::ColouredSleeves);
}

TEST_CASE("colliding shirts send the away side to its change kit") {
    TeamSheet a{}, b{};
    a.primary = Kit(0, 5, 5, 0, 0);
    a.secondary = Kit(0, 1, 1, 0, 0);
    b.primary = Kit(0, 5, 5, 9, 9);   // same shirt colour as a
    b.secondary = Kit(0, 8, 8, 9, 9);

    const KitChoice c = ResolveKits(a, b);
    CHECK(c.home.shirt == 5);
    CHECK(c.away.shirt == 8);

    // No clash: both keep their first kit.
    b.primary = Kit(0, 3, 3, 9, 9);
    const KitChoice d = ResolveKits(a, b);
    CHECK(d.home.shirt == 5);
    CHECK(d.away.shirt == 3);
}

TEST_CASE("KitBank bakes both sides, three faces each, and two keeper kits") {
    FakePalette game;
    TeamSheet a{}, b{};
    a.primary = Kit(2, 0, 1, 2, 3);
    b.primary = Kit(1, 4, 5, 6, 7);

    KitBank bank;
    CHECK_FALSE(bank.Ready());
    bank.Build(game.View(), {}, a, b);
    CHECK(bank.Ready());

    CHECK(bank.Geometry(0) == ShirtGeometry::VerticalStripes);
    CHECK(bank.Geometry(1) == ShirtGeometry::ColouredSleeves);

    // Distinct buffers per (side, face): the sprite texture cache keys on the address,
    // so sharing one would collapse two teams onto one texture set.
    CHECK(bank.Outfield(0, 0).Bytes().data() != bank.Outfield(1, 0).Bytes().data());
    CHECK(bank.Outfield(0, 0).Bytes().data() != bank.Outfield(0, 1).Bytes().data());
    CHECK(bank.Keeper(0).Bytes().data() != bank.Keeper(1).Bytes().data());

    // The two sides really do wear different shirts.
    CHECK(RedAt(bank.Outfield(0, 0), kIdxShirt) != RedAt(bank.Outfield(1, 0), kIdxShirt));
    // And the keepers wear neither side's shirt colour.
    for (int s = 0; s < 2; ++s) {
        CHECK(RedAt(bank.Keeper(s), kIdxShirt) != RedAt(bank.Outfield(0, 0), kIdxShirt));
        CHECK(RedAt(bank.Keeper(s), kIdxShirt) != RedAt(bank.Outfield(1, 0), kIdxShirt));
    }
}

TEST_CASE("a source with no palette still produces a usable one") {
    KitPalette out;
    BuildKitPalette(GamePalette{}, {}, Kit(2, 0, 1, 2, 3), 0, out);
    CHECK(out.rgba[3] == 0);          // index 0 stays transparent
    CHECK(out.Bytes().size() == 256 * 4);
}
