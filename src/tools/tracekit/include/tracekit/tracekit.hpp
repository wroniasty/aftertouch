#pragma once
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/trace.hpp"

// Trace generate + diff for A6. Links at_core only — see
// doc/implementation/A6-test-infrastructure.md.

namespace at::tracekit {

struct Scenario {
    uint32_t                 seed       = 1;
    uint8_t                  first_team = 0;
    trace::Profile           profile    = trace::Profile::kAmiga;
    std::vector<MatchInput>  inputs;
};

// Optional hook: called after each Step, before the tick is serialised. Tests use
// this to inject a deliberate physics change and prove the golden fails.
using MutateFn = void (*)(MatchState&);

// Writes header + one record per input into out. Returns false if inputs empty.
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

struct DiffResult {
    bool        identical = true;
    uint32_t    tick      = 0;
    const char* reason    = nullptr;
};

inline DiffResult Diff(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    DiffResult r;
    if (a.size() < trace::kHeaderSize || b.size() < trace::kHeaderSize) {
        r.identical = false;
        r.reason    = "truncated header";
        return r;
    }

    trace::Header ha, hb;
    if (!trace::DeserializeHeader(a, ha) || !trace::DeserializeHeader(b, hb)) {
        r.identical = false;
        r.reason    = "bad header";
        return r;
    }
    if (ha.seed != hb.seed || ha.profile != hb.profile ||
        ha.first_team != hb.first_team || ha.tick_hz != hb.tick_hz) {
        r.identical = false;
        r.reason    = "header mismatch";
        return r;
    }
    if (ha.record_count != hb.record_count) {
        r.identical = false;
        r.reason    = "record count mismatch";
        return r;
    }

    const size_t need = trace::kHeaderSize +
                        static_cast<size_t>(ha.record_count) * trace::kRecordSize;
    if (a.size() < need || b.size() < need) {
        r.identical = false;
        r.reason    = "truncated body";
        return r;
    }

    for (uint32_t i = 0; i < ha.record_count; ++i) {
        const size_t off = trace::kHeaderSize + static_cast<size_t>(i) * trace::kRecordSize;
        const auto ra = a.subspan(off, trace::kRecordSize);
        const auto rb = b.subspan(off, trace::kRecordSize);
        // Compare payload bytes (excluding the trailing stored hash) so a stale
        // hash cannot hide a content change, then confirm the stored hashes match.
        const size_t payload = trace::kRecordSize - 8;
        bool diverge = false;
        for (size_t b_i = 0; b_i < payload; ++b_i) {
            if (ra[b_i] != rb[b_i]) {
                diverge = true;
                break;
            }
        }
        if (!diverge && trace::RecordHash(ra) != trace::RecordHash(rb)) diverge = true;
        if (diverge) {
            MatchState sa;
            MatchInput ia;
            uint32_t tick = i;
            if (trace::DeserializeRecord(ra, sa, ia)) tick = sa.tick;
            else {
                // Corrupted record: read tick field little-endian from the front.
                tick = uint32_t(ra[0]) | (uint32_t(ra[1]) << 8) | (uint32_t(ra[2]) << 16) |
                       (uint32_t(ra[3]) << 24);
            }
            r.identical = false;
            r.tick      = tick;
            r.reason    = "record divergence";
            return r;
        }
    }
    return r;
}

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

// The canonical Wave-1 scenario: fixed seed, 100 idle ticks. B parts add richer logs.
inline Scenario KickoffScenario(uint32_t ticks = 100) {
    Scenario s;
    s.seed       = 0xA5A50001u;
    s.first_team = 0;
    s.inputs.assign(ticks, MatchInput{});
    // A little structure so the log is not all zeroes once input starts mattering.
    if (ticks > 10) {
        s.inputs[10].p1.dir  = Dir::N;
        s.inputs[10].p1.fire = true;
        s.inputs[20].p2.dir  = Dir::SE;
    }
    return s;
}

} // namespace at::tracekit
