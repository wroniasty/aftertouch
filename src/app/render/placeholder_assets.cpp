#include "render/placeholder_assets.hpp"
#include "render/pack_bank.hpp"

#include <array>
#include <vector>

namespace at {

struct PlaceholderAssets::Impl {
    std::array<render::LoadedPack, kShirtGeometryCount> kits;
    render::LoadedPack keepers;
    render::LoadedPack ball;
    render::LoadedPack numbers;
    render::LoadedPack pitch;

    std::array<std::vector<SpriteSheet>, kShirtGeometryCount> kit_sheets;
    std::vector<SpriteSheet> keeper_sheets;
    std::vector<SpriteSheet> ball_sheets;
    SpriteSheet              ball_shadow{};
    std::vector<SpriteSheet> number_sheets;
    PitchTiles               pitch_tiles{};
    bool                     pitch_ok = false;
    bool                     has_shadow = false;
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
    static const char* kKitFiles[kShirtGeometryCount] = {
        "/kit_vstripe.atp", "/kit_hstripe.atp", "/kit_sleeves.atp"};
    for (int g = 0; g < kShirtGeometryCount; ++g) {
        if (!render::LoadPackFile(root + kKitFiles[g], im.kits[static_cast<size_t>(g)]))
            return nullptr;
    }
    if (!render::LoadPackFile(root + "/keepers.atp", im.keepers)) return nullptr;
    if (!render::LoadPackFile(root + "/ball.atp", im.ball)) return nullptr;
    if (!render::LoadPackFile(root + "/numbers.atp", im.numbers)) return nullptr;
    if (!render::LoadPackFile(root + "/pitch1.atp", im.pitch)) return nullptr;

    for (int g = 0; g < kShirtGeometryCount; ++g) {
        if (!BuildSheets(im.kits[static_cast<size_t>(g)],
                         im.kit_sheets[static_cast<size_t>(g)]))
            return nullptr;
    }
    if (!BuildSheets(im.keepers, im.keeper_sheets)) return nullptr;
    if (!BuildSheets(im.numbers, im.number_sheets)) return nullptr;

    std::vector<SpriteSheet> ball_all;
    if (!BuildSheets(im.ball, ball_all)) return nullptr;
    if (static_cast<int>(ball_all.size()) != kBallFrameCount + 1) return nullptr;
    im.ball_sheets.assign(ball_all.begin(), ball_all.begin() + kBallFrameCount);
    im.ball_shadow = ball_all[kBallFrameCount];
    im.has_shadow  = true;

    im.pitch_ok = render::BindPitch(im.pitch, im.pitch_tiles);

    // Dimensional contract (A4 §2.5) — the placeholder path exists to prove the
    // interface, so it must fail loudly rather than serve a differently shaped bank.
    for (const auto& bank : im.kit_sheets) {
        if (static_cast<int>(bank.size()) != kPlayerFrameCount) return nullptr;
        for (const auto& s : bank) {
            if (s.width != kPlayerSpriteW || s.height != kPlayerSpriteH) return nullptr;
        }
    }
    if (static_cast<int>(im.keeper_sheets.size()) != kKeeperFrameCount) return nullptr;
    if (static_cast<int>(im.number_sheets.size()) != kMaxShirtNumber) return nullptr;
    if (!im.pitch_ok) return nullptr;
    if (im.pitch_tiles.tile_w != kPitchTileSize || im.pitch_tiles.tile_h != kPitchTileSize)
        return nullptr;
    if (im.pitch_tiles.grid_w != kPitchGridW || im.pitch_tiles.grid_h != kPitchGridH)
        return nullptr;

    return src;
}

const SpriteSheet* PlaceholderAssets::Player(ShirtGeometry geo, int frame) const {
    const size_t g = static_cast<size_t>(geo);
    if (g >= kShirtGeometryCount) return nullptr;
    if (frame < 0 || frame >= kPlayerFrameCount) return nullptr;
    return &impl_->kit_sheets[g][static_cast<size_t>(frame)];
}

const SpriteSheet* PlaceholderAssets::Keeper(int frame) const {
    if (frame < 0 || frame >= static_cast<int>(impl_->keeper_sheets.size()))
        return nullptr;
    return &impl_->keeper_sheets[static_cast<size_t>(frame)];
}

const SpriteSheet* PlaceholderAssets::Ball(int frame) const {
    if (impl_->ball_sheets.empty()) return nullptr;
    if (frame < 0) frame = 0;
    return &impl_->ball_sheets[static_cast<size_t>(frame) % impl_->ball_sheets.size()];
}

const SpriteSheet* PlaceholderAssets::BallShadow() const {
    return impl_->has_shadow ? &impl_->ball_shadow : nullptr;
}

const SpriteSheet* PlaceholderAssets::Number(int shirt_number) const {
    const int i = shirt_number - 1;
    if (i < 0 || i >= static_cast<int>(impl_->number_sheets.size())) return nullptr;
    return &impl_->number_sheets[static_cast<size_t>(i)];
}

const PitchTiles* PlaceholderAssets::Pitch(PitchType) const {
    // Placeholder ships one pitch; every PitchType maps to it until C1 needs more.
    return impl_->pitch_ok ? &impl_->pitch_tiles : nullptr;
}

std::span<const uint8_t> PlaceholderAssets::KitColourOrdinals() const {
    // No original palette here, so no ordinal table: the kit builder falls back to
    // RENDERING.md §5's values and tints the placeholder's flat rectangles anyway.
    return {};
}

int PlaceholderAssets::PlayerFrames() const {
    return static_cast<int>(impl_->kit_sheets[0].size());
}

} // namespace at
