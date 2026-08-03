#pragma once
#include "render/asset_source.hpp"

#include <memory>
#include <string>

namespace at {

class PlaceholderAssets final : public IAssetSource {
public:
    ~PlaceholderAssets() override;
    static std::unique_ptr<PlaceholderAssets> Open(const char* dir);

    const SpriteSheet* Player(ShirtGeometry geo, int frame) const override;
    const SpriteSheet* Keeper(int frame) const override;
    const SpriteSheet* Ball(int frame) const override;
    const SpriteSheet* BallShadow() const override;
    const SpriteSheet* Number(int shirt_number) const override;
    const PitchTiles*  Pitch(PitchType type) const override;
    std::span<const uint8_t> KitColourOrdinals() const override;
    bool               IsPlaceholder() const override { return true; }

    int PlayerFrames() const;

private:
    PlaceholderAssets();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace at
