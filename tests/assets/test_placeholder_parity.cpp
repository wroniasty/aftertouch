// A4 work item 4–5: placeholder and imported sources agree on every dimension,
// frame count, anchor and index — everything except pixel colour.
#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_pack.hpp"
#include "render/imported_assets.hpp"
#include "render/placeholder_assets.hpp"
#include "render/pack_bank.hpp"

namespace fs = std::filesystem;
using namespace at;

#ifndef AT_PLACEHOLDER_DIR
#error "AT_PLACEHOLDER_DIR required"
#endif

namespace {

std::vector<uint8_t> RetintPack(const std::vector<uint8_t>& src, uint8_t add) {
    REQUIRE(assets::Validate(src));
    assets::Header h;
    REQUIRE(assets::ReadHeader(src, h));
    auto out = src;
    for (uint32_t i = 0; i < h.entry_count; ++i) {
        const assets::Entry e = assets::EntryAt(out, i);
        auto px = out.begin() + h.blob_offset + e.offset;
        for (uint32_t k = 0; k < e.size; ++k) {
            if (px[k] != 0) px[k] = static_cast<uint8_t>(px[k] + add);
        }
    }
    // Flip source kind so this directory reads as an import twin.
    out[36] = static_cast<uint8_t>(assets::SourceKind::kOriginal);
    REQUIRE(assets::Validate(out));
    return out;
}

void WriteBytes(const fs::path& p, const std::vector<uint8_t>& b) {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    REQUIRE(fopen_s(&f, p.string().c_str(), "wb") == 0);
#else
    f = std::fopen(p.string().c_str(), "wb");
    REQUIRE(f);
#endif
    REQUIRE(std::fwrite(b.data(), 1, b.size(), f) == b.size());
    std::fclose(f);
}

fs::path MakeImportTwin(const fs::path& placeholder_dir) {
    const auto twin = fs::temp_directory_path() / "at_parity_import";
    fs::create_directories(twin);
    for (const char* name : {"kit_vstripe.atp", "kit_hstripe.atp", "kit_sleeves.atp",
                             "keepers.atp", "ball.atp", "numbers.atp", "pitch1.atp"}) {
        std::vector<uint8_t> bytes;
        REQUIRE(render::ReadFileBytes((placeholder_dir / name).string().c_str(), bytes));
        WriteBytes(twin / name, RetintPack(bytes, 3));
    }
    return twin;
}

} // namespace

TEST_CASE("PlaceholderAssets opens committed packs with dimensional contract") {
    auto ph = PlaceholderAssets::Open(AT_PLACEHOLDER_DIR);
    REQUIRE(ph);
    CHECK(ph->IsPlaceholder());
    CHECK(ph->PlayerFrames() == kPlayerFrameCount);

    const SpriteSheet* s = ph->Player(ShirtGeometry::VerticalStripes, 0);
    REQUIRE(s);
    CHECK(s->width == kPlayerSpriteW);
    CHECK(s->height == kPlayerSpriteH);
    CHECK(s->pixels.size() == size_t(kPlayerSpriteW * kPlayerSpriteH));

    // Four rotation frames plus the separate ground shadow (C3 §3.4).
    for (int f = 0; f < kBallFrameCount; ++f) REQUIRE(ph->Ball(f));
    REQUIRE(ph->BallShadow());
    REQUIRE(ph->Number(1));
    REQUIRE(ph->Number(kMaxShirtNumber));
    CHECK(ph->Number(0) == nullptr);
    CHECK(ph->Number(kMaxShirtNumber + 1) == nullptr);
    REQUIRE(ph->Keeper(0));
    const PitchTiles* pitch = ph->Pitch(PitchType::Normal);
    REQUIRE(pitch);
    CHECK(pitch->tile_w == kPitchTileSize);
    CHECK(pitch->grid_w == kPitchGridW);
    CHECK(pitch->grid_h == kPitchGridH);
    SpriteSheet tile;
    REQUIRE(pitch->Tile(0, &tile));
    CHECK(tile.width == kPitchTileSize);
}

TEST_CASE("placeholder vs imported parity: dims/anchors/counts, not colours") {
    auto ph = PlaceholderAssets::Open(AT_PLACEHOLDER_DIR);
    REQUIRE(ph);

    const fs::path twin = MakeImportTwin(AT_PLACEHOLDER_DIR);
    auto imp = ImportedAssets::TryOpen(twin.string().c_str());
    REQUIRE(imp);
    CHECK_FALSE(imp->IsPlaceholder());

    CHECK(ph->PlayerFrames() == imp->PlayerFrames());

    for (int frame = 0; frame < kPlayerFrameCount; ++frame) {
        for (ShirtGeometry geo : {ShirtGeometry::VerticalStripes,
                                  ShirtGeometry::HorizontalStripes,
                                  ShirtGeometry::ColouredSleeves}) {
            const SpriteSheet* a = ph->Player(geo, frame);
            const SpriteSheet* b = imp->Player(geo, frame);
            REQUIRE(a);
            REQUIRE(b);
            CHECK(a->width == b->width);
            CHECK(a->height == b->height);
            CHECK(a->anchor_x == b->anchor_x);
            CHECK(a->anchor_y == b->anchor_y);
            CHECK(a->pixels.size() == b->pixels.size());
            // At least one pixel must differ (the retint).
            bool differ = false;
            for (size_t i = 0; i < a->pixels.size(); ++i) {
                if (a->pixels[i] != b->pixels[i]) {
                    differ = true;
                    break;
                }
            }
            CHECK(differ);
        }
    }

    const PitchTiles* pa = ph->Pitch(PitchType::Dry);
    const PitchTiles* pb = imp->Pitch(PitchType::Dry);
    REQUIRE(pa);
    REQUIRE(pb);
    CHECK(pa->tile_count == pb->tile_count);
    CHECK(pa->grid_w == pb->grid_w);
    CHECK(pa->grid_h == pb->grid_h);
    CHECK(pa->indices.size() == pb->indices.size());
    CHECK(std::memcmp(pa->indices.data(), pb->indices.data(), pa->indices.size()) == 0);
}

TEST_CASE("ImportedAssets rejects stale manifest fingerprint") {
    const fs::path twin = MakeImportTwin(AT_PLACEHOLDER_DIR);
    // Copy placeholder manifest then demand a wrong fingerprint.
    std::vector<uint8_t> man;
    REQUIRE(render::ReadFileBytes(
        (std::string(AT_PLACEHOLDER_DIR) + "/manifest.atm").c_str(), man));
    WriteBytes(twin / "manifest.atm", man);

    CHECK(ImportedAssets::TryOpen(twin.string().c_str(), /*expected_fp=*/1) == nullptr);
    auto ok = ImportedAssets::TryOpen(twin.string().c_str(), /*expected_fp=*/0);
    REQUIRE(ok);
}

TEST_CASE("OpenAssetSource falls back to placeholder when generated missing") {
    auto src = OpenAssetSource("definitely/missing/generated", AT_PLACEHOLDER_DIR);
    REQUIRE(src);
    CHECK(src->IsPlaceholder());
}
