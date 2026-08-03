#include "render/pitch_atlas.hpp"

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

PitchAtlas::~PitchAtlas() { Clear(); }

void PitchAtlas::Clear() {
    for (SDL_Texture* t : textures_) {
        if (t) SDL_DestroyTexture(t);
    }
    textures_.clear();
    src_    = nullptr;
    tile_w_ = 0;
    tile_h_ = 0;
}

bool PitchAtlas::Ensure(SDL_Renderer* r, const PitchTiles* tiles) {
    if (!r || !tiles || tiles->tile_count == 0) {
        Clear();
        return false;
    }
    if (src_ == tiles && Ready()) return true;

    Clear();
    src_    = tiles;
    tile_w_ = tiles->tile_w;
    tile_h_ = tiles->tile_h;
    textures_.assign(tiles->tile_count, nullptr);

    std::vector<uint8_t> rgba(
        static_cast<size_t>(tile_w_) * static_cast<size_t>(tile_h_) * 4);

    for (uint32_t i = 0; i < tiles->tile_count; ++i) {
        SpriteSheet sheet{};
        if (!tiles->Tile(i, &sheet)) continue;
        if (!ExpandIndexed(sheet.pixels, tile_w_, tile_h_, tiles->palette_rgba,
                           tiles->palette_count, rgba))
            continue;

        SDL_Surface* surf = SDL_CreateSurfaceFrom(
            tile_w_, tile_h_, SDL_PIXELFORMAT_RGBA32, rgba.data(),
            tile_w_ * 4);
        if (!surf) continue;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_DestroySurface(surf);
        if (!tex) continue;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        textures_[i] = tex;
    }
    return Ready();
}

void PitchAtlas::Draw(SDL_Renderer* r, const DebugView& view, int match_w,
                      int match_h) const {
    if (!r || !src_ || textures_.empty()) return;

    const float scale = PitchUniformScale(view, match_w, match_h);
    if (scale <= 0.f) return;

    const int rows = PitchDrawableRows(*src_);
    const int cols = static_cast<int>(src_->grid_w);
    if (rows <= 0 || cols <= 0) return;

    int col0 = static_cast<int>(view.min_x) / kPitchTileSize;
    int row0 = static_cast<int>(view.min_y) / kPitchTileSize;
    int col1 = (static_cast<int>(view.max_x) + kPitchTileSize - 1) / kPitchTileSize;
    int row1 = (static_cast<int>(view.max_y) + kPitchTileSize - 1) / kPitchTileSize;
    if (col0 < 0) col0 = 0;
    if (row0 < 0) row0 = 0;
    if (col1 > cols) col1 = cols;
    if (row1 > rows) row1 = rows;

    (void)scale;

    for (int row = row0; row < row1; ++row) {
        for (int col = col0; col < col1; ++col) {
            const uint16_t idx =
                PitchGridIndex(*src_, col, PitchMatrixRow(*src_, row));
            if (idx == kPitchEmptyCell || idx >= textures_.size()) continue;
            SDL_Texture* tex = textures_[idx];
            if (!tex) continue;

            const int16_t wx0 = static_cast<int16_t>(col * kPitchTileSize);
            const int16_t wy0 = static_cast<int16_t>(row * kPitchTileSize);
            const int16_t wx1 = static_cast<int16_t>(wx0 + kPitchTileSize);
            const int16_t wy1 = static_cast<int16_t>(wy0 + kPitchTileSize);
            const ScreenPosF a = PitchToScreenF(wx0, wy0, view, match_w, match_h);
            const ScreenPosF b = PitchToScreenF(wx1, wy1, view, match_w, match_h);

            SDL_FRect dst{a.x, a.y, b.x - a.x, b.y - a.y};
            dst.w += 0.5f;
            dst.h += 0.5f;
            SDL_RenderTexture(r, tex, nullptr, &dst);
        }
    }
}

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

} // namespace at::render
