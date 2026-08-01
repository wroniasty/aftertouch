#pragma once
#include "render/asset_source.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace at {

class ImportedAssets final : public IAssetSource {
public:
    ~ImportedAssets() override;
    // Loads packs from dir. Requires slotA_blk0.atp, ball.atp, pitch1.atp at minimum.
    // If manifest.atm is present, its version must match and (when expected_fp != 0)
    // its fingerprint must equal expected_fp — stale generated/ is rejected.
    static std::unique_ptr<ImportedAssets> TryOpen(const char* dir,
                                                   uint64_t expected_fp = 0);

    const SpriteSheet* Player(TeamSlot slot, Dir dir, int frame) const override;
    const SpriteSheet* Ball() const override;
    const PitchTiles*  Pitch(PitchType type) const override;
    bool               IsPlaceholder() const override { return false; }

    uint64_t ManifestFingerprint() const;
    int      PlayerFrames() const;
    const SpriteSheet* PlayerSheet(TeamSlot slot, int frame) const;

private:
    ImportedAssets();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace at
