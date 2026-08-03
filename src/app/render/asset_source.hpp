#pragma once
#include "core/match_input.hpp"

#include <cstdint>
#include <memory>
#include <span>

// Runtime asset serving — A4 §2.5. No SDL types in this interface.
// See doc/implementation/A4-asset-pipeline.md.

namespace at {

// Shirt geometry → which 101-frame player bank to read. The geometry is the original's
// team FILE, not a block within one (C3 §1 Finding 2); the three blocks inside a file
// are the three faces and are byte-identical in the index domain, so faces are a
// palette matter and never a bank.
enum class ShirtGeometry : uint8_t {
    VerticalStripes   = 0,   // also plain: stripes colour == shirt colour
    HorizontalStripes = 1,
    ColouredSleeves   = 2,
};

inline constexpr int kShirtGeometryCount = 3;

// DATA.md ShirtType → geometry. Plain and vertical stripes share art.
inline ShirtGeometry GeometryForShirtType(uint8_t shirt_type) {
    switch (shirt_type) {
    case 1:  return ShirtGeometry::ColouredSleeves;
    case 3:  return ShirtGeometry::HorizontalStripes;
    default: return ShirtGeometry::VerticalStripes;   // 0 plain, 2 vertical stripes
    }
}

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
inline constexpr int kKeeperFrameCount = 58;
inline constexpr int kBallFrameCount   = 4;    // rotation; the shadow is separate
inline constexpr int kMaxShirtNumber   = 16;
inline constexpr int kKitColourCount   = 10;
inline constexpr int kPitchTileSize    = 16;
inline constexpr int kPitchGridW       = 42;
inline constexpr int kPitchGridH       = 55;
inline constexpr int kPitchWorldCols   = 42;
// 55, not 53. The original's own matrix carries content in every row -- rows 0 and 54
// are the stands, not padding -- and drawing them 1:1 as world rows puts the engine's
// pitch constants exactly on the painted markings (C3 §1 Finding 3). CAMERA.md §9
// agrees independently: its hard camera limit kCameraMaxY = 680 is 880 - 200, which
// only makes sense in an 880-tall world.
//
// The reference tree's matrix is a different artefact and DOES have a leading pad row,
// so the mapping is chosen per pack from its SourceKind rather than globally.
inline constexpr int kPitchWorldRows   = 55; // 42×55×16 = 672×880
inline constexpr int kPitchLegacyRows  = 53; // ref-tree / placeholder content rows
inline constexpr int kPitchWorldW      = kPitchWorldCols * kPitchTileSize;
inline constexpr int kPitchWorldH      = kPitchWorldRows * kPitchTileSize;
inline constexpr uint16_t kPitchEmptyCell = 0xFFFFu;

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
    // Optional RGBA palette (count × 4). Empty → caller uses a default ramp.
    std::span<const uint8_t>  palette_rgba;
    uint32_t                  palette_count = 0;
    // True when the pack came from an original installation. Decides the row mapping:
    // the original's matrix is 1:1 with world rows, the reference tree's is pad-shifted.
    bool                      from_original = false;
    // Opaque handle for Tile() — implementations fill these.
    const void*               _pack = nullptr;
    uint32_t (*_tile_fn)(const PitchTiles*, uint32_t, SpriteSheet*) = nullptr;

    bool Tile(uint32_t i, SpriteSheet* out) const {
        if (!_tile_fn || !out) return false;
        return _tile_fn(this, i, out) != 0;
    }
};

// Pure helpers (C1) — no SDL.
inline uint16_t PitchGridIndex(const PitchTiles& pt, int col, int row) {
    if (col < 0 || row < 0) return kPitchEmptyCell;
    if (col >= static_cast<int>(pt.grid_w) || row >= static_cast<int>(pt.grid_h))
        return kPitchEmptyCell;
    const size_t at = (static_cast<size_t>(row) * pt.grid_w + static_cast<size_t>(col)) * 2;
    if (at + 1 >= pt.indices.size()) return kPitchEmptyCell;
    return static_cast<uint16_t>(pt.indices[at] | (uint16_t(pt.indices[at + 1]) << 8));
}

// Drawable world rows. An original pack draws every matrix row; a reference-tree pack
// is pad + 53 content + pad, so it draws 53 starting one row in.
inline int PitchDrawableRows(const PitchTiles& pt) {
    const int h = static_cast<int>(pt.grid_h);
    const int cap = pt.from_original ? kPitchWorldRows : kPitchLegacyRows;
    return h < cap ? h : cap;
}

inline int PitchMatrixRow(const PitchTiles& pt, int world_row) {
    if (world_row < 0) return world_row;
    if (pt.from_original) return world_row;
    // Leading pad row when a legacy matrix is content+2 (55 = 53 + 2).
    if (static_cast<int>(pt.grid_h) == kPitchLegacyRows + 2) return world_row + 1;
    return world_row;
}

// Expand indexed pixels through an RGBA palette into out (w*h*4).
// Missing palette entries become magenta; index 0 with a==0 stays transparent.
inline bool ExpandIndexed(std::span<const uint8_t> indexed, int w, int h,
                          std::span<const uint8_t> palette_rgba, uint32_t palette_count,
                          std::span<uint8_t> out_rgba) {
    if (w <= 0 || h <= 0) return false;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (indexed.size() < n || out_rgba.size() < n * 4) return false;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t idx = indexed[i];
        uint8_t r = 255, g = 0, b = 255, a = 255;
        if (palette_count > 0 && idx < palette_count) {
            const size_t p = static_cast<size_t>(idx) * 4;
            if (p + 3 < palette_rgba.size()) {
                r = palette_rgba[p + 0];
                g = palette_rgba[p + 1];
                b = palette_rgba[p + 2];
                a = palette_rgba[p + 3];
            }
        } else if (palette_count == 0) {
            // Placeholder ramp: dark green field tones.
            r = static_cast<uint8_t>(20 + (idx % 6) * 8);
            g = static_cast<uint8_t>(70 + (idx % 6) * 18);
            b = static_cast<uint8_t>(30 + (idx % 6) * 6);
            a = (idx == 0) ? 0 : 255;
        }
        out_rgba[i * 4 + 0] = r;
        out_rgba[i * 4 + 1] = g;
        out_rgba[i * 4 + 2] = b;
        out_rgba[i * 4 + 3] = a;
    }
    return true;
}

struct GamePalette {
    std::span<const uint8_t> rgba; // count × 4
    uint32_t                 count = 0;
};

class IAssetSource {
public:
    virtual ~IAssetSource() = default;

    // frame is 0 .. kPlayerFrameCount-1; the animation stepper produces it.
    virtual const SpriteSheet* Player(ShirtGeometry geo, int frame) const = 0;
    virtual const SpriteSheet* Keeper(int frame) const = 0;
    // Ball rotation frame 0 .. kBallFrameCount-1, and its separate ground shadow.
    virtual const SpriteSheet* Ball(int frame) const = 0;
    virtual const SpriteSheet* BallShadow() const = 0;
    // Shirt numbers 1 .. kMaxShirtNumber (the marker above the controlled player).
    virtual const SpriteSheet* Number(int shirt_number) const = 0;
    virtual const PitchTiles*  Pitch(PitchType type) const = 0;
    // Game / kit palette (palette.atl). Empty → ExpandIndexed default ramp.
    virtual GamePalette        Palette() const { return {}; }
    // Ten kit-colour ordinals → palette indices (RENDERING.md §5). Empty → caller's
    // fallback; never hardcode the table in renderer code when the pack carries it.
    virtual std::span<const uint8_t> KitColourOrdinals() const { return {}; }
    virtual bool               IsPlaceholder() const = 0;
};

// Prefer assets/generated when a valid manifest (or complete pack set) is present;
// otherwise placeholder. Never fails to return a source when placeholder_dir is valid.
std::unique_ptr<IAssetSource> OpenAssetSource(const char* generated_dir,
                                              const char* placeholder_dir);

} // namespace at
