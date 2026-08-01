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

// Bands are named for WHERE THEY CAME FROM, not for what they depict.
//
// The obvious names -- "vertical stripes", "coloured sleeves", "horizontal stripes" --
// are exactly the trap. A band's shirt geometry depends on which team file the game
// loaded into that slot, so the same index means different art on different runs.
// convertGameSprites.py calls 644 "horizontal stripes" because the extraction that
// produced its BMPs had team3 in slot B; with the default team1/team2 pair, rendering
// 644 shows contrasting sleeves instead. Naming a bank after a run's configuration is
// how that becomes a silent bug, so these say slot and block and nothing more. Which
// geometry is which is C3's to resolve -- see A4 section 6.5.
constexpr Bank kBanks[] = {
    {"charset",     0,    227},
    {"score",       227,  114},
    {"slotA_blk0",  341,  101},   // team file in slot A, first 101-frame block
    {"slotA_blk1",  442,  101},
    {"slotA_blk2",  543,  101},
    {"slotB_blk0",  644,  101},   // team file in slot B
    {"slotB_blk1",  745,  101},
    {"slotB_blk2",  846,  101},
    {"keepers",     947,   58},
    {"bench",       1310,  12},
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

bool ImportSwosSprites(const SwosSpriteSet& set, const fs::path& out_dir,
                       SwosImportReport& report, std::string& err) {
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

            if (bank.first == 341) {
                for (uint8_t px : s.indices)
                    if (px < 16) ++report.layer_pixels[KitLayerTable()[px]];
            }
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

    const std::array<uint8_t, 16>& layers = KitLayerTable();
    pal.SetAux(16, 1, std::vector<uint8_t>(layers.begin(), layers.end()));

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
