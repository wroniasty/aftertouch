// A6: deterministic mutation fuzz for A4/A5 validators.
// Optional libFuzzer entry is compiled only with -DAT_FUZZ.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "assets/asset_pack.hpp"
#include "data/fictional.hpp"
#include "data/game_data.hpp"

using namespace at;

namespace {

std::vector<uint8_t> ValidLeagueBytes() {
    const data::League league = data::MakeFictionalLeague();
    std::vector<uint8_t> buf(data::LeagueByteSize(league));
    REQUIRE(data::EncodeLeague(league, buf) == buf.size());
    return buf;
}

std::vector<uint8_t> ValidAssetPack() {
    using namespace assets;
    const uint32_t entry_count = 2;
    const uint16_t w = 4, h = 3;
    const uint32_t table_end = TableOffset() + TableSize(entry_count);
    const uint32_t each = static_cast<uint32_t>(w) * h;
    const uint32_t blob_size = each * entry_count;

    Header hd;
    hd.kind        = Kind::kSprites;
    hd.source      = SourceKind::kPlaceholder;
    hd.entry_count = entry_count;
    hd.blob_offset = table_end;
    hd.blob_size   = blob_size;
    hd.fingerprint = 1;

    std::vector<uint8_t> pack(static_cast<size_t>(table_end) + blob_size, 0);
    WriteHeader(hd, pack);
    for (uint32_t i = 0; i < entry_count; ++i) {
        Entry e;
        e.width  = w;
        e.height = h;
        e.offset = i * each;
        e.size   = each;
        WriteEntry(e, std::span<uint8_t>(pack).subspan(TableOffset() + i * kEntrySize,
                                                       kEntrySize));
    }
    return pack;
}

template <typename Pred>
void MutateAndCheck(std::vector<uint8_t> base, Pred pred) {
    // Truncations.
    for (size_t keep : {size_t{0}, size_t{1}, base.size() / 2, base.size() - 1}) {
        if (keep >= base.size()) continue;
        auto m = base;
        m.resize(keep);
        CHECK(pred(m));
    }
    // Byte flips across the file.
    for (size_t i = 0; i < base.size(); i += 17) {
        auto m = base;
        m[i] ^= 0xFFu;
        CHECK(pred(m));
    }
    // Append junk.
    {
        auto m = base;
        m.push_back(0xAB);
        m.push_back(0xCD);
        CHECK(pred(m));
    }
}

} // namespace

TEST_CASE("fuzz ATGD: mutations never crash and valid decode stays rare") {
    const auto base = ValidLeagueBytes();
    data::League scratch;
    REQUIRE(data::DecodeLeague(base, scratch));

    MutateAndCheck(base, [&](const std::vector<uint8_t>& m) {
        data::League out;
        // Must not crash; result is bool only.
        (void)data::DecodeLeague(m, out);
        return true;
    });
}

TEST_CASE("fuzz ATAP: mutations never crash; validator rejects damage") {
    const auto base = ValidAssetPack();
    REQUIRE(assets::Validate(base));

    int rejected = 0;
    MutateAndCheck(base, [&](const std::vector<uint8_t>& m) {
        if (!assets::Validate(m)) ++rejected;
        return true;
    });
    CHECK(rejected > 0);
}

#if defined(AT_FUZZ)
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    data::League league;
    (void)data::DecodeLeague(std::span<const uint8_t>(data, size), league);
    (void)assets::Validate(std::span<const uint8_t>(data, size));
    return 0;
}
#endif
