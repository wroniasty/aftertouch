#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace at::assetc {

struct PitchReport {
    int      tile_count        = 0;
    int      tile_size         = 0;   // after downscale, expected 16
    int      grid_w            = 0;
    int      grid_h            = 0;
    int      palette_size      = 0;
    uint32_t max_tile_index    = 0;
    size_t   pack_bytes        = 0;

    // Fidelity telemetry. The reference's extracted tree is a resampled upscale, so
    // "did the original pixels survive" is a question with a numeric answer and the
    // importer is obliged to give it rather than to assume yes.
    int total_blocks       = 0;
    int nonuniform_blocks  = 0;   // 0 would mean the upscale was lossless
    int lossless_tiles     = 0;
    int weakest_majority   = 0;   // out of scale*scale; a low value is a plurality vote
};

// Import one pitch directory of the reference's extracted tree:
//
//   pitches/pitchN/pitchN.txt      the tile index matrix, whitespace separated
//   pitches/pitchN/ptN-####.png    the tiles, at 12x
//
// Produces a kPitch pack: entries are tiles as indexed pixels at original 16x16, the
// aux section is the index matrix at 16 bits per cell, and the palette is emitted
// separately by the caller.
bool ImportPitch(const std::filesystem::path& pitch_dir, int pitch_number, int scale,
                 std::vector<uint8_t>& out_pack, std::vector<uint8_t>& out_palette_pack,
                 PitchReport& report, std::string& err);

} // namespace at::assetc
