#pragma once
#include "render/asset_source.hpp"

#include "core/match_state.hpp"

#include <array>
#include <cstdint>
#include <span>

// C3 §3.2 — a kit is a palette, not a composite.
//
// The reference port tints six grey-scale layers and flattens them into a texture set
// per face per team, because SDL colour-mod needs separate surfaces to blit. Our art
// never left the index domain (A4 §2.3), so the whole kit system is a 256-entry lookup:
// indices 10/11/14/15 carry shirt, stripes, shorts and socks, 4/5/6 and 9/12/13 carry
// the skin and hair ramps, and everything else is untouched background.
//
// Pure — no SDL, no file I/O — so it unit-tests without a window.

namespace at::render {

inline constexpr uint32_t kPaletteEntries = 256;

// Where each tintable region lives in the 16-colour sprite palette. Mirrors assetc's
// KitLayerTable; the runtime prefers the ordinals shipped in palette.atl but the
// index→region mapping is a property of the art itself and does not vary.
inline constexpr uint8_t kIdxShirt   = 10;
inline constexpr uint8_t kIdxStripes = 11;
inline constexpr uint8_t kIdxShorts  = 14;
inline constexpr uint8_t kIdxSocks   = 15;
inline constexpr std::array<uint8_t, 3> kIdxSkin = {4, 5, 6};    // dark, medium, bright
inline constexpr std::array<uint8_t, 3> kIdxHair = {12, 9, 13};  // dark, medium, bright

// RENDERING.md §5. Used only when the pack ships no ordinal table (placeholder path).
inline constexpr std::array<uint8_t, kKitColourCount> kFallbackKitOrdinals = {
    1, 2, 3, 6, 10, 11, 12, 13, 14, 15};

inline constexpr int kFaceCount = 3;   // white, ginger, black (RENDERING.md §5)

struct Rgba8 {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

// One ready-to-draw palette: RGBA bytes in the layout DrawIndexedSprite expects.
// The buffer must outlive every texture drawn through it — the sprite texture cache
// keys on its address, which is exactly what makes one bake per kit enough.
struct KitPalette {
    std::array<uint8_t, kPaletteEntries * 4> rgba{};

    std::span<const uint8_t> Bytes() const { return rgba; }
};

// Build a kit palette over `game` (the 256-entry game palette from palette.atl).
// `ordinals` is the ten kit-colour → palette-index table; pass an empty span to use
// kFallbackKitOrdinals. `face` selects the skin and hair ramps (0..kFaceCount-1).
void BuildKitPalette(const GamePalette& game, std::span<const uint8_t> ordinals,
                     const KitSpec& kit, uint8_t face, KitPalette& out);

// The four kit colours a side actually wears, resolved for clashes. Two teams whose
// shirt colours collide is the one case the original solves by switching to the away
// kit, and it has to be decided once at kickoff rather than per frame.
struct KitChoice {
    KitSpec home{};
    KitSpec away{};
};
KitChoice ResolveKits(const TeamSheet& a, const TeamSheet& b);

// A match's bake: two sides × three faces, plus one keeper palette per side.
// Rebuilt only when the sheets change.
class KitBank {
public:
    // side is 0 (home) or 1 (away); face is 0..2.
    void Build(const GamePalette& game, std::span<const uint8_t> ordinals,
               const TeamSheet& home, const TeamSheet& away);

    bool Ready() const { return ready_; }

    const KitPalette& Outfield(int side, uint8_t face) const;
    const KitPalette& Keeper(int side) const;
    ShirtGeometry     Geometry(int side) const {
        return geometry_[static_cast<size_t>(side & 1)];
    }

private:
    std::array<std::array<KitPalette, kFaceCount>, 2> outfield_{};
    std::array<KitPalette, 2>                         keeper_{};
    std::array<ShirtGeometry, 2>                      geometry_{};
    bool                                              ready_ = false;
};

} // namespace at::render
