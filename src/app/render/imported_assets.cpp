#include "render/imported_assets.hpp"
#include "render/pack_bank.hpp"

#include <array>
#include <vector>

namespace at {

namespace {
// palette.atl aux: 16 layer bytes then 10 kit-colour ordinals (assetc swos_import.hpp).
constexpr size_t kAuxLayerBytes   = 16;
constexpr size_t kAuxOrdinalBytes = 10;
} // namespace

struct ImportedAssets::Impl {
    std::array<render::LoadedPack, kShirtGeometryCount> kits;
    render::LoadedPack keepers;
    render::LoadedPack ball;
    render::LoadedPack numbers;
    render::LoadedPack pitch;
    render::LoadedPack pitch_pal;
    render::LoadedPack game_pal;

    std::array<std::vector<SpriteSheet>, kShirtGeometryCount> kit_sheets;
    std::vector<SpriteSheet> keeper_sheets;
    std::vector<SpriteSheet> ball_sheets;      // rotation frames
    SpriteSheet              ball_shadow{};
    std::vector<SpriteSheet> number_sheets;
    std::vector<uint8_t>     synth_ball_px;
    std::vector<uint8_t>     synth_shadow_px;
    PitchTiles               pitch_tiles{};
    GamePalette              palette{};
    std::span<const uint8_t> palette_bytes{};
    std::span<const uint8_t> kit_ordinals{};
    bool                     pitch_ok = false;
    bool                     has_shadow = false;
    uint64_t                 manifest_fp = 0;
    bool                     has_manifest = false;
};

ImportedAssets::ImportedAssets() = default;
ImportedAssets::~ImportedAssets() = default;

static bool BuildSheets(const render::LoadedPack& pack, std::vector<SpriteSheet>& out) {
    if (!pack.ok) return false;
    out.resize(pack.header.entry_count);
    for (uint32_t i = 0; i < pack.header.entry_count; ++i) {
        if (!render::FillSheet(pack, i, out[i])) return false;
    }
    return true;
}

std::unique_ptr<ImportedAssets> ImportedAssets::TryOpen(const char* dir,
                                                        uint64_t expected_fp) {
    if (!dir || !*dir) return nullptr;
    auto src = std::unique_ptr<ImportedAssets>(new ImportedAssets());
    src->impl_ = std::make_unique<Impl>();
    auto& im = *src->impl_;

    const std::string root = dir;

    std::vector<uint8_t> man_bytes;
    if (render::ReadFileBytes((root + "/manifest.atm").c_str(), man_bytes)) {
        render::ManifestInfo man;
        if (!render::ReadManifest(man_bytes, man)) return nullptr;
        if (expected_fp != 0 && man.fingerprint != expected_fp) return nullptr;
        im.manifest_fp  = man.fingerprint;
        im.has_manifest = true;
    } else if (expected_fp != 0) {
        return nullptr; // caller demanded a fingerprint but no manifest
    }

    static const char* kKitFiles[kShirtGeometryCount] = {
        "/kit_vstripe.atp", "/kit_hstripe.atp", "/kit_sleeves.atp"};
    if (!render::LoadPackFile(root + kKitFiles[0], im.kits[0])) return nullptr;
    for (int g = 1; g < kShirtGeometryCount; ++g) {
        // A partial import still plays; every team just wears the same cut of shirt.
        if (!render::LoadPackFile(root + kKitFiles[g], im.kits[static_cast<size_t>(g)]))
            im.kits[static_cast<size_t>(g)] = im.kits[0];
    }
    if (!render::LoadPackFile(root + "/pitch1.atp", im.pitch)) return nullptr;

    for (int g = 0; g < kShirtGeometryCount; ++g) {
        if (!BuildSheets(im.kits[static_cast<size_t>(g)],
                         im.kit_sheets[static_cast<size_t>(g)]))
            return nullptr;
    }
    if (im.kit_sheets[0].empty()) return nullptr;

    if (render::LoadPackFile(root + "/keepers.atp", im.keepers))
        BuildSheets(im.keepers, im.keeper_sheets);
    if (render::LoadPackFile(root + "/numbers.atp", im.numbers))
        BuildSheets(im.numbers, im.number_sheets);

    if (render::LoadPackFile(root + "/ball.atp", im.ball)) {
        std::vector<SpriteSheet> all;
        if (BuildSheets(im.ball, all) && !all.empty()) {
            // The bank is four rotation frames then the shadow; a shorter pack keeps
            // whatever it has and loses the shadow rather than mis-indexing it.
            const size_t rot = all.size() > kBallFrameCount
                                   ? static_cast<size_t>(kBallFrameCount)
                                   : all.size();
            im.ball_sheets.assign(all.begin(), all.begin() + static_cast<ptrdiff_t>(rot));
            if (all.size() > rot) {
                im.ball_shadow = all[rot];
                im.has_shadow  = true;
            }
        }
    }
    if (im.ball_sheets.empty()) {
        // No ball pack — a small white disc, palette indices 2/3, plus a flat shadow.
        constexpr int W = 4, H = 4;
        static constexpr uint8_t kDisc[] = {0, 2, 2, 0, 2, 3, 2, 2,
                                            2, 2, 2, 2, 0, 2, 2, 0};
        im.synth_ball_px.assign(kDisc, kDisc + W * H);
        SpriteSheet s{};
        s.width = W; s.height = H; s.anchor_x = 1; s.anchor_y = 3;
        s.pixels = im.synth_ball_px;
        im.ball_sheets.assign(static_cast<size_t>(kBallFrameCount), s);

        im.synth_shadow_px.assign(static_cast<size_t>(W * H), 8);
        im.ball_shadow = s;
        im.ball_shadow.pixels = im.synth_shadow_px;
        im.has_shadow = true;
    }

    im.pitch_ok = render::BindPitch(im.pitch, im.pitch_tiles);
    if (!im.pitch_ok) return nullptr;
    if (render::LoadPackFile(root + "/pitch1.atl", im.pitch_pal)) {
        render::BindPitchPalette(im.pitch_pal, im.pitch_tiles);
    }
    if (render::LoadPackFile(root + "/palette.atl", im.game_pal) &&
        im.game_pal.header.kind == assets::Kind::kPalette &&
        im.game_pal.header.entry_count >= 1) {
        const assets::Entry e = assets::EntryAt(im.game_pal.bytes, 0);
        im.palette_bytes = assets::Pixels(im.game_pal.bytes, im.game_pal.header, e);
        if (!im.palette_bytes.empty() && (im.palette_bytes.size() % 4) == 0) {
            im.palette.rgba  = im.palette_bytes;
            im.palette.count = static_cast<uint32_t>(im.palette_bytes.size() / 4);
        }
        const auto aux = assets::Aux(im.game_pal.bytes, im.game_pal.header);
        if (aux.size() >= kAuxLayerBytes + kAuxOrdinalBytes)
            im.kit_ordinals = aux.subspan(kAuxLayerBytes, kAuxOrdinalBytes);
    }

    return src;
}

const SpriteSheet* ImportedAssets::Player(ShirtGeometry geo, int frame) const {
    const size_t g = static_cast<size_t>(geo);
    if (g >= kShirtGeometryCount) return nullptr;
    const auto& sheets = impl_->kit_sheets[g];
    if (frame < 0 || frame >= static_cast<int>(sheets.size())) return nullptr;
    return &sheets[static_cast<size_t>(frame)];
}

const SpriteSheet* ImportedAssets::Keeper(int frame) const {
    const auto& sheets = impl_->keeper_sheets;
    if (frame < 0 || frame >= static_cast<int>(sheets.size())) return nullptr;
    return &sheets[static_cast<size_t>(frame)];
}

const SpriteSheet* ImportedAssets::Ball(int frame) const {
    const auto& sheets = impl_->ball_sheets;
    if (sheets.empty()) return nullptr;
    if (frame < 0) frame = 0;
    return &sheets[static_cast<size_t>(frame) % sheets.size()];
}

const SpriteSheet* ImportedAssets::BallShadow() const {
    return impl_->has_shadow ? &impl_->ball_shadow : nullptr;
}

const SpriteSheet* ImportedAssets::Number(int shirt_number) const {
    const int i = shirt_number - 1;
    if (i < 0 || i >= static_cast<int>(impl_->number_sheets.size())) return nullptr;
    return &impl_->number_sheets[static_cast<size_t>(i)];
}

const PitchTiles* ImportedAssets::Pitch(PitchType) const {
    return impl_->pitch_ok ? &impl_->pitch_tiles : nullptr;
}

GamePalette ImportedAssets::Palette() const {
    return impl_->palette;
}

std::span<const uint8_t> ImportedAssets::KitColourOrdinals() const {
    return impl_->kit_ordinals;
}

uint64_t ImportedAssets::ManifestFingerprint() const {
    return impl_->manifest_fp;
}

int ImportedAssets::PlayerFrames() const {
    return static_cast<int>(impl_->kit_sheets[0].size());
}

} // namespace at
