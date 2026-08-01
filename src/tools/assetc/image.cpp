#include "image.hpp"

#include <algorithm>
#include <array>
#include <map>

// Keep the decoder's exposed surface as small as it can be: we read PNG and nothing
// else, so every other format's parser is compiled out rather than left reachable by
// a hostile file. STB_IMAGE_STATIC keeps its symbols out of the link.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WRITE
#include "stb_image.h"

namespace at::assetc {

bool LoadPng(const std::filesystem::path& path, Image& out, std::string& err) {
    int w = 0, h = 0, channels = 0;
    // Force four channels so callers never branch on the source's channel count.
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) {
        err = "cannot decode " + path.string() + ": " + stbi_failure_reason();
        return false;
    }
    if (w <= 0 || h <= 0) {
        stbi_image_free(data);
        err = "degenerate image " + path.string();
        return false;
    }

    out.width  = w;
    out.height = h;
    out.pixels.resize(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < out.pixels.size(); ++i) {
        out.pixels[i] = Rgba{data[i * 4 + 0], data[i * 4 + 1],
                             data[i * 4 + 2], data[i * 4 + 3]};
    }
    stbi_image_free(data);
    return true;
}

Downscaled DownscaleMajority(const Image& src, int factor) {
    Downscaled r;
    if (factor <= 0 || src.width % factor != 0 || src.height % factor != 0) return r;

    r.image.width  = src.width / factor;
    r.image.height = src.height / factor;
    r.image.pixels.resize(static_cast<size_t>(r.image.width) * r.image.height);
    r.weakest_majority = factor * factor;

    // std::map rather than unordered_map: the block is tiny, and a defined iteration
    // order makes the tie-break below deterministic across platforms. An importer
    // whose output depends on hash ordering is an importer whose output differs
    // between Windows and macOS, which is exactly what this project cannot have.
    std::map<std::array<uint8_t, 4>, int> counts;

    for (int by = 0; by < r.image.height; ++by) {
        for (int bx = 0; bx < r.image.width; ++bx) {
            counts.clear();
            for (int j = 0; j < factor; ++j) {
                for (int i = 0; i < factor; ++i) {
                    const Rgba& p = src.At(bx * factor + i, by * factor + j);
                    ++counts[{p.r, p.g, p.b, p.a}];
                }
            }
            if (counts.size() > 1) ++r.nonuniform_blocks;

            auto best = counts.begin();
            for (auto it = counts.begin(); it != counts.end(); ++it)
                if (it->second > best->second) best = it;

            r.weakest_majority = std::min(r.weakest_majority, best->second);
            r.image.At(bx, by) = Rgba{best->first[0], best->first[1],
                                      best->first[2], best->first[3]};
        }
    }
    return r;
}

bool Quantise(const Image& src, std::vector<Rgba>& palette,
              std::vector<uint8_t>& out_indices, std::string& err) {
    out_indices.resize(src.pixels.size());
    for (size_t i = 0; i < src.pixels.size(); ++i) {
        const Rgba& p = src.pixels[i];
        auto it = std::find(palette.begin(), palette.end(), p);
        if (it == palette.end()) {
            if (palette.size() >= 256) {
                err = "more than 256 distinct colours; source is not indexed art";
                return false;
            }
            palette.push_back(p);
            it = palette.end() - 1;
        }
        out_indices[i] = static_cast<uint8_t>(it - palette.begin());
    }
    return true;
}

} // namespace at::assetc
