#pragma once
#include "core/match_input.hpp"

#include <cstdint>
#include <memory>
#include <span>

// Runtime asset serving — A4 §2.5. No SDL types in this interface.
// See doc/implementation/A4-asset-pipeline.md.

namespace at {

// Playing side → which team-file slot's sprite bank to read.
enum class TeamSlot : uint8_t { A = 0, B = 1 };

// Surface type ordinals from doc/PITCH.md. Pitch file selection is separate;
// Pitch() maps these onto pitchN packs when present.
enum class PitchType : uint8_t {
    Frozen = 0,
    Muddy  = 1,
    Wet    = 2,
    Soft   = 3,
    Normal = 4,
    Dry    = 5,
    Hard   = 6,
};

inline constexpr int kPlayerFrameCount = 101;
inline constexpr int kPlayerSpriteW    = 12;
inline constexpr int kPlayerSpriteH    = 15;
inline constexpr int kPitchTileSize    = 16;
inline constexpr int kPitchGridW       = 42;
inline constexpr int kPitchGridH       = 55; // matrix as stored; world uses 53 (A4 §6.2)

struct SpriteSheet {
    uint16_t                  width    = 0;
    uint16_t                  height   = 0;
    int16_t                   anchor_x = 0;
    int16_t                   anchor_y = 0;
    std::span<const uint8_t>  pixels; // indexed, width*height
};

struct PitchTiles {
    uint16_t                  tile_w   = 0;
    uint16_t                  tile_h   = 0;
    uint32_t                  tile_count = 0;
    uint16_t                  grid_w   = 0;
    uint16_t                  grid_h   = 0;
    // tile_count entries via Tile(i); grid is row-major u16 indices in `indices`.
    std::span<const uint8_t>  indices; // grid_w * grid_h * 2 LE u16s
    // Opaque handle for Tile() — implementations fill these.
    const void*               _pack = nullptr;
    uint32_t (*_tile_fn)(const PitchTiles*, uint32_t, SpriteSheet*) = nullptr;

    bool Tile(uint32_t i, SpriteSheet* out) const {
        if (!_tile_fn || !out) return false;
        return _tile_fn(this, i, out) != 0;
    }
};

class IAssetSource {
public:
    virtual ~IAssetSource() = default;

    // frame is 0 .. kPlayerFrameCount-1 within the slot's primary bank (blk0).
    // Dir is accepted for C3 facing; indexing today is by frame only.
    virtual const SpriteSheet* Player(TeamSlot slot, Dir dir, int frame) const = 0;
    virtual const SpriteSheet* Ball() const = 0;
    virtual const PitchTiles*  Pitch(PitchType type) const = 0;
    virtual bool               IsPlaceholder() const = 0;
};

// Prefer assets/generated when a valid manifest (or complete pack set) is present;
// otherwise placeholder. Never fails to return a source when placeholder_dir is valid.
std::unique_ptr<IAssetSource> OpenAssetSource(const char* generated_dir,
                                              const char* placeholder_dir);

} // namespace at
