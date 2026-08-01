#include "pack_writer.hpp"

#include <cstdio>
#include <fstream>

namespace at::assetc {

namespace fs = std::filesystem;

bool PackBuilder::Add(uint16_t width, uint16_t height, int16_t anchor_x, int16_t anchor_y,
                      const std::vector<uint8_t>& pixels, std::string& err) {
    if (pixels.size() != static_cast<size_t>(width) * height) {
        err = "entry pixel count does not match its dimensions";
        return false;
    }
    assets::Entry e;
    e.width    = width;
    e.height   = height;
    e.anchor_x = anchor_x;
    e.anchor_y = anchor_y;
    e.offset   = static_cast<uint32_t>(blob_.size());
    e.size     = static_cast<uint32_t>(pixels.size());
    entries_.push_back(e);
    blob_.insert(blob_.end(), pixels.begin(), pixels.end());
    return true;
}

void PackBuilder::MixSource(const std::vector<uint8_t>& bytes) {
    fingerprint_ = assets::Fingerprint(bytes, fingerprint_);
}

void PackBuilder::MixSource(const std::string& text) {
    fingerprint_ = assets::Fingerprint(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()),
        fingerprint_);
}

std::vector<uint8_t> PackBuilder::Build() const {
    assets::Header h;
    h.kind        = kind_;
    h.source      = source_;
    h.entry_count = static_cast<uint32_t>(entries_.size());
    h.aux_w       = aux_w_;
    h.aux_h       = aux_h_;
    h.fingerprint = fingerprint_;

    const uint32_t table_end =
        assets::TableOffset() + assets::TableSize(h.entry_count);
    h.blob_offset = table_end;
    h.blob_size   = static_cast<uint32_t>(blob_.size());
    h.aux_offset  = aux_.empty() ? 0u : h.blob_offset + h.blob_size;
    h.aux_size    = static_cast<uint32_t>(aux_.size());

    std::vector<uint8_t> out(static_cast<size_t>(table_end) + blob_.size() + aux_.size());
    assets::WriteHeader(h, out);
    for (size_t i = 0; i < entries_.size(); ++i) {
        assets::WriteEntry(entries_[i],
                           std::span<uint8_t>(out).subspan(
                               assets::TableOffset() + i * assets::kEntrySize,
                               assets::kEntrySize));
    }
    std::copy(blob_.begin(), blob_.end(), out.begin() + h.blob_offset);
    if (!aux_.empty())
        std::copy(aux_.begin(), aux_.end(), out.begin() + h.aux_offset);
    return out;
}

bool IsWriteLocationAllowed(const fs::path& out_path, std::string& why_not) {
    std::error_code ec;
    fs::path dir = fs::weakly_canonical(out_path, ec).parent_path();
    if (ec) dir = out_path.parent_path();

    // Find the nearest enclosing repository, if any.
    for (fs::path p = dir; !p.empty(); p = p.parent_path()) {
        if (fs::exists(p / ".git", ec)) {
            const fs::path allowed = fs::weakly_canonical(p / "assets" / "generated", ec);
            for (fs::path q = dir; !q.empty(); q = q.parent_path()) {
                if (q == allowed) return true;
                if (q == p) break;
            }
            why_not = "refusing to write into the tracked tree at " + p.string() +
                      "; imported assets belong under assets/generated/ only "
                      "(see PLAN.md section 10)";
            return false;
        }
        if (p == p.parent_path()) break;
    }
    // Not inside a repository at all: nothing to protect.
    return true;
}

bool WritePack(const fs::path& out_path, const std::vector<uint8_t>& bytes,
               std::string& err) {
    if (!IsWriteLocationAllowed(out_path, err)) return false;

    std::error_code ec;
    fs::create_directories(out_path.parent_path(), ec);

    std::ofstream f(out_path, std::ios::binary | std::ios::trunc);
    if (!f) {
        err = "cannot open " + out_path.string() + " for writing";
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        err = "write failed for " + out_path.string();
        return false;
    }
    return true;
}

} // namespace at::assetc
