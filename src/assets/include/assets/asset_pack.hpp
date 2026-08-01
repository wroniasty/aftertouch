#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

// The runtime asset container -- see doc/implementation/A4-asset-pipeline.md section 2.4.
//
// One format serves every pack (sprites, pitches, palettes) rather than six bespoke
// ones. Same discipline as core/trace.hpp, and for the same reasons:
//
//  * PURE. Encoding and decoding operate on byte spans. The caller does the file I/O.
//    That is what lets one definition be shared by an offline tool that writes packs
//    and a runtime that reads them, without either depending on the other.
//
//  * OFFSETS, NOT POINTERS; FIXED-WIDTH TABLES. A pack is usable as soon as its bytes
//    are in memory. There is no parse step and no fix-up pass.
//
//  * LITTLE-ENDIAN BY EXPLICIT BYTE WRITES. Not by struct layout, so a pack built on
//    one platform loads on the other.
//
//  * FINGERPRINTED. The header records which source produced the pack and a hash of
//    the inputs, so a stale assets/generated/ after a source change is detected rather
//    than silently used.
//
// Pixels are stored INDEXED at ORIGINAL resolution. Original resolution because
// pixel-identical comparison against the reference during trace-diffing is the whole
// reason A4 sits in Wave 1 (PLAN.md section 10). Indexed because that is what the art
// was before the reference's pipeline expanded it, and it keeps a pitch under a
// hundred kilobytes.

namespace at::assets {

// "ATAP", little-endian.
inline constexpr uint32_t kMagic = 0x50415441u;
inline constexpr uint16_t kFormatVersion = 1;

enum class Kind : uint16_t {
    kSprites = 0,   // a sprite bank; each entry is one frame
    kPitch   = 1,   // tile bank; aux holds the tile index matrix
    kPalette = 2,   // one entry, the palette itself
};

// Recorded so that "where did these bytes come from" is answerable from the file.
// A pack built from the reference's extracted tree is a bootstrap and carries
// resampling artefacts; one built from an original installation does not. Anything
// that claims to be the original's pixels must be able to tell them apart.
enum class SourceKind : uint8_t {
    kPlaceholder = 0,
    kRefTree     = 1,
    kOriginal    = 2,
};

inline constexpr size_t kHeaderSize = 48;
inline constexpr size_t kEntrySize  = 16;

struct Header {
    Kind       kind         = Kind::kSprites;
    uint32_t   entry_count  = 0;
    uint32_t   blob_offset  = 0;
    uint32_t   blob_size    = 0;
    uint32_t   aux_offset   = 0;
    uint32_t   aux_size     = 0;
    // Grid dimensions for the aux section. For kPitch this is the tile index matrix.
    uint16_t   aux_w        = 0;
    uint16_t   aux_h        = 0;
    SourceKind source       = SourceKind::kPlaceholder;
    uint64_t   fingerprint  = 0;
};

// One frame or tile. Anchors are signed: a sprite's visual centre can sit outside
// its own bounding box once trimmed.
struct Entry {
    uint16_t width    = 0;
    uint16_t height   = 0;
    int16_t  anchor_x = 0;
    int16_t  anchor_y = 0;
    uint32_t offset   = 0;   // relative to Header::blob_offset
    uint32_t size     = 0;
};

inline constexpr uint32_t TableOffset() {
    return static_cast<uint32_t>(kHeaderSize);
}
inline constexpr uint32_t TableSize(uint32_t entry_count) {
    return entry_count * static_cast<uint32_t>(kEntrySize);
}

// ---------------------------------------------------------------------------

namespace detail {

constexpr void PutU8(std::span<uint8_t> b, size_t& at, uint8_t v) { b[at++] = v; }
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

constexpr uint8_t GetU8(std::span<const uint8_t> b, size_t& at) { return b[at++]; }
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

} // namespace detail

constexpr uint64_t Fingerprint(std::span<const uint8_t> bytes, uint64_t seed = 1469598103934665603ull) {
    uint64_t h = seed;
    for (uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

// Returns bytes written, or 0 if the span is too small.
constexpr size_t WriteHeader(const Header& h, std::span<uint8_t> out) {
    if (out.size() < kHeaderSize) return 0;
    size_t at = 0;
    detail::PutU32(out, at, kMagic);
    detail::PutU16(out, at, kFormatVersion);
    detail::PutU16(out, at, static_cast<uint16_t>(h.kind));
    detail::PutU32(out, at, h.entry_count);
    detail::PutU32(out, at, TableOffset());
    detail::PutU32(out, at, h.blob_offset);
    detail::PutU32(out, at, h.blob_size);
    detail::PutU32(out, at, h.aux_offset);
    detail::PutU32(out, at, h.aux_size);
    detail::PutU16(out, at, h.aux_w);
    detail::PutU16(out, at, h.aux_h);
    detail::PutU8(out, at, static_cast<uint8_t>(h.source));
    detail::PutU8(out, at, 0);
    detail::PutU16(out, at, 0);
    detail::PutU64(out, at, h.fingerprint);
    return at;
}

// Rejects a wrong magic or an unknown version. Both are silent misreads otherwise,
// and a silent misread of an asset pack looks like corrupt art rather than a bug.
constexpr bool ReadHeader(std::span<const uint8_t> in, Header& h) {
    if (in.size() < kHeaderSize) return false;
    size_t at = 0;
    if (detail::GetU32(in, at) != kMagic) return false;
    if (detail::GetU16(in, at) != kFormatVersion) return false;
    h.kind        = static_cast<Kind>(detail::GetU16(in, at));
    h.entry_count = detail::GetU32(in, at);
    if (detail::GetU32(in, at) != TableOffset()) return false;
    h.blob_offset = detail::GetU32(in, at);
    h.blob_size   = detail::GetU32(in, at);
    h.aux_offset  = detail::GetU32(in, at);
    h.aux_size    = detail::GetU32(in, at);
    h.aux_w       = detail::GetU16(in, at);
    h.aux_h       = detail::GetU16(in, at);
    h.source      = static_cast<SourceKind>(detail::GetU8(in, at));
    at += 3;
    h.fingerprint = detail::GetU64(in, at);
    return true;
}

constexpr size_t WriteEntry(const Entry& e, std::span<uint8_t> out) {
    if (out.size() < kEntrySize) return 0;
    size_t at = 0;
    detail::PutU16(out, at, e.width);
    detail::PutU16(out, at, e.height);
    detail::PutU16(out, at, static_cast<uint16_t>(e.anchor_x));
    detail::PutU16(out, at, static_cast<uint16_t>(e.anchor_y));
    detail::PutU32(out, at, e.offset);
    detail::PutU32(out, at, e.size);
    return at;
}

constexpr bool ReadEntry(std::span<const uint8_t> in, Entry& e) {
    if (in.size() < kEntrySize) return false;
    size_t at = 0;
    e.width    = detail::GetU16(in, at);
    e.height   = detail::GetU16(in, at);
    e.anchor_x = static_cast<int16_t>(detail::GetU16(in, at));
    e.anchor_y = static_cast<int16_t>(detail::GetU16(in, at));
    e.offset   = detail::GetU32(in, at);
    e.size     = detail::GetU32(in, at);
    return true;
}

// A whole-pack structural check. Every offset and length must land inside the file
// and every entry inside the blob. This is the guard that turns a truncated or
// adversarial pack into a rejection instead of an out-of-bounds read -- the importer
// reads third-party binary data, and PLAN.md section 7 is explicit that a silent
// misparse there is worse than a crash.
inline bool Validate(std::span<const uint8_t> pack) {
    Header h;
    if (!ReadHeader(pack, h)) return false;

    const size_t table_end = TableOffset() + static_cast<size_t>(TableSize(h.entry_count));
    if (table_end > pack.size()) return false;
    if (static_cast<size_t>(h.blob_offset) + h.blob_size > pack.size()) return false;
    if (h.blob_offset < table_end) return false;
    if (h.aux_size != 0 &&
        static_cast<size_t>(h.aux_offset) + h.aux_size > pack.size()) return false;

    for (uint32_t i = 0; i < h.entry_count; ++i) {
        Entry e;
        if (!ReadEntry(pack.subspan(TableOffset() + i * kEntrySize, kEntrySize), e))
            return false;
        if (static_cast<size_t>(e.offset) + e.size > h.blob_size) return false;
        // An entry's declared size must match its declared dimensions; a mismatch
        // means one of the two is a lie and the pixels cannot be indexed safely.
        if (static_cast<size_t>(e.width) * e.height != e.size) return false;
    }
    return true;
}

// Convenience readers. Callers are expected to have run Validate() first; these do
// the bounds arithmetic but assume the pack is structurally sound.
inline Entry EntryAt(std::span<const uint8_t> pack, uint32_t i) {
    Entry e;
    ReadEntry(pack.subspan(TableOffset() + i * kEntrySize, kEntrySize), e);
    return e;
}

inline std::span<const uint8_t> Pixels(std::span<const uint8_t> pack, const Header& h,
                                       const Entry& e) {
    return pack.subspan(static_cast<size_t>(h.blob_offset) + e.offset, e.size);
}

inline std::span<const uint8_t> Aux(std::span<const uint8_t> pack, const Header& h) {
    return pack.subspan(h.aux_offset, h.aux_size);
}

} // namespace at::assets
