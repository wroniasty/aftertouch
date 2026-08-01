#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/trace.hpp"

// Trace generate + diff for A3/A6. Links at_core only — no SDL.
// See doc/implementation/A3-trace-harness.md.

namespace at::tracekit {

struct Scenario {
    uint32_t                 seed       = 1;
    uint8_t                  first_team = 0;
    trace::Profile           profile    = trace::Profile::kAmiga;
    std::vector<MatchInput>  inputs;
};

using MutateFn = void (*)(MatchState&);

inline bool Generate(const Scenario& scenario, std::vector<uint8_t>& out,
                     MutateFn mutate = nullptr) {
    if (scenario.inputs.empty()) return false;

    MatchEngine engine;
    engine.Reset(scenario.seed);

    out.clear();
    out.resize(trace::kHeaderSize + scenario.inputs.size() * trace::kRecordSize);

    trace::Header hdr;
    hdr.seed         = scenario.seed;
    hdr.record_count = static_cast<uint32_t>(scenario.inputs.size());
    hdr.profile      = scenario.profile;
    hdr.first_team   = scenario.first_team;
    hdr.tick_hz      = MatchEngine::kTickHz;
    if (trace::SerializeHeader(hdr, out) != trace::kHeaderSize) return false;

    size_t at = trace::kHeaderSize;
    for (const MatchInput& in : scenario.inputs) {
        engine.Step(in);
        MatchState state = engine.State();
        if (mutate) mutate(state);
        if (trace::SerializeRecord(state, in, std::span<uint8_t>(out).subspan(at)) !=
            trace::kRecordSize) {
            return false;
        }
        at += trace::kRecordSize;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Divergence: zero tolerance, field classes, drift profile (A3 §2.6)
// ---------------------------------------------------------------------------

enum class FieldClass : uint8_t {
    Header,
    Input,
    Phase,
    Score,
    Position,   // entity pos / delta Fix words
    EntityMeta, // remaining entity fields
    Side,       // sheet / control / squad / tactics
    Globals,
    Clock,
    Rng,
    Hash,
    Unknown,
};

inline const char* FieldClassName(FieldClass c) {
    switch (c) {
    case FieldClass::Header:     return "header";
    case FieldClass::Input:      return "input";
    case FieldClass::Phase:      return "phase";
    case FieldClass::Score:      return "score";
    case FieldClass::Position:   return "position";
    case FieldClass::EntityMeta: return "entity_meta";
    case FieldClass::Side:       return "side";
    case FieldClass::Globals:    return "globals";
    case FieldClass::Clock:      return "clock";
    case FieldClass::Rng:        return "rng";
    case FieldClass::Hash:       return "hash";
    default:                     return "unknown";
    }
}

// Classify a byte offset inside a record (0 .. kRecordSize-1). ATTR v2 layout.
inline FieldClass ClassifyRecordOffset(size_t off) {
    if (off < 4) return FieldClass::Unknown; // tick
    if (off == 4) return FieldClass::Phase;
    if (off >= 5 && off < 9) return FieldClass::Input;
    if (off >= 9 && off < 11) return FieldClass::Score;
    if (off == 11) return FieldClass::Unknown; // last_roll
    if (off >= trace::kRecordSize - 8) return FieldClass::Hash;

    const size_t arena_bytes = trace::kArenaEntityCount * trace::kEntityWireSize;
    const size_t arena_end   = trace::kRecordPrefixSize + arena_bytes;
    if (off < arena_end) {
        const size_t body   = off - trace::kRecordPrefixSize;
        const size_t within = body % trace::kEntityWireSize;
        if (within < 24) return FieldClass::Position; // 6 × Fix
        return FieldClass::EntityMeta;
    }

    const size_t sides_end = arena_end + 2 * trace::kSideWireSize;
    if (off < sides_end) return FieldClass::Side;

    const size_t globals_end = sides_end + trace::kGlobalsWireSize;
    if (off < globals_end) return FieldClass::Globals;

    const size_t clock_end = globals_end + trace::kClockWireSize;
    if (off < clock_end) return FieldClass::Clock;

    const size_t surface_end = clock_end + trace::kSurfaceWireSize;
    if (off < surface_end) return FieldClass::Globals; // surface coeffs

    if (off < surface_end + 12) return FieldClass::Rng;
    return FieldClass::Unknown;
}

struct DriftSample {
    uint32_t tick          = 0;
    uint64_t l1_position   = 0; // sum |Δraw| over Fix fields that differ
    uint32_t bytes_differ  = 0;
};

struct DiffResult {
    bool        identical   = true;
    uint32_t    tick        = 0;
    const char* reason      = nullptr;
    FieldClass  first_class = FieldClass::Unknown;
    size_t      first_byte  = 0;
    std::vector<DriftSample> drift; // from first divergence to end (inclusive)
};

inline uint32_t ReadTickLE(std::span<const uint8_t> rec) {
    return uint32_t(rec[0]) | (uint32_t(rec[1]) << 8) | (uint32_t(rec[2]) << 16) |
           (uint32_t(rec[3]) << 24);
}

inline uint64_t PositionL1(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    // Entity Fix fields start at record offset 12; 23 entities × 6 Fix × 4 bytes.
    uint64_t sum = 0;
    constexpr size_t kFixBytes = 23 * 6 * 4;
    for (size_t i = 0; i < kFixBytes; i += 4) {
        const size_t off = 12 + i;
        uint32_t va = uint32_t(a[off]) | (uint32_t(a[off + 1]) << 8) |
                      (uint32_t(a[off + 2]) << 16) | (uint32_t(a[off + 3]) << 24);
        uint32_t vb = uint32_t(b[off]) | (uint32_t(b[off + 1]) << 8) |
                      (uint32_t(b[off + 2]) << 16) | (uint32_t(b[off + 3]) << 24);
        const int64_t da = static_cast<int32_t>(va);
        const int64_t db = static_cast<int32_t>(vb);
        int64_t d = da - db;
        if (d < 0) d = -d;
        sum += static_cast<uint64_t>(d);
    }
    return sum;
}

inline DiffResult Diff(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    DiffResult r;
    if (a.size() < trace::kHeaderSize || b.size() < trace::kHeaderSize) {
        r.identical   = false;
        r.reason      = "truncated header";
        r.first_class = FieldClass::Header;
        return r;
    }

    trace::Header ha, hb;
    if (!trace::DeserializeHeader(a, ha) || !trace::DeserializeHeader(b, hb)) {
        r.identical   = false;
        r.reason      = "bad header";
        r.first_class = FieldClass::Header;
        return r;
    }
    if (ha.seed != hb.seed || ha.profile != hb.profile ||
        ha.first_team != hb.first_team || ha.tick_hz != hb.tick_hz) {
        r.identical   = false;
        r.reason      = "header mismatch";
        r.first_class = FieldClass::Header;
        return r;
    }
    if (ha.record_count != hb.record_count) {
        r.identical   = false;
        r.reason      = "record count mismatch";
        r.first_class = FieldClass::Header;
        return r;
    }

    const size_t need = trace::kHeaderSize +
                        static_cast<size_t>(ha.record_count) * trace::kRecordSize;
    if (a.size() < need || b.size() < need) {
        r.identical   = false;
        r.reason      = "truncated body";
        r.first_class = FieldClass::Header;
        return r;
    }

    bool found = false;
    for (uint32_t i = 0; i < ha.record_count; ++i) {
        const size_t off = trace::kHeaderSize + static_cast<size_t>(i) * trace::kRecordSize;
        const auto ra = a.subspan(off, trace::kRecordSize);
        const auto rb = b.subspan(off, trace::kRecordSize);

        size_t first_diff = trace::kRecordSize;
        uint32_t bytes_differ = 0;
        for (size_t b_i = 0; b_i < trace::kRecordSize; ++b_i) {
            if (ra[b_i] != rb[b_i]) {
                if (first_diff == trace::kRecordSize) first_diff = b_i;
                ++bytes_differ;
            }
        }
        if (bytes_differ == 0) continue;

        const uint32_t tick = ReadTickLE(ra);
        if (!found) {
            found             = true;
            r.identical       = false;
            r.tick            = tick;
            r.reason          = "record divergence";
            r.first_byte      = first_diff;
            r.first_class     = ClassifyRecordOffset(first_diff);
        }
        DriftSample sample;
        sample.tick         = tick;
        sample.bytes_differ = bytes_differ;
        sample.l1_position  = PositionL1(ra, rb);
        r.drift.push_back(sample);
    }
    return r;
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

inline bool ReadFile(const char* path, std::vector<uint8_t>& out) {
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
    if (sz < 0) {
        std::fclose(f);
        return false;
    }
    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    const size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return n == out.size();
}

inline bool WriteFile(const char* path, std::span<const uint8_t> bytes) {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0) return false;
#else
    f = std::fopen(path, "wb");
    if (!f) return false;
#endif
    const size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

// ---------------------------------------------------------------------------
// Input log (.atin) — committed corpus driver (A3 §2.7)
// ---------------------------------------------------------------------------

inline constexpr uint32_t kAtinMagic = 0x4E495441u; // "ATIN" LE
inline constexpr uint16_t kAtinVersion = 1;

inline bool SerializeInputLog(const Scenario& s, std::vector<uint8_t>& out) {
    out.clear();
    out.resize(16 + s.inputs.size() * 4);
    size_t at = 0;
    auto put_u8 = [&](uint8_t v) { out[at++] = v; };
    auto put_u16 = [&](uint16_t v) {
        out[at++] = static_cast<uint8_t>(v & 0xFF);
        out[at++] = static_cast<uint8_t>((v >> 8) & 0xFF);
    };
    auto put_u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) out[at++] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
    };
    put_u32(kAtinMagic);
    put_u16(kAtinVersion);
    put_u8(static_cast<uint8_t>(s.profile));
    put_u8(s.first_team);
    put_u32(s.seed);
    put_u32(static_cast<uint32_t>(s.inputs.size()));
    for (const MatchInput& in : s.inputs) {
        put_u8(static_cast<uint8_t>(static_cast<int8_t>(in.p1.dir)));
        put_u8(static_cast<uint8_t>(in.p1.fire ? 1 : 0));
        put_u8(static_cast<uint8_t>(static_cast<int8_t>(in.p2.dir)));
        put_u8(static_cast<uint8_t>(in.p2.fire ? 1 : 0));
    }
    return at == out.size();
}

inline bool DeserializeInputLog(std::span<const uint8_t> in, Scenario& s) {
    if (in.size() < 16) return false;
    size_t at = 0;
    auto get_u8 = [&]() { return in[at++]; };
    auto get_u16 = [&]() {
        const uint16_t lo = in[at++];
        const uint16_t hi = in[at++];
        return static_cast<uint16_t>(lo | (hi << 8));
    };
    auto get_u32 = [&]() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint32_t(in[at++]) << (8 * i);
        return v;
    };
    if (get_u32() != kAtinMagic) return false;
    if (get_u16() != kAtinVersion) return false;
    s.profile    = static_cast<trace::Profile>(get_u8());
    s.first_team = get_u8();
    s.seed       = get_u32();
    const uint32_t count = get_u32();
    if (in.size() < 16 + static_cast<size_t>(count) * 4) return false;
    s.inputs.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        s.inputs[i].p1.dir  = static_cast<Dir>(static_cast<int8_t>(get_u8()));
        s.inputs[i].p1.fire = get_u8() != 0;
        s.inputs[i].p2.dir  = static_cast<Dir>(static_cast<int8_t>(get_u8()));
        s.inputs[i].p2.fire = get_u8() != 0;
    }
    return true;
}

// Hash chain over per-record payload hashes — proves a regenerated reference
// matches the corpus definition without committing the full binary every time.
inline uint64_t HashChain(std::span<const uint8_t> attr) {
    trace::Header h;
    if (!trace::DeserializeHeader(attr, h)) return 0;
    const size_t need =
        trace::kHeaderSize + static_cast<size_t>(h.record_count) * trace::kRecordSize;
    if (attr.size() < need) return 0;
    uint64_t chain = 1469598103934665603ull;
    for (uint32_t i = 0; i < h.record_count; ++i) {
        const size_t off = trace::kHeaderSize + static_cast<size_t>(i) * trace::kRecordSize;
        const uint64_t rh = trace::RecordHash(attr.subspan(off, trace::kRecordSize));
        chain ^= rh;
        chain *= 1099511628211ull;
    }
    return chain;
}

// Wave-1 stub oracle: same inputs/seed as the engine, ball.x bumped by 1 raw
// unit every tick so a clone without the reference checkout still exercises a
// real diverge-at-tick-1 path (A3 §5). Replaced by instrumented SWOS when present.
inline void StubOracleMutate(MatchState& s) {
    s.ball.pos.x = Fix::FromRaw(s.ball.pos.x.Raw() + 1);
}

inline Scenario KickoffScenario(uint32_t ticks = 100) {
    Scenario s;
    s.seed       = 0xA5A50001u;
    s.first_team = 0;
    s.inputs.assign(ticks, MatchInput{});
    if (ticks > 10) {
        s.inputs[10].p1.dir  = Dir::N;
        s.inputs[10].p1.fire = true;
    }
    if (ticks > 20) {
        s.inputs[20].p2.dir = Dir::SE;
    }
    return s;
}

// Eight-way jog + fire hold — exercises the aftertouch-shaped input channel before
// B6 exists. Seed distinct from kickoff.
inline Scenario ShotCurlScenario(uint32_t ticks = 80) {
    Scenario s;
    s.seed       = 0xA5A50002u;
    s.first_team = 0;
    s.inputs.assign(ticks, MatchInput{});
    for (uint32_t i = 0; i < ticks; ++i) {
        if (i >= 5 && i < 40) {
            s.inputs[i].p1.dir  = Dir::NE;
            s.inputs[i].p1.fire = true;
        } else if (i >= 40 && i < 60) {
            s.inputs[i].p1.dir = Dir::E;
        }
    }
    return s;
}

} // namespace at::tracekit
