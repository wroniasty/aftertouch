#include "render/asset_source.hpp"
#include "render/imported_assets.hpp"
#include "render/placeholder_assets.hpp"

namespace at {

std::unique_ptr<IAssetSource> OpenAssetSource(const char* generated_dir,
                                              const char* placeholder_dir) {
    if (generated_dir && *generated_dir) {
        if (auto imp = ImportedAssets::TryOpen(generated_dir)) return imp;
    }
    if (placeholder_dir && *placeholder_dir) {
        if (auto ph = PlaceholderAssets::Open(placeholder_dir)) return ph;
    }
    return nullptr;
}

} // namespace at
