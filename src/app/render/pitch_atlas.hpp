#pragma once

#include "render/asset_source.hpp"
#include "render/pitch_view.hpp"

#include <span>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace at::render {

// GPU cache of pitch tiles expanded from indexed ATAP pixels + palette.
// SDL lives here only — at_asset_source stays free of it.
class PitchAtlas {
public:
    PitchAtlas() = default;
    ~PitchAtlas();

    PitchAtlas(const PitchAtlas&)            = delete;
    PitchAtlas& operator=(const PitchAtlas&) = delete;

    // Build or rebuild when `tiles` identity / fingerprint changes.
    bool Ensure(SDL_Renderer* r, const PitchTiles* tiles);

    void Draw(SDL_Renderer* r, const DebugView& view, int match_w,
              int match_h) const;

    void Clear();

    bool Ready() const { return !textures_.empty(); }

private:
    const PitchTiles*              src_ = nullptr;
    std::vector<SDL_Texture*>      textures_;
    uint16_t                       tile_w_ = 0;
    uint16_t                       tile_h_ = 0;
};

// Expand a sprite sheet through a palette (or default ramp) and draw centred
// on (screen_x, screen_y) at `scale` (nearest). Index 0 is always transparent
// (SWOS sprite convention; palette RGB for 0 is often opaque green).
// Textures are cached by pixel pointer for the process lifetime.
void DrawIndexedSprite(SDL_Renderer* r, const SpriteSheet& sheet,
                       std::span<const uint8_t> palette_rgba, uint32_t palette_count,
                       float screen_x, float screen_y, float scale);

} // namespace at::render
