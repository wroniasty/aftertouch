#pragma once

#include "render/asset_source.hpp"

#include <span>

struct SDL_Renderer;

namespace at::render {

// Expand a sprite sheet through a palette (or default ramp) and draw centred
// on (screen_x, screen_y) at `scale` (nearest). Index 0 is always transparent
// (SWOS sprite convention; palette RGB for 0 is often opaque green).
// Textures are cached by (pixel pointer, palette pointer) for the process
// lifetime, which is what makes one kit bake enough for a whole match.
//
// Lives in its own translation unit rather than beside PitchAtlas because the
// sprite_viewer tool needs exactly this and nothing else: the index-0 key
// colour and the anchor convention are rules that must not fork between the
// game and the tool used to inspect the art.
void DrawIndexedSprite(SDL_Renderer* r, const SpriteSheet& sheet,
                       std::span<const uint8_t> palette_rgba, uint32_t palette_count,
                       float screen_x, float screen_y, float scale);

// Drop every cached texture. The cache keys on the palette's *address*, which is
// exactly right for the game -- kits are baked once per match and never touched
// again -- and exactly wrong for anything that edits a palette in place, because
// the key does not change when the colours do. Call this after rebuilding a
// palette buffer you are going to keep drawing through. The shell never needs
// it; sprite_viewer calls it on every kit edit.
void InvalidateIndexedSpriteCache();

} // namespace at::render
