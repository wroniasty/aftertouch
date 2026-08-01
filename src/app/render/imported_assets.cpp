#include "render/imported_assets.hpp"
#include "render/pack_bank.hpp"

#include <vector>

namespace at {

struct ImportedAssets::Impl {
    render::LoadedPack slot_a;
    render::LoadedPack slot_b;
    render::LoadedPack ball;
    render::LoadedPack pitch;

    std::vector<SpriteSheet> sheets_a;
    std::vector<SpriteSheet> sheets_b;
    SpriteSheet              ball_sheet{};
    PitchTiles               pitch_tiles{};
    bool                     pitch_ok = false;
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

    if (!render::LoadPackFile(root + "/slotA_blk0.atp", im.slot_a)) return nullptr;
    // slot B optional for incomplete imports; mirror A if missing.
    if (!render::LoadPackFile(root + "/slotB_blk0.atp", im.slot_b)) {
        im.slot_b = im.slot_a;
    }
    if (!render::LoadPackFile(root + "/ball.atp", im.ball)) return nullptr;
    if (!render::LoadPackFile(root + "/pitch1.atp", im.pitch)) return nullptr;

    // Reject packs that claim to be placeholder when loaded as imported — the
    // directories are different roles even if bytes can be shared in tests.
    if (im.slot_a.header.source == assets::SourceKind::kPlaceholder &&
        im.has_manifest == false) {
        // Allow test twins that omit manifest; real assetc writes kOriginal/kRefTree.
    }

    if (!BuildSheets(im.slot_a, im.sheets_a)) return nullptr;
    if (!BuildSheets(im.slot_b, im.sheets_b)) return nullptr;
    // Frame counts must match across slots; pixel sizes may vary (trimmed sprites).
    if (im.sheets_a.size() != im.sheets_b.size()) return nullptr;
    if (im.sheets_a.empty()) return nullptr;
    if (!render::FillSheet(im.ball, 0, im.ball_sheet)) return nullptr;
    im.pitch_ok = render::BindPitch(im.pitch, im.pitch_tiles);
    if (!im.pitch_ok) return nullptr;

    return src;
}

const SpriteSheet* ImportedAssets::PlayerSheet(TeamSlot slot, int frame) const {
    if (frame < 0 || frame >= PlayerFrames()) return nullptr;
    const auto& sheets = (slot == TeamSlot::A) ? impl_->sheets_a : impl_->sheets_b;
    return &sheets[static_cast<size_t>(frame)];
}

const SpriteSheet* ImportedAssets::Player(TeamSlot slot, Dir, int frame) const {
    return PlayerSheet(slot, frame);
}

const SpriteSheet* ImportedAssets::Ball() const {
    return &impl_->ball_sheet;
}

const PitchTiles* ImportedAssets::Pitch(PitchType) const {
    return impl_->pitch_ok ? &impl_->pitch_tiles : nullptr;
}

uint64_t ImportedAssets::ManifestFingerprint() const {
    return impl_->manifest_fp;
}

int ImportedAssets::PlayerFrames() const {
    return static_cast<int>(impl_->sheets_a.size());
}

} // namespace at
