#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_pack.hpp"

namespace at::assetc {

// Accumulates entries and their pixels, then serialises a complete pack.
class PackBuilder {
public:
    PackBuilder(assets::Kind kind, assets::SourceKind source)
        : kind_(kind), source_(source) {}

    // Pixels must be exactly width*height bytes; the pack's structural validator
    // enforces that on read, so it is enforced here on write too.
    bool Add(uint16_t width, uint16_t height, int16_t anchor_x, int16_t anchor_y,
             const std::vector<uint8_t>& pixels, std::string& err);

    void SetAux(uint16_t w, uint16_t h, std::vector<uint8_t> data) {
        aux_w_ = w;
        aux_h_ = h;
        aux_   = std::move(data);
    }

    // Mixed into the fingerprint. Call once per input file consumed, so that a pack
    // knows what it was built from and a stale one is detectable.
    void MixSource(const std::vector<uint8_t>& bytes);
    void MixSource(const std::string& text);

    std::vector<uint8_t> Build() const;

private:
    assets::Kind             kind_;
    assets::SourceKind       source_;
    std::vector<assets::Entry> entries_;
    std::vector<uint8_t>     blob_;
    std::vector<uint8_t>     aux_;
    uint16_t                 aux_w_ = 0;
    uint16_t                 aux_h_ = 0;
    uint64_t                 fingerprint_ = 1469598103934665603ull;
};

// Writes bytes to disk, refusing to write into the repository's tracked tree.
//
// PLAN.md section 10 is unambiguous that the rightsholder's artwork must never enter
// version control, and "we will be careful" is not a mechanism. An importer that
// quietly fills a tracked directory is not recoverable by `git rm` -- the history
// keeps it. So the check is in the tool, not in the instructions: walk up from the
// destination looking for a .git directory, and if one is found, require the
// destination to be under <repo>/assets/generated.
bool WritePack(const std::filesystem::path& out_path,
               const std::vector<uint8_t>& bytes, std::string& err);

// Exposed for testing; WritePack calls it.
bool IsWriteLocationAllowed(const std::filesystem::path& out_path, std::string& why_not);

} // namespace at::assetc
