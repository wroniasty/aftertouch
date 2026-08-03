#include "swos_sprites.hpp"

#include <fstream>

namespace at::assetc {

namespace fs = std::filesystem;

namespace {

bool ReadFile(const fs::path& p, std::vector<uint8_t>& out, std::string& err) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) {
        err = "cannot open " + p.string();
        return false;
    }
    const std::streamsize n = f.tellg();
    f.seekg(0);
    out.resize(static_cast<size_t>(n));
    if (n > 0 && !f.read(reinterpret_cast<char*>(out.data()), n)) {
        err = "short read on " + p.string();
        return false;
    }
    return true;
}

uint16_t U16(const std::vector<uint8_t>& b, size_t at) {
    return static_cast<uint16_t>(b[at] | (b[at + 1] << 8));
}
uint32_t U32(const std::vector<uint8_t>& b, size_t at) {
    return static_cast<uint32_t>(b[at]) | (static_cast<uint32_t>(b[at + 1]) << 8) |
           (static_cast<uint32_t>(b[at + 2]) << 16) |
           (static_cast<uint32_t>(b[at + 3]) << 24);
}

bool ReadHeaderAt(const std::vector<uint8_t>& file, size_t at, SwosSpriteHeader& h) {
    if (at + kSwosSpriteHeaderSize > file.size()) return false;
    h.pixel_offset = U32(file, at + 0);
    h.width        = U16(file, at + 10);
    h.nlines       = U16(file, at + 12);
    h.wquads       = U16(file, at + 14);
    h.centre_x     = static_cast<int16_t>(U16(file, at + 16));
    h.centre_y     = static_cast<int16_t>(U16(file, at + 18));
    h.ordinal      = U16(file, at + 22);
    return true;
}

// sprite.dat's 1334 offsets divide into seven bands, each backed by one file. The
// first 227 offsets are relative to charset.dat; the remaining 1107 are relative to a
// joint memory block formed by concatenating the rest in this order.
//
// Two bands are DUPLICATE SLOTS. The original loads two team files and two copies of
// the goalkeepers -- one per playing side -- so bands 3 and 5 point at a second copy
// of the same structures. Their headers therefore carry the FIRST copy's ordinal:
// sprite 644 self-identifies as 341, and sprite 1063 as 947. Measured, not assumed.
// A verifier that demands ordinal == index scores 0 on those bands and looks like a
// parsing failure when nothing is wrong.
struct Band {
    const char* file;
    size_t      first;
    size_t      count;
    size_t      ordinal_bias;   // subtract before comparing a header's ordinal
    bool        in_joint_block;
    // bench.dat alone stores its pixel pointers relative to the joint block rather
    // than to its own file -- docs/SWOS/sprites.txt calls this out explicitly, and
    // without it every bench sprite decodes to nothing.
    bool        pixels_joint_relative;
};

constexpr Band kBands[] = {
    {"charset.dat", 0,    227, 0,   false, false},
    {"score.dat",   227,  114, 0,   true,  false},
    {nullptr,       341,  303, 0,   true,  false},  // team slot A -- file chosen below
    {nullptr,       644,  303, 303, true,  false},  // team slot B -- duplicate ordinals
    {"goal1.dat",   947,  116, 0,   true,  false},
    {"goal1.dat",   1063, 116, 116, true,  false},  // duplicate ordinals
    {"bench.dat",   1179, 155, 0,   true,  true },
};

constexpr size_t kBandCount = sizeof(kBands) / sizeof(kBands[0]);

} // namespace

bool LoadSwosPalette(const fs::path& swos_dir, std::array<Rgba, 256>& out,
                     std::string& err) {
    std::vector<uint8_t> pal;
    if (!ReadFile(swos_dir / "pal.256", pal, err)) return false;

    constexpr size_t kPaletteOffset = 0xFA00;
    if (pal.size() < kPaletteOffset + 256 * 3) {
        err = "pal.256 is too small to contain a palette at 0xFA00";
        return false;
    }
    for (int i = 0; i < 256; ++i) {
        const uint8_t r = pal[kPaletteOffset + i * 3 + 0];
        const uint8_t g = pal[kPaletteOffset + i * 3 + 1];
        const uint8_t b = pal[kPaletteOffset + i * 3 + 2];
        if (r > 63 || g > 63 || b > 63) {
            err = "pal.256 entry " + std::to_string(i) +
                  " exceeds the 6-bit VGA range; wrong offset or wrong file";
            return false;
        }
        // 6 bits to 8, so that 63 becomes 255 rather than 252.
        auto expand = [](uint8_t v) {
            return static_cast<uint8_t>((v << 2) | (v >> 4));
        };
        out[static_cast<size_t>(i)] = Rgba{expand(r), expand(g), expand(b), 255};
    }
    return true;
}

std::vector<uint8_t> DecodePlanar(const std::vector<uint8_t>& file,
                                  const SwosSpriteHeader& h) {
    std::vector<uint8_t> px;
    const size_t bytes_per_line = static_cast<size_t>(h.wquads) * 8;
    const size_t plane_bytes    = static_cast<size_t>(h.wquads) * 2;
    if (bytes_per_line == 0 || h.width == 0 || h.nlines == 0) return px;

    const size_t need = static_cast<size_t>(h.pixel_offset) + bytes_per_line * h.nlines;
    if (need > file.size()) return px;

    px.assign(static_cast<size_t>(h.width) * h.nlines, 0);
    for (uint16_t y = 0; y < h.nlines; ++y) {
        const size_t line = h.pixel_offset + static_cast<size_t>(y) * bytes_per_line;
        for (uint16_t x = 0; x < h.width; ++x) {
            const size_t byte = x / 8;
            const int    bit  = 7 - (x % 8);
            if (byte >= plane_bytes) break;
            uint8_t v = 0;
            for (int p = 0; p < 4; ++p) {
                const uint8_t plane_byte = file[line + p * plane_bytes + byte];
                v = static_cast<uint8_t>(v | (((plane_byte >> bit) & 1) << p));
            }
            px[static_cast<size_t>(y) * h.width + x] = v;
        }
    }
    return px;
}

Image ToRgba(const SwosSprite& s, const std::array<Rgba, 256>& palette) {
    Image img;
    img.width  = s.header.width;
    img.height = s.header.nlines;
    img.pixels.resize(s.indices.size());
    for (size_t i = 0; i < s.indices.size(); ++i) {
        const uint8_t idx = s.indices[i];
        img.pixels[i] = idx == 0 ? Rgba{0, 0, 0, 0} : palette[idx];
    }
    return img;
}

bool LoadSwosSprites(const fs::path& swos_dir, SwosSpriteSet& out, std::string& err) {
    if (!LoadSwosPalette(swos_dir, out.palette, err)) return false;

    std::vector<uint8_t> index_file;
    if (!ReadFile(swos_dir / "sprite.dat", index_file, err)) return false;

    // Every dword is an offset; the file ends with a single zero terminator.
    //
    // Do NOT stop at the first zero. Offsets are relative to charset.dat for the
    // first 227 entries and then RESTART FROM ZERO relative to the joint block, so
    // entry 227 is legitimately 0 and treating it as the terminator truncates the
    // table to the character set. Trim trailing zeros instead.
    std::vector<uint32_t> offsets;
    for (size_t at = 0; at + 4 <= index_file.size(); at += 4)
        offsets.push_back(U32(index_file, at));
    while (offsets.size() > 1 && offsets.back() == 0) offsets.pop_back();

    if (offsets.empty()) {
        err = "sprite.dat contains no offsets";
        return false;
    }

    // Load each band's backing file. The two team slots take team1/team2 by default;
    // the three team files are the three shirt geometries and are interchangeable in
    // either slot, so which pair is loaded changes the art, not the layout.
    std::vector<uint8_t> files[kBandCount];
    const char* team_a = "team1.dat";
    const char* team_b = "team2.dat";
    for (size_t b = 0; b < kBandCount; ++b) {
        const char* name = kBands[b].file;
        if (!name) name = (kBands[b].first == 341) ? team_a : team_b;
        if (!ReadFile(swos_dir / name, files[b], err)) return false;
    }
    out.assembly = std::string("charset.dat | score.dat | ") + team_a + " | " +
                   team_b + " | goal1.dat x2 | bench.dat";

    // Each band's base within the joint block is the sum of the preceding joint-block
    // bands' file sizes. sprite.dat's offsets are joint-block-relative; a header's own
    // pixel pointer is file-relative (verified: structure offset + 24).
    size_t joint_base[kBandCount] = {};
    size_t running = 0;
    for (size_t b = 0; b < kBandCount; ++b) {
        if (!kBands[b].in_joint_block) continue;
        joint_base[b] = running;
        running += files[b].size();
    }

    out.sprites.resize(offsets.size());
    for (size_t b = 0; b < kBandCount; ++b) {
        const Band& band = kBands[b];
        const std::vector<uint8_t>& file = files[b];

        for (size_t k = 0; k < band.count; ++k) {
            const size_t i = band.first + k;
            if (i >= offsets.size()) break;

            const size_t joint_off = offsets[i];
            if (band.in_joint_block && joint_off < joint_base[b]) {
                ++out.skipped;
                continue;
            }
            const size_t struct_off =
                band.in_joint_block ? joint_off - joint_base[b] : joint_off;

            SwosSprite& s = out.sprites[i];
            if (!ReadHeaderAt(file, struct_off, s.header)) {
                ++out.skipped;
                continue;
            }

            ++out.ordinal_checked;
            if (s.header.ordinal + band.ordinal_bias == i) ++out.ordinal_matches;

            SwosSpriteHeader decode_header = s.header;
            if (band.pixels_joint_relative) {
                if (decode_header.pixel_offset < joint_base[b]) {
                    ++out.skipped;
                    continue;
                }
                decode_header.pixel_offset -= static_cast<uint32_t>(joint_base[b]);
            }
            s.indices = DecodePlanar(file, decode_header);
            if (s.indices.empty()) {
                ++out.skipped;
            } else {
                ++out.decoded;
            }
        }
    }

    // The player bank, once per team file. sprite.dat describes two SLOTS, and which
    // team file the game happened to load into a slot is a run's configuration -- so a
    // slot is the wrong thing to import. The geometry is the file, and all three are
    // read here with the slot-A band's addressing (the files are byte-for-byte the same
    // size and layout, so the offsets apply unchanged).
    static const char* kGeometryFiles[kShirtGeometryCount] = {"team1.dat", "team2.dat",
                                                              "team3.dat"};
    const size_t player_first = kBands[2].first;          // 341
    const size_t player_base  = joint_base[2];
    for (int g = 0; g < kShirtGeometryCount; ++g) {
        std::vector<uint8_t> file;
        if (!ReadFile(swos_dir / kGeometryFiles[g], file, err)) return false;

        auto& bank = out.geometries[static_cast<size_t>(g)];
        bank.resize(kPlayerBankFrames);
        for (int k = 0; k < kPlayerBankFrames; ++k) {
            const size_t i = player_first + static_cast<size_t>(k);
            if (i >= offsets.size()) break;
            const size_t joint_off = offsets[i];
            if (joint_off < player_base) continue;
            SwosSprite& s = bank[static_cast<size_t>(k)];
            if (!ReadHeaderAt(file, joint_off - player_base, s.header)) continue;
            s.indices = DecodePlanar(file, s.header);
        }
    }
    return true;
}

} // namespace at::assetc
