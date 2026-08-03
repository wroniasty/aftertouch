#include "render/kit_palette.hpp"

#include <algorithm>

namespace at::render {

namespace {

// Face ramps. Face 0 is left as the art shipped, so the default look is exactly the
// original's; faces 1 and 2 are ours. The reference stores these as SDL colour-mod
// multipliers over grey-scale layers (RENDERING.md §4), which is not reproducible
// against indexed art without inventing the greys, so we state the ramps directly and
// keep them in one place instead of scattering magic numbers through the renderer.
struct Ramp {
    Rgba8 dark, medium, bright;
};

constexpr Ramp kGingerHair{{120, 48, 0, 255}, {180, 72, 0, 255}, {228, 120, 32, 255}};
constexpr Ramp kDarkHair{{40, 40, 40, 255}, {60, 60, 60, 255}, {92, 92, 92, 255}};
constexpr Ramp kDarkSkin{{54, 18, 0, 255}, {96, 40, 8, 255}, {140, 72, 24, 255}};

void PutRamp(KitPalette& p, const std::array<uint8_t, 3>& idx, const Ramp& r) {
    const Rgba8 shades[3] = {r.dark, r.medium, r.bright};
    for (int i = 0; i < 3; ++i) {
        const size_t at = static_cast<size_t>(idx[static_cast<size_t>(i)]) * 4;
        p.rgba[at + 0] = shades[i].r;
        p.rgba[at + 1] = shades[i].g;
        p.rgba[at + 2] = shades[i].b;
        p.rgba[at + 3] = 255;
    }
}

void PutColour(KitPalette& p, uint8_t index, Rgba8 c) {
    const size_t at = static_cast<size_t>(index) * 4;
    p.rgba[at + 0] = c.r;
    p.rgba[at + 1] = c.g;
    p.rgba[at + 2] = c.b;
    p.rgba[at + 3] = c.a;
}

Rgba8 Read(const KitPalette& p, uint8_t index) {
    const size_t at = static_cast<size_t>(index) * 4;
    return Rgba8{p.rgba[at + 0], p.rgba[at + 1], p.rgba[at + 2], p.rgba[at + 3]};
}

// Seed a palette from the game palette, or from ExpandIndexed's default ramp when a
// source ships none (the placeholder path) so that untinted indices look the same as
// they do everywhere else rather than turning magenta.
void SeedBase(const GamePalette& game, KitPalette& out) {
    for (uint32_t i = 0; i < kPaletteEntries; ++i) {
        const size_t at = static_cast<size_t>(i) * 4;
        if (i < game.count && (static_cast<size_t>(i) * 4 + 3) < game.rgba.size()) {
            out.rgba[at + 0] = game.rgba[i * 4 + 0];
            out.rgba[at + 1] = game.rgba[i * 4 + 1];
            out.rgba[at + 2] = game.rgba[i * 4 + 2];
            out.rgba[at + 3] = game.rgba[i * 4 + 3];
        } else {
            out.rgba[at + 0] = static_cast<uint8_t>(20 + (i % 6) * 8);
            out.rgba[at + 1] = static_cast<uint8_t>(70 + (i % 6) * 18);
            out.rgba[at + 2] = static_cast<uint8_t>(30 + (i % 6) * 6);
            out.rgba[at + 3] = (i == 0) ? 0 : 255;
        }
    }
}

uint8_t OrdinalFor(std::span<const uint8_t> ordinals, uint8_t kit_colour) {
    const auto& table = ordinals.size() >= kKitColourCount
                            ? ordinals
                            : std::span<const uint8_t>(kFallbackKitOrdinals);
    const size_t i = kit_colour < kKitColourCount ? kit_colour : 0;
    return table[i];
}

} // namespace

void BuildKitPalette(const GamePalette& game, std::span<const uint8_t> ordinals,
                     const KitSpec& kit, uint8_t face, KitPalette& out) {
    SeedBase(game, out);

    const Rgba8 shirt   = Read(out, OrdinalFor(ordinals, kit.shirt));
    // Plain shirts have no second colour: paint the stripe index with the base so the
    // vertical-stripe geometry renders as a flat shirt (PLAYER_SPRITES.md §3).
    const bool  plain   = kit.shirt_type == 0;
    const Rgba8 stripes = plain ? shirt : Read(out, OrdinalFor(ordinals, kit.stripes));
    const Rgba8 shorts  = Read(out, OrdinalFor(ordinals, kit.shorts));
    const Rgba8 socks   = Read(out, OrdinalFor(ordinals, kit.socks));

    PutColour(out, kIdxShirt, shirt);
    PutColour(out, kIdxStripes, stripes);
    PutColour(out, kIdxShorts, shorts);
    PutColour(out, kIdxSocks, socks);

    switch (face) {
    case 1: PutRamp(out, kIdxHair, kGingerHair); break;
    case 2:
        PutRamp(out, kIdxSkin, kDarkSkin);
        PutRamp(out, kIdxHair, kDarkHair);
        break;
    default: break;   // face 0 wears the art's own skin and hair
    }
}

KitChoice ResolveKits(const TeamSheet& a, const TeamSheet& b) {
    KitChoice out;
    out.home = a.primary;
    out.away = b.primary;
    // One rule, applied once: if the shirts collide, the away side changes. Comparing
    // the stripe colour too catches two teams whose "different" kits differ only in a
    // colour the geometry never shows.
    if (out.home.shirt == out.away.shirt) {
        out.away = b.secondary;
        if (out.home.shirt == out.away.shirt) out.home = a.secondary;
    }
    return out;
}

void KitBank::Build(const GamePalette& game, std::span<const uint8_t> ordinals,
                    const TeamSheet& home, const TeamSheet& away) {
    const KitChoice choice = ResolveKits(home, away);
    const KitSpec sides[2] = {choice.home, choice.away};

    for (int s = 0; s < 2; ++s) {
        const size_t si = static_cast<size_t>(s);
        geometry_[si] = GeometryForShirtType(sides[si].shirt_type);
        for (uint8_t f = 0; f < kFaceCount; ++f)
            BuildKitPalette(game, ordinals, sides[si], f, outfield_[si][f]);

        // Keepers wear their own kit. The state carries no keeper colours, so pick a
        // kit colour neither side is wearing — deterministic, and it keeps the two
        // keepers apart from each other as well as from the outfielders.
        KitSpec gk{};
        gk.shirt_type = 0;
        uint8_t pick = 0;
        for (uint8_t c = 0; c < kKitColourCount; ++c) {
            if (c == sides[0].shirt || c == sides[1].shirt) continue;
            if (c == sides[0].stripes || c == sides[1].stripes) continue;
            pick = static_cast<uint8_t>((c + static_cast<uint8_t>(s)) % kKitColourCount);
            if (pick != sides[0].shirt && pick != sides[1].shirt) break;
        }
        gk.shirt = gk.stripes = pick;
        gk.shorts = gk.socks = pick;
        BuildKitPalette(game, ordinals, gk, 0, keeper_[si]);
    }
    ready_ = true;
}

const KitPalette& KitBank::Outfield(int side, uint8_t face) const {
    const size_t s = static_cast<size_t>(side & 1);
    const size_t f = face < kFaceCount ? face : 0;
    return outfield_[s][f];
}

const KitPalette& KitBank::Keeper(int side) const {
    return keeper_[static_cast<size_t>(side & 1)];
}

} // namespace at::render
