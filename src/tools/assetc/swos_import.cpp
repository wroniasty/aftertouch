#include "swos_import.hpp"

#include "pack_writer.hpp"

namespace at::assetc {

namespace fs = std::filesystem;

namespace {

// The documented sprite banks. Ranges from docs/SWOS/sprites.txt, cross-checked
// against convertGameSprites.py's constants and confirmed by the probe: every one of
// these lands on the sprite count it should.
struct Bank {
    const char* name;
    size_t      first;
    size_t      count;
};

// Bands indexed straight out of sprite.dat. The player bank is NOT here: it is loaded
// once per team file into set.geometries (swos_sprites.hpp), because a sprite.dat slot
// describes which team file a particular run loaded, not which shirt geometry the art
// depicts. A4 §6.5 is resolved there.
constexpr Bank kBanks[] = {
    {"charset",     0,    227},
    {"score",       227,  114},
    {"keepers",     947,   58},
    {"bench",       1310,  12},
    // The match's small change, all of it in bench.dat and none of it imported before.
    {"ball",        1179,   5},   // 4 rotation frames at 4x4, then the shadow
    {"numbers",     1188,  16},   // shirt numbers 1..16; 1..9 are 3x5, 10..16 are 5x5
};

} // namespace

const std::array<uint8_t, 16>& KitLayerTable() {
    static const std::array<uint8_t, 16> kTable = {
        /*  0 */ static_cast<uint8_t>(KitLayer::kBackground),
        /*  1 */ static_cast<uint8_t>(KitLayer::kBackground),
        /*  2 */ static_cast<uint8_t>(KitLayer::kBackground),
        /*  3 */ static_cast<uint8_t>(KitLayer::kBackground),
        /*  4 */ static_cast<uint8_t>(KitLayer::kSkin),
        /*  5 */ static_cast<uint8_t>(KitLayer::kSkin),
        /*  6 */ static_cast<uint8_t>(KitLayer::kSkin),
        /*  7 */ static_cast<uint8_t>(KitLayer::kBackground),   // "goes to zero"
        /*  8 */ static_cast<uint8_t>(KitLayer::kBackground),
        /*  9 */ static_cast<uint8_t>(KitLayer::kHair),
        /* 10 */ static_cast<uint8_t>(KitLayer::kShirt),
        /* 11 */ static_cast<uint8_t>(KitLayer::kStripes),
        /* 12 */ static_cast<uint8_t>(KitLayer::kHair),
        /* 13 */ static_cast<uint8_t>(KitLayer::kHair),
        /* 14 */ static_cast<uint8_t>(KitLayer::kShorts),
        /* 15 */ static_cast<uint8_t>(KitLayer::kSocks),
    };
    return kTable;
}

const std::array<uint8_t, 10>& KitColourOrdinals() {
    static const std::array<uint8_t, 10> kOrdinals = {1, 2, 3, 6, 10, 11, 12, 13, 14, 15};
    return kOrdinals;
}

namespace {

// One 101-frame geometry bank. Frames the original left empty are written as 1x1
// transparent rather than skipped, because the animation tables index by frame number
// and a short pack would silently shift every index after the gap.
bool WriteGeometryBank(const std::vector<SwosSprite>& frames, const char* name,
                       const fs::path& out_dir, SwosImportReport& report,
                       std::string& err) {
    PackBuilder pack(assets::Kind::kSprites, assets::SourceKind::kOriginal);
    const std::vector<uint8_t> blank(1, 0);
    int written = 0;
    for (int k = 0; k < kPlayerBankFrames; ++k) {
        const bool have = k < static_cast<int>(frames.size()) && !frames[static_cast<size_t>(k)].indices.empty();
        if (have) {
            const SwosSprite& s = frames[static_cast<size_t>(k)];
            if (!pack.Add(s.header.width, s.header.nlines, s.header.centre_x,
                          s.header.centre_y, s.indices, err))
                return false;
            pack.MixSource(s.indices);
            ++written;
        } else if (!pack.Add(1, 1, 0, 0, blank, err)) {
            return false;
        }
    }
    const std::vector<uint8_t> bytes = pack.Build();
    if (!assets::Validate(bytes)) {
        err = std::string("bank ") + name + " failed its own validator";
        return false;
    }
    if (!WritePack(out_dir / (std::string(name) + ".atp"), bytes, err)) return false;
    ++report.packs;
    report.sprites += written;
    report.bytes += bytes.size();
    return true;
}

} // namespace

bool ImportSwosSprites(const SwosSpriteSet& set, const fs::path& out_dir,
                       SwosImportReport& report, std::string& err) {
    static const char* kGeometryBankNames[kShirtGeometryCount] = {
        "kit_vstripe", "kit_hstripe", "kit_sleeves"};
    for (int g = 0; g < kShirtGeometryCount; ++g) {
        if (!WriteGeometryBank(set.geometries[static_cast<size_t>(g)],
                               kGeometryBankNames[g], out_dir, report, err))
            return false;
        if (g == 0) {
            for (const SwosSprite& s : set.geometries[0])
                for (uint8_t px : s.indices)
                    if (px < 16) ++report.layer_pixels[KitLayerTable()[px]];
        }
    }

    for (const Bank& bank : kBanks) {
        PackBuilder pack(assets::Kind::kSprites, assets::SourceKind::kOriginal);
        int written = 0;

        for (size_t k = 0; k < bank.count; ++k) {
            const size_t i = bank.first + k;
            if (i >= set.sprites.size()) break;
            const SwosSprite& s = set.sprites[i];
            if (s.indices.empty()) continue;

            // Pixels go in as PALETTE INDICES, not colours. The index carries the
            // layer, so expanding to RGB here would destroy the kit system and force
            // us to ship six near-empty masks per frame to get it back.
            if (!pack.Add(s.header.width, s.header.nlines, s.header.centre_x,
                          s.header.centre_y, s.indices, err))
                return false;
            pack.MixSource(s.indices);
            ++written;
        }

        const std::vector<uint8_t> bytes = pack.Build();
        if (!assets::Validate(bytes)) {
            err = std::string("bank ") + bank.name + " failed its own validator";
            return false;
        }
        if (!WritePack(out_dir / (std::string(bank.name) + ".atp"), bytes, err))
            return false;

        ++report.packs;
        report.sprites += written;
        report.bytes += bytes.size();
    }

    // The palette pack: 256 RGBA entries, with the kit-layer routing table in the aux
    // section so the runtime reads it rather than hardcoding it.
    PackBuilder pal(assets::Kind::kPalette, assets::SourceKind::kOriginal);
    std::vector<uint8_t> rgba(256 * 4);
    for (size_t i = 0; i < 256; ++i) {
        rgba[i * 4 + 0] = set.palette[i].r;
        rgba[i * 4 + 1] = set.palette[i].g;
        rgba[i * 4 + 2] = set.palette[i].b;
        rgba[i * 4 + 3] = set.palette[i].a;
    }
    if (!pal.Add(256 * 4, 1, 0, 0, rgba, err)) return false;
    pal.MixSource(rgba);

    const std::array<uint8_t, 16>& layers   = KitLayerTable();
    const std::array<uint8_t, 10>& ordinals = KitColourOrdinals();
    std::vector<uint8_t> aux;
    aux.reserve(kPaletteAuxBytes);
    aux.insert(aux.end(), layers.begin(), layers.end());
    aux.insert(aux.end(), ordinals.begin(), ordinals.end());
    pal.SetAux(kPaletteAuxBytes, 1, std::move(aux));

    const std::vector<uint8_t> pal_bytes = pal.Build();
    if (!assets::Validate(pal_bytes)) {
        err = "palette pack failed its own validator";
        return false;
    }
    if (!WritePack(out_dir / "palette.atl", pal_bytes, err)) return false;
    ++report.packs;
    report.bytes += pal_bytes.size();

    return true;
}

} // namespace at::assetc
