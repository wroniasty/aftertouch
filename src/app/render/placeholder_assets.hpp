#pragma once
#include "render/asset_source.hpp"

#include <memory>
#include <string>

namespace at {

class PlaceholderAssets final : public IAssetSource {
public:
    ~PlaceholderAssets() override;
    static std::unique_ptr<PlaceholderAssets> Open(const char* dir);

    const SpriteSheet* Player(TeamSlot slot, Dir dir, int frame) const override;
    const SpriteSheet* Ball() const override;
    const PitchTiles*  Pitch(PitchType type) const override;
    bool               IsPlaceholder() const override { return true; }

    int PlayerFrames() const;
    const SpriteSheet* PlayerSheet(TeamSlot slot, int frame) const;

private:
    PlaceholderAssets();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace at
