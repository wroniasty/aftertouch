#include "render/placeholder_assets.hpp"
#include "render/pack_bank.hpp"

#include <array>
#include <vector>

namespace at {

struct PlaceholderAssets::Impl {
    render::LoadedPack slot_a;
    render::LoadedPack slot_b;
    render::LoadedPack ball;
    render::LoadedPack pitch;

    std::vector<SpriteSheet> sheets_a;
    std::vector<SpriteSheet> sheets_b;
    SpriteSheet              ball_sheet{};
    PitchTiles               pitch_tiles{};
    bool                     pitch_ok = false;
};

PlaceholderAssets::PlaceholderAssets() = default;
PlaceholderAssets::~PlaceholderAssets() = default;

static bool BuildSheets(const render::LoadedPack& pack, std::vector<SpriteSheet>& out) {
    if (!pack.ok) return false;
    out.resize(pack.header.entry_count);
    for (uint32_t i = 0; i < pack.header.entry_count; ++i) {
        if (!render::FillSheet(pack, i, out[i])) return false;
    }
    return true;
}

std::unique_ptr<PlaceholderAssets> PlaceholderAssets::Open(const char* dir) {
    if (!dir || !*dir) return nullptr;
    auto src = std::unique_ptr<PlaceholderAssets>(new PlaceholderAssets());
    src->impl_ = std::make_unique<Impl>();
    auto& im = *src->impl_;

    const std::string root = dir;
    if (!render::LoadPackFile(root + "/slotA_blk0.atp", im.slot_a)) return nullptr;
    if (!render::LoadPackFile(root + "/slotB_blk0.atp", im.slot_b)) return nullptr;
    if (!render::LoadPackFile(root + "/ball.atp", im.ball)) return nullptr;
    if (!render::LoadPackFile(root + "/pitch1.atp", im.pitch)) return nullptr;

    if (!BuildSheets(im.slot_a, im.sheets_a)) return nullptr;
    if (!BuildSheets(im.slot_b, im.sheets_b)) return nullptr;
    if (!render::FillSheet(im.ball, 0, im.ball_sheet)) return nullptr;
    im.pitch_ok = render::BindPitch(im.pitch, im.pitch_tiles);

    // Dimensional contract (A4 §2.5).
    if (static_cast<int>(im.sheets_a.size()) != kPlayerFrameCount) return nullptr;
    if (static_cast<int>(im.sheets_b.size()) != kPlayerFrameCount) return nullptr;
    for (const auto& s : im.sheets_a) {
        if (s.width != kPlayerSpriteW || s.height != kPlayerSpriteH) return nullptr;
    }
    if (!im.pitch_ok) return nullptr;
    if (im.pitch_tiles.tile_w != kPitchTileSize || im.pitch_tiles.tile_h != kPitchTileSize)
        return nullptr;
    if (im.pitch_tiles.grid_w != kPitchGridW || im.pitch_tiles.grid_h != kPitchGridH)
        return nullptr;

    return src;
}

const SpriteSheet* PlaceholderAssets::PlayerSheet(TeamSlot slot, int frame) const {
    if (frame < 0 || frame >= kPlayerFrameCount) return nullptr;
    const auto& sheets = (slot == TeamSlot::A) ? impl_->sheets_a : impl_->sheets_b;
    return &sheets[static_cast<size_t>(frame)];
}

const SpriteSheet* PlaceholderAssets::Player(TeamSlot slot, Dir, int frame) const {
    return PlayerSheet(slot, frame);
}

const SpriteSheet* PlaceholderAssets::Ball() const {
    return &impl_->ball_sheet;
}

const PitchTiles* PlaceholderAssets::Pitch(PitchType) const {
    // Placeholder ships one pitch; every PitchType maps to it until C1 needs more.
    return impl_->pitch_ok ? &impl_->pitch_tiles : nullptr;
}

int PlaceholderAssets::PlayerFrames() const {
    return static_cast<int>(impl_->sheets_a.size());
}

} // namespace at
