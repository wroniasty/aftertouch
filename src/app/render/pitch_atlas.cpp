#include "render/pitch_atlas.hpp"

#include <SDL3/SDL.h>

#include <vector>

namespace at::render {

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

} // namespace at::render
