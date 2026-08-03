// assetc -- the asset importer.
//
// Reads art from a source tree we are entitled to read and writes OUR runtime format
// into a gitignored directory. It never writes into the tracked tree; see
// pack_writer.cpp for why that is enforced in code rather than in a comment.
//
//   assetc --source-dir <dir> --out <dir> [--pitch N] [--scale 12]
//
// doc/implementation/A4-asset-pipeline.md
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "assets/asset_pack.hpp"
#include "pack_writer.hpp"
#include "pitch.hpp"
#include "swos_import.hpp"
#include "swos_sprites.hpp"

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path source_dir;   // the reference's extracted PNG tree
    fs::path swos_dir;     // an original installation
    fs::path out_dir;
    int      pitch = 1;
    int      scale = 12;
};

void Usage() {
    std::printf(
        "assetc -- import art into the aftertouch runtime format\n"
        "\n"
        "  --swos-dir <dir>     an ORIGINAL installation (the folder holding\n"
        "                       sprite.dat). The faithful source.\n"
        "  --source-dir <dir>   the reference's extracted PNG tree. A bootstrap:\n"
        "                       resampled, so not pixel-faithful. See A4 6.1.\n"
        "  --out <dir>          destination; must be under <repo>/assets/generated\n"
        "  --pitch <n>          pitch number to import (default 1)\n"
        "  --scale <n>          upscale factor of the PNG tree (default 12)\n");
}

bool ParseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&](const char** dst) {
            if (i + 1 >= argc) return false;
            *dst = argv[++i];
            return true;
        };
        const char* v = nullptr;
        if (std::strcmp(a, "--source-dir") == 0) {
            if (!next(&v)) return false;
            o.source_dir = v;
        } else if (std::strcmp(a, "--swos-dir") == 0) {
            if (!next(&v)) return false;
            o.swos_dir = v;
        } else if (std::strcmp(a, "--out") == 0) {
            if (!next(&v)) return false;
            o.out_dir = v;
        } else if (std::strcmp(a, "--pitch") == 0) {
            if (!next(&v)) return false;
            o.pitch = std::atoi(v);
        } else if (std::strcmp(a, "--scale") == 0) {
            if (!next(&v)) return false;
            o.scale = std::atoi(v);
        } else {
            std::printf("unknown argument: %s\n", a);
            return false;
        }
    }
    return !(o.source_dir.empty() && o.swos_dir.empty());
}

// Report what an original installation actually contains. Run before trusting any
// of it: SWOS 2020 ships the port's own data directory, and after what the extracted
// PNG tree turned out to be, "these look like the original files" is not a finding.
//
// The decisive check is the ordinal. Every sprite header carries its own index at
// +22, so if our sprite.dat parsing, our joint-block assembly and our header layout
// are all right, the ordinals agree with the table that pointed at them. If any one
// of the three is wrong, they do not.
int ProbeSwos(const Options& o) {
    at::assetc::SwosSpriteSet set;
    std::string err;
    if (!at::assetc::LoadSwosSprites(o.swos_dir, set, err)) {
        std::printf("error: %s\n", err.c_str());
        return 1;
    }

    std::printf("original data at %s\n", o.swos_dir.string().c_str());
    std::printf("  sprite.dat     %zu sprites\n", set.sprites.size());
    std::printf("  assembly       %s\n", set.assembly.c_str());
    std::printf("  ordinals       %d / %d headers self-identify correctly\n",
                set.ordinal_matches, set.ordinal_checked);
    std::printf("  decoded        %d  (skipped %d)\n", set.decoded, set.skipped);

    const bool trustworthy =
        set.ordinal_checked > 0 &&
        set.ordinal_matches * 10 >= set.ordinal_checked * 9;   // 90%
    if (!trustworthy) {
        std::printf(
            "\n  WARNING: headers do not self-identify. Either the joint block is\n"
            "  assembled wrongly, or these .dat files are not the layout the format\n"
            "  documentation describes. Do NOT import from this until it is resolved.\n");
        return 1;
    }

    // Dimensions of the player bank, as a shape check against the documented ranges.
    auto describe = [&](const char* label, size_t lo, size_t hi) {
        int min_w = 9999, max_w = 0, min_h = 9999, max_h = 0, n = 0;
        int cx_lo = 9999, cx_hi = -9999, cy_lo = 9999, cy_hi = -9999;
        for (size_t i = lo; i <= hi && i < set.sprites.size(); ++i) {
            const auto& h = set.sprites[i].header;
            if (set.sprites[i].indices.empty()) continue;
            ++n;
            min_w = std::min<int>(min_w, h.width);
            max_w = std::max<int>(max_w, h.width);
            min_h = std::min<int>(min_h, h.nlines);
            max_h = std::max<int>(max_h, h.nlines);
            cx_lo = std::min<int>(cx_lo, h.centre_x);
            cx_hi = std::max<int>(cx_hi, h.centre_x);
            cy_lo = std::min<int>(cy_lo, h.centre_y);
            cy_hi = std::max<int>(cy_hi, h.centre_y);
        }
        if (n == 0) {
            std::printf("  %-14s none decoded\n", label);
            return;
        }
        std::printf("  %-14s %3d sprites  %dx%d..%dx%d  centre x[%d..%d] y[%d..%d]\n",
                    label, n, min_w, min_h, max_w, max_h, cx_lo, cx_hi, cy_lo, cy_hi);
    };

    std::printf("\n  documented banks\n");
    describe("charset", 0, 226);
    describe("players", 341, 441);
    describe("sleeves", 442, 542);
    describe("hstripes", 644, 744);
    describe("goalkeepers", 947, 1004);
    describe("bench", 1310, 1321);

    // Which palette indices the player bank actually uses. The kit system is built
    // on this: skin is 4/5/6, hair 12/9/13, shirt 10/11, shorts 14, socks 15. If the
    // player sprites use exactly those, the layer split is a pure function of the
    // index and we can perform it losslessly at import.
    int used[16] = {};
    for (size_t i = 341; i <= 441 && i < set.sprites.size(); ++i)
        for (uint8_t px : set.sprites[i].indices)
            if (px < 16) ++used[px];

    std::printf("\n  palette indices used by the player bank\n   ");
    for (int i = 0; i < 16; ++i)
        if (used[i]) std::printf(" %d", i);
    std::printf("\n");

    if (o.out_dir.empty()) {
        std::printf("\n  (probe only -- pass --out to write packs)\n");
        return 0;
    }

    at::assetc::SwosImportReport rep;
    if (!at::assetc::ImportSwosSprites(set, o.out_dir, rep, err)) {
        std::printf("error: %s\n", err.c_str());
        return 1;
    }

    std::printf("\n  wrote %d packs, %d sprites, %zu bytes to %s\n", rep.packs,
                rep.sprites, rep.bytes, o.out_dir.string().c_str());

    // Pitches come from the same installation and are a separate file pair per pitch.
    // Import every one present rather than only --pitch N: they cost 200 KB each, the
    // seasonal selection is later work, and a half-imported set is the kind of thing
    // that is discovered on the pitch nobody tested.
    std::printf("\n  pitches\n");
    int pitches = 0;
    for (int n = 1; n <= 6; ++n) {
        std::error_code ec;
        if (!fs::exists(o.swos_dir / ("pitch" + std::to_string(n) + ".blk"), ec)) continue;

        std::vector<uint8_t> pack, pal;
        at::assetc::SwosPitchReport pr;
        if (!at::assetc::ImportSwosPitch(o.swos_dir, n, pack, pal, pr, err)) {
            std::printf("error: %s\n", err.c_str());
            return 1;
        }
        if (!at::assets::Validate(pack) || !at::assets::Validate(pal)) {
            std::printf("error: pitch %d produced a pack that fails its own validator\n", n);
            return 1;
        }
        const std::string stem = "pitch" + std::to_string(n);
        if (!at::assetc::WritePack(o.out_dir / (stem + ".atp"), pack, err) ||
            !at::assetc::WritePack(o.out_dir / (stem + ".atl"), pal, err)) {
            std::printf("error: %s\n", err.c_str());
            return 1;
        }
        std::printf("    %-8s %4d tiles (%d used)  %dx%d cells  %zu bytes\n", stem.c_str(),
                    pr.tile_count, pr.tiles_used, pr.grid_w, pr.grid_h, pr.pack_bytes);
        ++pitches;
    }
    if (pitches == 0)
        std::printf("    none found (no pitchN.blk beside sprite.dat)\n");

    static const char* kLayerNames[] = {"background", "skin",   "hair",  "shirt",
                                        "stripes",    "shorts", "socks"};
    std::printf("\n  kit layers across the player bank\n");
    for (int i = 0; i < 7; ++i)
        std::printf("    %-11s %6d px\n", kLayerNames[i], rep.layer_pixels[i]);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    if (!ParseArgs(argc, argv, o)) {
        Usage();
        return 2;
    }

    // An original installation is the faithful source, so it takes precedence.
    if (!o.swos_dir.empty()) return ProbeSwos(o);

    if (o.out_dir.empty()) {
        Usage();
        return 2;
    }

    const fs::path pitch_dir =
        o.source_dir / "pitches" / ("pitch" + std::to_string(o.pitch));
    if (!fs::exists(pitch_dir)) {
        std::printf("error: no such directory: %s\n", pitch_dir.string().c_str());
        return 1;
    }

    std::vector<uint8_t> pack, palette_pack;
    at::assetc::PitchReport report;
    std::string err;

    if (!at::assetc::ImportPitch(pitch_dir, o.pitch, o.scale, pack, palette_pack,
                                 report, err)) {
        std::printf("error: %s\n", err.c_str());
        return 1;
    }

    if (!at::assets::Validate(pack) || !at::assets::Validate(palette_pack)) {
        std::printf("error: produced a pack that fails its own validator\n");
        return 1;
    }

    const fs::path pack_path =
        o.out_dir / ("pitch" + std::to_string(o.pitch) + ".atp");
    const fs::path pal_path =
        o.out_dir / ("pitch" + std::to_string(o.pitch) + ".atl");

    if (!at::assetc::WritePack(pack_path, pack, err) ||
        !at::assetc::WritePack(pal_path, palette_pack, err)) {
        std::printf("error: %s\n", err.c_str());
        return 1;
    }

    std::printf("pitch %d\n", o.pitch);
    std::printf("  tiles          %d at %dx%d\n", report.tile_count, report.tile_size,
                report.tile_size);
    std::printf("  grid           %d x %d  (max index %u)\n", report.grid_w,
                report.grid_h, report.max_tile_index);
    std::printf("  world space    %d x %d from the matrix as parsed\n",
                report.grid_w * report.tile_size, report.grid_h * report.tile_size);
    std::printf("                 %d x %d if the documented 42x53 is right (A4 6.2)\n",
                42 * report.tile_size, 53 * report.tile_size);
    std::printf("  palette        %d colours\n", report.palette_size);
    std::printf("  wrote          %s (%zu bytes)\n", pack_path.string().c_str(),
                report.pack_bytes);
    std::printf("  wrote          %s (%zu bytes)\n", pal_path.string().c_str(),
                palette_pack.size());

    // Fidelity, reported rather than assumed. A4 section 2.2 exists because the
    // reference tree is a resampled upscale; these are the numbers that say how much
    // of the original survived, and they are printed every run so that nobody has to
    // remember to go looking.
    std::printf("\n  fidelity\n");
    std::printf("    lossless tiles   %d / %d\n", report.lossless_tiles,
                report.tile_count);
    std::printf("    blended blocks   %d / %d (%.1f%%)\n", report.nonuniform_blocks,
                report.total_blocks,
                report.total_blocks ? 100.0 * report.nonuniform_blocks / report.total_blocks
                                    : 0.0);
    std::printf("    weakest vote     %d / %d pixels\n", report.weakest_majority,
                o.scale * o.scale);

    const bool blended = report.palette_size > 16 ||
                         report.weakest_majority * 2 <= o.scale * o.scale;
    if (blended) {
        std::printf(
            "\n  WARNING: this source is resampled, not indexed art.\n"
            "  The original is 16-colour and this import produced %d colours, with\n"
            "  blocks decided by as little as %d of %d pixels. These are NOT the\n"
            "  original's pixels -- they are the reference port's LANCZOS blends,\n"
            "  recovered by plurality vote. Usable to get a picture on screen; NOT\n"
            "  usable for the pixel-identical comparison that A4 exists for.\n"
            "  Fix: import from an original installation, or regenerate the source\n"
            "  tree with convertGameSprites.py --blocky. See A4 section 6.1.\n",
            report.palette_size, report.weakest_majority, o.scale * o.scale);
    }
    return 0;
}
