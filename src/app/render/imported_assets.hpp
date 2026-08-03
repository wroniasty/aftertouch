#pragma once
#include "render/asset_source.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace at {

class ImportedAssets final : public IAssetSource {
public:
    ~ImportedAssets() override;
    // Loads packs from dir. Requires kit_vstripe.atp and pitch1.atp; the other two
    // geometries fall back to vertical stripes when absent, and ball / numbers /
    // keepers degrade to synthesised or null rather than failing the whole source.
    // pitch1.atl supplies the tile palette when present.
    // If manifest.atm is present, its version must match and (when expected_fp != 0)
    // its fingerprint must equal expected_fp — stale generated/ is rejected.
    static std::unique_ptr<ImportedAssets> TryOpen(const char* dir,
                                                   uint64_t expected_fp = 0);

    const SpriteSheet* Player(ShirtGeometry geo, int frame) const override;
    const SpriteSheet* Keeper(int frame) const override;
    const SpriteSheet* Ball(int frame) const override;
    const SpriteSheet* BallShadow() const override;
    const SpriteSheet* Number(int shirt_number) const override;
    const PitchTiles*  Pitch(PitchType type) const override;
    GamePalette        Palette() const override;
    std::span<const uint8_t> KitColourOrdinals() const override;
    bool               IsPlaceholder() const override { return false; }

    uint64_t ManifestFingerprint() const;
    int      PlayerFrames() const;

private:
    ImportedAssets();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace at
