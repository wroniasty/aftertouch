#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/fixed.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"

// The trace record: one fixed-width, little-endian record per tick, plus a header.
// This is the format the whole project is measured with -- see
// doc/implementation/A3-trace-harness.md.
//
// Three properties are deliberate and none of them are free:
//
//  * PURE. No I/O, no allocation, no clock. The caller supplies the bytes and does
//    the writing, which is what lets the format live next to the state it encodes
//    without breaching Rule 1 (PLAN.md section 0).
//
//  * FIELD BY FIELD, NOT memcpy. The record is built from explicit byte writes, so
//    it does not depend on struct layout, compiler padding or host endianness. A
//    trace recorded on Windows is byte-identical to one recorded on macOS, and it
//    stays readable when B1 reshapes MatchState. Copying the object representation
//    would be shorter and would silently encode whatever the compiler left in the
//    padding.
//
//  * FIXED WIDTH. tick -> file offset is arithmetic rather than a scan, which is
//    what lets the differ binary-search a hundred thousand ticks for the first
//    divergence instead of decoding all of them.
//
// The hash covers the SERIALISED PAYLOAD rather than the state object. That is not
// a shortcut: hashing the object would require MatchState to have no padding (a
// guarantee it does not yet carry -- A2 work item 7), while hashing the payload is
// padding-independent by construction and is also what a reader can verify without
// reconstructing the state at all.

namespace at::trace {

// "ATTR", stored little-endian.
inline constexpr uint32_t kMagic = 0x52545441u;

// Version 1 encodes the fields MatchState has today. B1 fills the struct out --
// velocity, speed, heading, player state, the RNG streams -- and bumps this. The
// format is versioned before there is anything to lose precisely so that the bump
// is a normal event rather than a migration.
inline constexpr uint16_t kFormatVersion = 1;

enum class Profile : uint8_t { kAmiga = 0, kPc = 1 };

// Header, written once at the start of a trace file.
struct Header {
    uint32_t seed          = 0;
    uint32_t record_count  = 0;
    Profile  profile       = Profile::kAmiga;
    // Which team's decision pass runs on tick 0. Both sides of a diff must agree or
    // every decision is one tick out of step -- A3 section 2.4.
    uint8_t  first_team    = 0;
    uint8_t  tick_hz       = 50;
};

inline constexpr size_t kHeaderSize = 24;

// Per-entity: 6 fixed-point words plus two bytes.
inline constexpr size_t kEntitySize = 6 * 4 + 2;

// tick(4) + phase(1) + input(4) + score(2) + reserved(1) + entities + hash(8)
inline constexpr size_t kRecordSize =
    4 + 1 + 4 + 2 + 1 + kEntitySize * (1 + 22) + 8;

// ---------------------------------------------------------------------------
// Byte cursors. Explicit little-endian, bounds-checked by the caller through the
// span it passes in.
// ---------------------------------------------------------------------------

namespace detail {

constexpr void PutU8(std::span<uint8_t> b, size_t& at, uint8_t v) {
    b[at++] = v;
}
constexpr void PutU16(std::span<uint8_t> b, size_t& at, uint16_t v) {
    b[at++] = static_cast<uint8_t>(v & 0xFFu);
    b[at++] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}
constexpr void PutU32(std::span<uint8_t> b, size_t& at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at++] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
}
constexpr void PutU64(std::span<uint8_t> b, size_t& at, uint64_t v) {
    for (int i = 0; i < 8; ++i) b[at++] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
}
// Fixed point travels as its raw bit pattern, reinterpreted through uint32_t so no
// signed conversion is involved in either direction.
constexpr void PutFix(std::span<uint8_t> b, size_t& at, Fix v) {
    PutU32(b, at, static_cast<uint32_t>(v.Raw()));
}

constexpr uint8_t GetU8(std::span<const uint8_t> b, size_t& at) {
    return b[at++];
}
constexpr uint16_t GetU16(std::span<const uint8_t> b, size_t& at) {
    const uint16_t lo = b[at++];
    const uint16_t hi = b[at++];
    return static_cast<uint16_t>(lo | (hi << 8));
}
constexpr uint32_t GetU32(std::span<const uint8_t> b, size_t& at) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at++]) << (8 * i);
    return v;
}
constexpr uint64_t GetU64(std::span<const uint8_t> b, size_t& at) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(b[at++]) << (8 * i);
    return v;
}
constexpr Fix GetFix(std::span<const uint8_t> b, size_t& at) {
    return Fix::FromRaw(static_cast<int32_t>(GetU32(b, at)));
}

constexpr void PutEntity(std::span<uint8_t> b, size_t& at, const EntityState& e) {
    PutFix(b, at, e.pos.x);
    PutFix(b, at, e.pos.y);
    PutFix(b, at, e.pos.z);
    PutFix(b, at, e.vel.x);
    PutFix(b, at, e.vel.y);
    PutFix(b, at, e.vel.z);
    PutU8(b, at, e.anim_frame);
    PutU8(b, at, e.flags);
}

constexpr EntityState GetEntity(std::span<const uint8_t> b, size_t& at) {
    EntityState e;
    e.pos.x = GetFix(b, at);
    e.pos.y = GetFix(b, at);
    e.pos.z = GetFix(b, at);
    e.vel.x = GetFix(b, at);
    e.vel.y = GetFix(b, at);
    e.vel.z = GetFix(b, at);
    e.anim_frame = GetU8(b, at);
    e.flags      = GetU8(b, at);
    return e;
}

} // namespace detail

// FNV-1a over the payload. Not cryptographic and does not need to be -- it exists
// so the differ can reject a record whose bytes and hash disagree, and so a scan
// for the first divergence compares one word per tick instead of six hundred.
constexpr uint64_t HashBytes(std::span<const uint8_t> bytes) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

// Returns bytes written, or 0 if the span is too small.
constexpr size_t SerializeHeader(const Header& h, std::span<uint8_t> out) {
    if (out.size() < kHeaderSize) return 0;
    size_t at = 0;
    detail::PutU32(out, at, kMagic);
    detail::PutU16(out, at, kFormatVersion);
    detail::PutU16(out, at, static_cast<uint16_t>(kRecordSize));
    detail::PutU32(out, at, h.seed);
    detail::PutU32(out, at, h.record_count);
    detail::PutU8(out, at, static_cast<uint8_t>(h.profile));
    detail::PutU8(out, at, h.first_team);
    detail::PutU8(out, at, h.tick_hz);
    detail::PutU8(out, at, 0);            // reserved
    detail::PutU32(out, at, 0);           // reserved
    return at;
}

// Rejects a wrong magic, a version it does not understand, or a stride that
// disagrees with this build -- all three are silent misreads if not checked.
constexpr bool DeserializeHeader(std::span<const uint8_t> in, Header& h) {
    if (in.size() < kHeaderSize) return false;
    size_t at = 0;
    if (detail::GetU32(in, at) != kMagic) return false;
    if (detail::GetU16(in, at) != kFormatVersion) return false;
    if (detail::GetU16(in, at) != static_cast<uint16_t>(kRecordSize)) return false;
    h.seed         = detail::GetU32(in, at);
    h.record_count = detail::GetU32(in, at);
    h.profile      = static_cast<Profile>(detail::GetU8(in, at));
    h.first_team   = detail::GetU8(in, at);
    h.tick_hz      = detail::GetU8(in, at);
    return true;
}

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

// The input is written into the record as well as into the scenario's input log.
// A trace that does not carry the inputs that produced it cannot be replayed by
// anyone holding only the trace, and the first time that matters is the first bug
// report. The two copies disagreeing is itself a detectable error.
constexpr size_t SerializeRecord(const MatchState& s, const MatchInput& in,
                                 std::span<uint8_t> out) {
    if (out.size() < kRecordSize) return 0;
    size_t at = 0;
    detail::PutU32(out, at, s.tick);
    detail::PutU8(out, at, static_cast<uint8_t>(s.phase));
    detail::PutU8(out, at, static_cast<uint8_t>(in.p1.dir));
    detail::PutU8(out, at, static_cast<uint8_t>(in.p1.fire ? 1 : 0));
    detail::PutU8(out, at, static_cast<uint8_t>(in.p2.dir));
    detail::PutU8(out, at, static_cast<uint8_t>(in.p2.fire ? 1 : 0));
    detail::PutU8(out, at, s.score[0]);
    detail::PutU8(out, at, s.score[1]);
    detail::PutU8(out, at, 0);            // reserved

    detail::PutEntity(out, at, s.ball);
    for (const EntityState& p : s.players) detail::PutEntity(out, at, p);

    const uint64_t h = HashBytes(out.subspan(0, at));
    detail::PutU64(out, at, h);
    return at;
}

constexpr bool DeserializeRecord(std::span<const uint8_t> in, MatchState& s,
                                 MatchInput& input) {
    if (in.size() < kRecordSize) return false;
    size_t at = 0;
    s.tick     = detail::GetU32(in, at);
    s.phase    = static_cast<MatchPhase>(detail::GetU8(in, at));
    input.p1.dir  = static_cast<Dir>(detail::GetU8(in, at));
    input.p1.fire = detail::GetU8(in, at) != 0;
    input.p2.dir  = static_cast<Dir>(detail::GetU8(in, at));
    input.p2.fire = detail::GetU8(in, at) != 0;
    s.score[0] = detail::GetU8(in, at);
    s.score[1] = detail::GetU8(in, at);
    detail::GetU8(in, at);                // reserved

    s.ball = detail::GetEntity(in, at);
    for (EntityState& p : s.players) p = detail::GetEntity(in, at);

    const uint64_t want = HashBytes(in.subspan(0, at));
    const uint64_t got  = detail::GetU64(in, at);
    return want == got;
}

// The hash a record carries, without decoding it. This is what the differ scans.
constexpr uint64_t RecordHash(std::span<const uint8_t> record) {
    size_t at = kRecordSize - 8;
    return detail::GetU64(record, at);
}

} // namespace at::trace
