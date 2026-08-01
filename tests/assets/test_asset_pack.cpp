// A4 work item 1: the runtime asset container.
//
// The validator is the point of most of these. The importer reads third-party binary
// data, and PLAN.md section 7 is explicit that a crash there is likely and a silent
// misparse is worse -- so a malformed pack must be *rejected*, not merely survived.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>
#include <vector>

#include "assets/asset_pack.hpp"

using namespace at::assets;

namespace {

// A minimal but structurally complete pack: two entries, a blob, an aux section.
std::vector<uint8_t> MakePack(uint32_t entry_count = 2, uint16_t w = 4, uint16_t h = 3) {
    const uint32_t table_end = TableOffset() + TableSize(entry_count);
    const uint32_t each      = static_cast<uint32_t>(w) * h;
    const uint32_t blob_size = each * entry_count;
    const uint32_t aux_size  = 8;

    Header hd;
    hd.kind        = Kind::kPitch;
    hd.source      = SourceKind::kRefTree;
    hd.entry_count = entry_count;
    hd.blob_offset = table_end;
    hd.blob_size   = blob_size;
    hd.aux_offset  = table_end + blob_size;
    hd.aux_size    = aux_size;
    hd.aux_w       = 4;
    hd.aux_h       = 1;
    hd.fingerprint = 0x0123456789ABCDEFull;

    std::vector<uint8_t> pack(static_cast<size_t>(table_end) + blob_size + aux_size, 0);
    WriteHeader(hd, pack);
    for (uint32_t i = 0; i < entry_count; ++i) {
        Entry e;
        e.width    = w;
        e.height   = h;
        e.anchor_x = static_cast<int16_t>(-3 - static_cast<int>(i));
        e.anchor_y = static_cast<int16_t>(7 + static_cast<int>(i));
        e.offset   = i * each;
        e.size     = each;
        WriteEntry(e, std::span<uint8_t>(pack).subspan(
                          TableOffset() + i * kEntrySize, kEntrySize));
    }
    for (uint32_t i = 0; i < blob_size; ++i)
        pack[hd.blob_offset + i] = static_cast<uint8_t>(i * 7 + 1);
    return pack;
}

} // namespace

TEST_CASE("Asset pack header round-trips") {
    const auto pack = MakePack();
    Header h;
    REQUIRE(ReadHeader(pack, h));
    CHECK(h.kind == Kind::kPitch);
    CHECK(h.source == SourceKind::kRefTree);
    CHECK(h.entry_count == 2u);
    CHECK(h.aux_w == 4);
    CHECK(h.aux_h == 1);
    CHECK(h.fingerprint == 0x0123456789ABCDEFull);
}

TEST_CASE("Asset pack entries round-trip, including negative anchors") {
    const auto pack = MakePack();
    const Entry e0 = EntryAt(pack, 0);
    const Entry e1 = EntryAt(pack, 1);
    CHECK(e0.width == 4);
    CHECK(e0.height == 3);
    CHECK(e0.anchor_x == -3);      // signed: a visual centre may sit outside the box
    CHECK(e0.anchor_y == 7);
    CHECK(e1.anchor_x == -4);
    CHECK(e1.offset == 12u);
}

TEST_CASE("Asset pack pixels are addressable without a parse step") {
    const auto pack = MakePack();
    Header h;
    REQUIRE(ReadHeader(pack, h));
    const auto px = Pixels(pack, h, EntryAt(pack, 1));
    CHECK(px.size() == 12u);
    CHECK(px[0] == static_cast<uint8_t>(12 * 7 + 1));
}

TEST_CASE("Asset pack aux section is addressable") {
    const auto pack = MakePack();
    Header h;
    REQUIRE(ReadHeader(pack, h));
    CHECK(Aux(pack, h).size() == 8u);
}

TEST_CASE("A well-formed pack validates") {
    CHECK(Validate(MakePack()));
    CHECK(Validate(MakePack(1, 16, 16)));
    CHECK(Validate(MakePack(215, 16, 16)));
}

TEST_CASE("Malformed packs are rejected rather than survived") {
    SUBCASE("wrong magic") {
        auto p = MakePack();
        p[0] ^= 0xFF;
        CHECK_FALSE(Validate(p));
    }
    SUBCASE("unknown version") {
        auto p = MakePack();
        p[4] = 0x7F;
        CHECK_FALSE(Validate(p));
    }
    SUBCASE("truncated file") {
        auto p = MakePack();
        p.resize(p.size() / 2);
        CHECK_FALSE(Validate(p));
    }
    SUBCASE("empty file") {
        CHECK_FALSE(Validate(std::vector<uint8_t>{}));
    }
    SUBCASE("entry claiming pixels past the end of the blob") {
        auto p = MakePack();
        Entry e = EntryAt(p, 0);
        e.offset = 0xFFFFFF00u;
        WriteEntry(e, std::span<uint8_t>(p).subspan(TableOffset(), kEntrySize));
        CHECK_FALSE(Validate(p));
    }
    SUBCASE("entry whose size disagrees with its dimensions") {
        auto p = MakePack();
        Entry e = EntryAt(p, 0);
        e.width = 999;   // size stays 12
        WriteEntry(e, std::span<uint8_t>(p).subspan(TableOffset(), kEntrySize));
        CHECK_FALSE(Validate(p));
    }
    SUBCASE("entry count larger than the file can hold") {
        auto p = MakePack();
        Header h;
        ReadHeader(p, h);
        h.entry_count = 100000;
        WriteHeader(h, p);
        CHECK_FALSE(Validate(p));
    }
    SUBCASE("blob overlapping the entry table") {
        auto p = MakePack();
        Header h;
        ReadHeader(p, h);
        h.blob_offset = 8;   // inside the header
        WriteHeader(h, p);
        CHECK_FALSE(Validate(p));
    }
}

TEST_CASE("Fingerprints differ when inputs differ") {
    const std::vector<uint8_t> a{1, 2, 3};
    const std::vector<uint8_t> b{1, 2, 4};
    CHECK(Fingerprint(a) != Fingerprint(b));
    // Chained, as the builder mixes one source file at a time.
    CHECK(Fingerprint(b, Fingerprint(a)) != Fingerprint(a, Fingerprint(b)));
}

TEST_CASE("Asset pack encoding is explicitly little-endian") {
    const auto pack = MakePack();
    CHECK(pack[0] == 0x41);   // 'A'
    CHECK(pack[1] == 0x54);   // 'T'
    CHECK(pack[2] == 0x41);   // 'A'
    CHECK(pack[3] == 0x50);   // 'P'
    CHECK(pack[4] == kFormatVersion);
    CHECK(pack[5] == 0);
}
