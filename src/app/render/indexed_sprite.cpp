#include "render/indexed_sprite.hpp"

#include <SDL3/SDL.h>

#include <unordered_map>
#include <vector>

namespace at::render {

namespace {

struct TexKey {
    const uint8_t* px      = nullptr;
    const uint8_t* palette = nullptr;
    uint16_t       w       = 0;
    uint16_t       h       = 0;
    bool operator==(const TexKey& o) const {
        return px == o.px && palette == o.palette && w == o.w && h == o.h;
    }
};

struct TexKeyHash {
    size_t operator()(const TexKey& k) const {
        size_t h = reinterpret_cast<size_t>(k.px);
        h ^= reinterpret_cast<size_t>(k.palette) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= (static_cast<size_t>(k.w) << 16) ^ k.h;
        return h;
    }
};

std::unordered_map<TexKey, SDL_Texture*, TexKeyHash>& SpriteTexCache() {
    static std::unordered_map<TexKey, SDL_Texture*, TexKeyHash> cache;
    return cache;
}

} // namespace

void DrawIndexedSprite(SDL_Renderer* r, const SpriteSheet& sheet,
                       std::span<const uint8_t> palette_rgba, uint32_t palette_count,
                       float screen_x, float screen_y, float scale) {
    if (!r || sheet.width == 0 || sheet.height == 0 || sheet.pixels.empty()) return;
    if (scale <= 0.f) scale = 1.f;

    TexKey key{sheet.pixels.data(), palette_rgba.data(), sheet.width, sheet.height};
    auto& cache = SpriteTexCache();
    SDL_Texture* tex = nullptr;
    if (auto it = cache.find(key); it != cache.end()) {
        tex = it->second;
    } else {
        std::vector<uint8_t> rgba(static_cast<size_t>(sheet.width) *
                                  static_cast<size_t>(sheet.height) * 4);
        if (!ExpandIndexed(sheet.pixels, sheet.width, sheet.height, palette_rgba,
                           palette_count, rgba))
            return;
        // Sprite key colour: index 0 is transparent regardless of palette RGB.
        const size_t n = static_cast<size_t>(sheet.width) * sheet.height;
        for (size_t i = 0; i < n; ++i) {
            if (sheet.pixels[i] == 0) {
                rgba[i * 4 + 0] = 0;
                rgba[i * 4 + 1] = 0;
                rgba[i * 4 + 2] = 0;
                rgba[i * 4 + 3] = 0;
            }
        }

        SDL_Surface* surf = SDL_CreateSurfaceFrom(
            sheet.width, sheet.height, SDL_PIXELFORMAT_RGBA32, rgba.data(),
            sheet.width * 4);
        if (!surf) return;
        tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_DestroySurface(surf);
        if (!tex) return;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        cache.emplace(key, tex);
    }

    const float dw = static_cast<float>(sheet.width) * scale;
    const float dh = static_cast<float>(sheet.height) * scale;
    const float ax = static_cast<float>(sheet.anchor_x) * scale;
    const float ay = static_cast<float>(sheet.anchor_y) * scale;
    SDL_FRect dst{screen_x - ax, screen_y - ay, dw, dh};
    SDL_RenderTexture(r, tex, nullptr, &dst);
}

void InvalidateIndexedSpriteCache() {
    auto& cache = SpriteTexCache();
    for (auto& [key, tex] : cache) {
        (void)key;
        if (tex) SDL_DestroyTexture(tex);
    }
    cache.clear();
}

} // namespace at::render
