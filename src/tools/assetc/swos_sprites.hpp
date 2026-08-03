#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "image.hpp"

namespace at::assetc {

// Reading the original's own sprite data -- 4 bits per pixel against a 16-colour
// palette, exactly as the game stored it. No resampling, no greyscale flattening, no
// brightening: this is the source A4 section 6.1 says the pipeline needs, and it is
// why the reference's extracted PNG tree is only ever a bootstrap.
//
// Format: doc/SWOS/sprites.txt in the reference port.

// The 24-byte on-disk sprite structure.
struct SwosSpriteHeader {
    uint32_t pixel_offset = 0;   // +0,  relative to its own file
    uint16_t width        = 0;   // +10, exact width in pixels
    uint16_t nlines       = 0;   // +12
    uint16_t wquads       = 0;   // +14, width in paragraphs: pixels/16, bytes/8
    int16_t  centre_x     = 0;   // +16, documented range [-8..34]
    int16_t  centre_y     = 0;   // +18, documented range [0..27]
    uint16_t ordinal      = 0;   // +22, its own index in sprite.dat
};

inline constexpr size_t kSwosSpriteHeaderSize = 24;

struct SwosSprite {
    SwosSpriteHeader     header;
    // One byte per pixel, values 0..15. Palette indices, not colours: the layer split
    // for players is a function of the index, so expanding to RGB here would destroy
    // the very information the kit system needs.
    std::vector<uint8_t> indices;
};

// The three shirt geometries, one per team file. Measured, not inferred: team1/2/3.dat
// differ in all 101 frames, and reading the shirt indices off a front standing frame
// says which is which -- team1 alternates 10/11 across the torso, team2 is uniform per
// row, team3 puts 11 on the sleeve columns. This resolves A4 §6.5, and it also settles
// the other half of that question: the THREE BLOCKS INSIDE one team file are
// byte-identical, so they are the three faces (a palette difference we keep in the
// index domain), not three geometries. We therefore import 101 frames per file, not 303.
enum class ShirtGeometryFile : int {
    kVerticalStripes   = 0,   // team1.dat -- also serves plain, stripes == shirt colour
    kHorizontalStripes = 1,   // team2.dat
    kColouredSleeves   = 2,   // team3.dat
};

inline constexpr int kShirtGeometryCount = 3;
inline constexpr int kPlayerBankFrames   = 101;

struct SwosSpriteSet {
    std::vector<SwosSprite>  sprites;      // by sprite.dat ordinal
    // 101 frames per geometry, indexed by ShirtGeometryFile.
    std::array<std::vector<SwosSprite>, kShirtGeometryCount> geometries{};
    std::array<Rgba, 256>    palette{};    // pal.256, expanded to 8 bits per channel
    std::string              assembly;     // which .dat files formed the joint block
    int                      ordinal_matches = 0;
    int                      ordinal_checked = 0;
    int                      decoded         = 0;
    int                      skipped         = 0;
};

// Load pal.256. The palette lives at offset 0xFA00, 256 entries of three bytes, each
// component 0..63 (VGA 6-bit DAC). Expanded as v<<2 | v>>4 so that 63 maps to 255
// rather than to 252 -- the reference tree's <<2 leaves whites 1.2% dark.
bool LoadSwosPalette(const std::filesystem::path& swos_dir,
                     std::array<Rgba, 256>& out, std::string& err);

// Load every sprite named by sprite.dat.
//
// The joint block that offsets past 227 are relative to is assembled from several
// .dat files, and the documentation is ambiguous about which team files occupy which
// slot. It does not have to be guessed: every sprite header carries its own ordinal
// at +22, so an assembly can be scored by how many headers land where sprite.dat says
// they should. This tries the plausible assemblies and keeps the one that verifies.
bool LoadSwosSprites(const std::filesystem::path& swos_dir, SwosSpriteSet& out,
                     std::string& err);

// Decode one sprite's planar pixels.
//
// A line is split into four bitplanes of wquads*2 bytes each. Each pixel takes one
// bit from each plane; the first pixel takes the high bit of each, and plane 3
// contributes the most significant bit of the resulting nibble. Amiga heritage,
// visible thirty years later.
std::vector<uint8_t> DecodePlanar(const std::vector<uint8_t>& file,
                                  const SwosSpriteHeader& h);

// Render a decoded sprite to RGBA through the palette, treating index 0 as
// transparent. For inspection and for the shape cross-check against the reference
// tree; the pack stores indices, not colours.
Image ToRgba(const SwosSprite& s, const std::array<Rgba, 256>& palette);

} // namespace at::assetc
