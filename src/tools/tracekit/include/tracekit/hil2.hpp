#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

// Minimal HIL2 reader — viewer smoke-test and independent positional cross-check.
// Not the ATTR trace format. See doc/implementation/A3-trace-harness.md §2.2
// and doc/DATA.md §6 / reference docs/highlights.txt.

namespace at::hil2 {

inline constexpr uint32_t kMagic = 0x324C4948u; // "HIL2" LE
inline constexpr size_t   kHeaderBytes = 3512;  // through padding

struct Header {
    uint16_t version_major = 0;
    uint16_t version_minor = 0;
    uint32_t scene_data_offset = 0;
    uint16_t scene_count = 0;
    uint16_t goals_team1 = 0;
    uint16_t goals_team2 = 0;
    uint8_t  pitch_type = 0;
    uint8_t  pitch_file = 0;
    uint16_t max_substitutes = 0;
    char     game_name[41]{};
    char     game_round[41]{};
};

struct SceneRange {
    uint32_t start = 0; // relative to scene buffer
    uint32_t end   = 0;
};

struct FrameInfo {
    uint32_t offset = 0; // relative to scene buffer
    uint32_t next   = 0;
    uint32_t prev   = 0;
    int32_t  camera_x = 0;
    int32_t  camera_y = 0;
    uint16_t goals1 = 0;
    uint16_t goals2 = 0;
};

struct File {
    Header                  header;
    std::vector<SceneRange> scenes;
    std::vector<FrameInfo>  frames; // all scenes concatenated, in file order
    std::vector<uint8_t>    raw;
};

namespace detail {

inline uint16_t Ru16(std::span<const uint8_t> b, size_t at) {
    return static_cast<uint16_t>(b[at] | (uint16_t(b[at + 1]) << 8));
}
inline uint32_t Ru32(std::span<const uint8_t> b, size_t at) {
    return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) |
           (uint32_t(b[at + 3]) << 24);
}
inline int32_t Ri32(std::span<const uint8_t> b, size_t at) {
    return static_cast<int32_t>(Ru32(b, at));
}

} // namespace detail

inline bool Parse(std::span<const uint8_t> bytes, File& out) {
    out = File{};
    out.raw.assign(bytes.begin(), bytes.end());
    if (bytes.size() < kHeaderBytes) return false;
    if (detail::Ru32(bytes, 0) != kMagic) return false;

    out.header.version_major     = detail::Ru16(bytes, 4);
    out.header.version_minor     = detail::Ru16(bytes, 6);
    out.header.scene_data_offset = detail::Ru32(bytes, 8);
    std::memcpy(out.header.game_name, bytes.data() + 3420, 40);
    std::memcpy(out.header.game_round, bytes.data() + 3460, 40);
    out.header.game_name[40]  = '\0';
    out.header.game_round[40] = '\0';
    out.header.scene_count    = detail::Ru16(bytes, 3500);
    out.header.goals_team1    = detail::Ru16(bytes, 3502);
    out.header.goals_team2    = detail::Ru16(bytes, 3504);
    out.header.pitch_type     = bytes[3506];
    out.header.pitch_file     = bytes[3507];
    out.header.max_substitutes = detail::Ru16(bytes, 3508);

    if (out.header.scene_data_offset > bytes.size()) return false;
    if (out.header.scene_count == 0) return true;

    const size_t buf = out.header.scene_data_offset;
    const size_t table_bytes =
        static_cast<size_t>(out.header.scene_count) * 8u;
    if (buf + table_bytes > bytes.size()) return false;

    out.scenes.resize(out.header.scene_count);
    for (uint16_t i = 0; i < out.header.scene_count; ++i) {
        out.scenes[i].start = detail::Ru32(bytes, buf + i * 8u);
        out.scenes[i].end   = detail::Ru32(bytes, buf + i * 8u + 4);
        if (out.scenes[i].end < out.scenes[i].start) return false;
        if (buf + out.scenes[i].end > bytes.size()) return false;
    }

    // Walk each scene's frame chain via next-frame offsets.
    for (const SceneRange& sc : out.scenes) {
        if (sc.start == sc.end) continue;
        uint32_t at = sc.start;
        // Guard against cycles / corruption.
        for (int guard = 0; guard < 1'000'000; ++guard) {
            if (at + 24 > sc.end) break;
            const size_t abs = buf + at;
            FrameInfo fr;
            fr.offset   = at;
            fr.next     = detail::Ru32(bytes, abs + 0);
            fr.prev     = detail::Ru32(bytes, abs + 4);
            fr.camera_x = detail::Ri32(bytes, abs + 8);
            fr.camera_y = detail::Ri32(bytes, abs + 12);
            fr.goals1   = detail::Ru16(bytes, abs + 16);
            fr.goals2   = detail::Ru16(bytes, abs + 18);
            out.frames.push_back(fr);
            if (fr.next <= at || fr.next > sc.end) break;
            if (fr.next == sc.end) break;
            at = fr.next;
        }
    }
    return true;
}

// Build a minimal one-scene, one-frame HIL2 for tests (no sprites).
inline std::vector<uint8_t> MakeMinimalFixture() {
    constexpr uint32_t kSceneOff = 3512;
    constexpr uint32_t kFrameRel = 8;   // after one start/end pair
    constexpr uint32_t kFrameEnd = kFrameRel + 24;
    std::vector<uint8_t> b(kSceneOff + kFrameEnd, 0);

    auto wu16 = [&](size_t at, uint16_t v) {
        b[at]     = static_cast<uint8_t>(v & 0xFF);
        b[at + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    };
    auto wu32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) b[at + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
    };

    wu32(0, kMagic);
    wu16(4, 2); // major
    wu16(6, 0); // minor
    wu32(8, kSceneOff);
    std::memcpy(b.data() + 3420, "FRIENDLY", 8);
    std::memcpy(b.data() + 3460, "FIRST ROUND", 11);
    wu16(3500, 1); // scenes
    wu16(3502, 0);
    wu16(3504, 0);
    b[3506] = 1;
    b[3507] = 1;
    wu16(3508, 2);

    wu32(kSceneOff + 0, kFrameRel);
    wu32(kSceneOff + 4, kFrameEnd);
    // Frame
    wu32(kSceneOff + kFrameRel + 0, kFrameEnd); // next
    wu32(kSceneOff + kFrameRel + 4, kFrameRel); // prev
    wu32(kSceneOff + kFrameRel + 8, 100);       // camera x
    wu32(kSceneOff + kFrameRel + 12, 200);      // camera y
    return b;
}

} // namespace at::hil2
