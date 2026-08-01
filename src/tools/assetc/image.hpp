#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace at::assetc {

struct Rgba {
    uint8_t r = 0, g = 0, b = 0, a = 0;
    friend bool operator==(const Rgba&, const Rgba&) = default;
};

struct Image {
    int               width  = 0;
    int               height = 0;
    std::vector<Rgba> pixels;

    const Rgba& At(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
    Rgba&       At(int x, int y)       { return pixels[static_cast<size_t>(y) * width + x]; }
};

// PNG decode. The only place third-party binary data enters this tool.
bool LoadPng(const std::filesystem::path& path, Image& out, std::string& err);

struct Downscaled {
    Image image;
    // Blocks that were not a single flat colour. Zero means the upscale was lossless
    // and the original pixels are recovered by construction rather than by vote.
    int   nonuniform_blocks = 0;
    // Worst block seen, as the majority colour's share of its block. 1.0 would be
    // unanimous; the importer reports the minimum so a marginal vote is visible.
    int   weakest_majority  = 0;   // count out of factor*factor
};

// Undo an integer upscale by taking each block's majority colour.
//
// A3/A4's whole reason for existing is comparison against the reference, and the
// reference's extracted tree is a 12x upscale that is exact for pitch tiles and very
// nearly exact for sprites (A4 section 2.2). Majority vote recovers the original in
// both cases; the counters above are what let the caller tell which case it was in
// rather than assuming.
Downscaled DownscaleMajority(const Image& src, int factor);

// Build an indexed image against a palette shared across several images, growing the
// palette as new colours appear. Fails past 256 colours rather than dithering: the
// source art is 16-colour and anything beyond that means we imported the wrong thing.
bool Quantise(const Image& src, std::vector<Rgba>& palette,
              std::vector<uint8_t>& out_indices, std::string& err);

} // namespace at::assetc
