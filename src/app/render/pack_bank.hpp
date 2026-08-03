#pragma once
#include "assets/asset_pack.hpp"
#include "render/asset_source.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Shared ATAP load helpers for PlaceholderAssets / ImportedAssets.

namespace at::render {

struct LoadedPack {
    std::vector<uint8_t> bytes;
    assets::Header       header{};
    bool                 ok = false;
};

inline bool ReadFileBytes(const char* path, std::vector<uint8_t>& out) {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0) return false;
#else
    f = std::fopen(path, "rb");
    if (!f) return false;
#endif
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    const long sz = std::ftell(f);
    if (sz < 0 || std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    const size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return n == out.size();
}

inline bool LoadPackFile(const std::string& path, LoadedPack& out) {
    out = LoadedPack{};
    if (!ReadFileBytes(path.c_str(), out.bytes)) return false;
    if (!assets::Validate(out.bytes)) return false;
    if (!assets::ReadHeader(out.bytes, out.header)) return false;
    out.ok = true;
    return true;
}

inline bool FillSheet(const LoadedPack& pack, uint32_t index, SpriteSheet& sheet) {
    if (!pack.ok || index >= pack.header.entry_count) return false;
    const assets::Entry e = assets::EntryAt(pack.bytes, index);
    sheet.width    = e.width;
    sheet.height   = e.height;
    sheet.anchor_x = e.anchor_x;
    sheet.anchor_y = e.anchor_y;
    sheet.pixels   = assets::Pixels(pack.bytes, pack.header, e);
    return sheet.pixels.size() == static_cast<size_t>(e.width) * e.height;
}

// manifest.atm — little-endian, versioned TOC for a pack directory.
inline constexpr uint32_t kManifestMagic   = 0x314D5441u; // "ATM1"
inline constexpr uint16_t kManifestVersion = 1;
inline constexpr size_t   kManifestHeader  = 16;
inline constexpr size_t   kManifestEntry   = 40; // name[32] + fingerprint u64

struct ManifestInfo {
    uint16_t             version = 0;
    assets::SourceKind   source  = assets::SourceKind::kPlaceholder;
    uint64_t             fingerprint = 0;
    uint32_t             pack_count  = 0;
};

inline bool ReadManifest(const std::vector<uint8_t>& bytes, ManifestInfo& m) {
    if (bytes.size() < kManifestHeader) return false;
    size_t at = 0;
    auto gu32 = [&]() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint32_t(bytes[at++]) << (8 * i);
        return v;
    };
    auto gu16 = [&]() {
        const uint16_t lo = bytes[at++];
        const uint16_t hi = bytes[at++];
        return uint16_t(lo | (hi << 8));
    };
    auto gu64 = [&]() {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= uint64_t(bytes[at++]) << (8 * i);
        return v;
    };
    if (gu32() != kManifestMagic) return false;
    m.version = gu16();
    if (m.version != kManifestVersion) return false;
    m.source = static_cast<assets::SourceKind>(bytes[at++]);
    at++; // reserved
    m.fingerprint = gu64();
    // pack_count follows header in full writes; tolerate header-only stubs.
    if (bytes.size() >= kManifestHeader + 4) {
        m.pack_count = gu32();
    }
    return true;
}

inline uint32_t PitchTileFn(const PitchTiles* pt, uint32_t i, SpriteSheet* out) {
    if (!pt || !pt->_pack || !out) return 0;
    const auto* pack = static_cast<const LoadedPack*>(pt->_pack);
    return FillSheet(*pack, i, *out) ? 1u : 0u;
}

inline bool BindPitch(const LoadedPack& pack, PitchTiles& out) {
    if (!pack.ok || pack.header.kind != assets::Kind::kPitch) return false;
    out.tile_count = pack.header.entry_count;
    out.grid_w     = pack.header.aux_w;
    out.grid_h     = pack.header.aux_h;
    out.indices    = assets::Aux(pack.bytes, pack.header);
    out.palette_rgba  = {};
    out.palette_count = 0;
    out.from_original = pack.header.source == assets::SourceKind::kOriginal;
    if (pack.header.entry_count == 0) return false;
    const assets::Entry e0 = assets::EntryAt(pack.bytes, 0);
    out.tile_w = e0.width;
    out.tile_h = e0.height;
    out._pack  = &pack;
    out._tile_fn = &PitchTileFn;
    return out.indices.size() == size_t(out.grid_w) * out.grid_h * 2;
}

// Palette pack: one entry whose pixels are count×4 RGBA bytes (width = count*4, h = 1).
inline bool BindPitchPalette(const LoadedPack& pal, PitchTiles& pitch) {
    if (!pal.ok || pal.header.kind != assets::Kind::kPalette) return false;
    if (pal.header.entry_count < 1) return false;
    const assets::Entry e = assets::EntryAt(pal.bytes, 0);
    const auto px = assets::Pixels(pal.bytes, pal.header, e);
    if (px.size() < 4 || (px.size() % 4) != 0) return false;
    pitch.palette_rgba  = px;
    pitch.palette_count = static_cast<uint32_t>(px.size() / 4);
    return true;
}

} // namespace at::render
