#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "swos_sprites.hpp"

namespace at::assetc {

// Which tintable part of a player a palette index belongs to.
//
// This is the kit system, and in the original data it is not stored anywhere: it is
// a property of the palette index itself. Index 14 is shorts because index 14 is
// shorts. So the "six layers" the reference port ships as six directories of PNGs
// are not source data at all -- they are a derived view, and we can derive it
// ourselves from the nibbles, exactly and for free.
enum class KitLayer : uint8_t {
    kBackground = 0,   // never recoloured
    kSkin       = 1,   // 4, 5, 6   (dark, medium, bright)
    kHair       = 2,   // 12, 9, 13 (dark, medium, bright)
    kShirt      = 3,   // 10        base colour
    kStripes    = 4,   // 11        swapped with 10 for vertical stripes
    kShorts     = 5,   // 14
    kSocks      = 6,   // 15
};

// Palette index -> layer, for all 16 colours. Emitted alongside the palette so the
// runtime never hardcodes it.
const std::array<uint8_t, 16>& KitLayerTable();

// Kit colour ordinal -> palette index. Ten kit colours mapping NON-CONTIGUOUSLY onto
// the palette (RENDERING.md §5); indices 4, 5, 7, 8, 9 are skipped because they are
// skin and hair. This is the only bridge between DATA.md's colour numbers and RGB, so
// like the layer table it travels in the palette pack rather than living in renderer
// code where A5 cannot reach it.
const std::array<uint8_t, 10>& KitColourOrdinals();

// palette.atl aux layout: the layer table, then the kit ordinals.
inline constexpr int kPaletteAuxLayerBytes   = 16;
inline constexpr int kPaletteAuxOrdinalBytes = 10;
inline constexpr int kPaletteAuxBytes = kPaletteAuxLayerBytes + kPaletteAuxOrdinalBytes;

struct SwosImportReport {
    int packs        = 0;
    int sprites      = 0;
    size_t bytes     = 0;
    // Per-layer pixel counts across the player bank, so a routing mistake is visible
    // as a layer that is empty or absurdly large rather than as odd-looking kits.
    int layer_pixels[7] = {};
};

// Write our runtime packs from a loaded original installation.
bool ImportSwosSprites(const SwosSpriteSet& set, const std::filesystem::path& out_dir,
                       SwosImportReport& report, std::string& err);

} // namespace at::assetc
