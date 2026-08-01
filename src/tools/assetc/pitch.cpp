#include "pitch.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "image.hpp"
#include "pack_writer.hpp"

namespace at::assetc {

namespace fs = std::filesystem;

namespace {

// The index matrix is plain whitespace-separated integers, one row per line. Rows are
// read as they come rather than assumed: compileAssets.py declares 42x53 while the
// files parse as 42x55, and which is right matters enough that guessing is worse than
// reporting (A4 section 6.2).
bool ReadGrid(const fs::path& path, std::vector<uint16_t>& cells, int& w, int& h,
              std::string& err) {
    std::ifstream f(path);
    if (!f) {
        err = "cannot open " + path.string();
        return false;
    }
    cells.clear();
    w = 0;
    h = 0;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream is(line);
        int row_w = 0, v = 0;
        while (is >> v) {
            if (v < 0 || v > 65535) {
                err = "tile index out of range in " + path.string();
                return false;
            }
            cells.push_back(static_cast<uint16_t>(v));
            ++row_w;
        }
        if (row_w == 0) continue;   // blank trailing line
        if (w == 0) {
            w = row_w;
        } else if (row_w != w) {
            err = "ragged tile matrix in " + path.string() + ": row " +
                  std::to_string(h) + " has " + std::to_string(row_w) +
                  " columns, expected " + std::to_string(w);
            return false;
        }
        ++h;
    }
    if (w == 0 || h == 0) {
        err = "empty tile matrix in " + path.string();
        return false;
    }
    return true;
}

std::string TileName(int pitch_number, int index) {
    std::ostringstream os;
    os << "pt" << pitch_number << '-';
    os.width(4);
    os.fill('0');
    os << index;
    os << ".png";
    return os.str();
}

} // namespace

bool ImportPitch(const fs::path& pitch_dir, int pitch_number, int scale,
                 std::vector<uint8_t>& out_pack, std::vector<uint8_t>& out_palette_pack,
                 PitchReport& report, std::string& err) {
    std::vector<uint16_t> cells;
    int grid_w = 0, grid_h = 0;
    const fs::path grid_path =
        pitch_dir / ("pitch" + std::to_string(pitch_number) + ".txt");
    if (!ReadGrid(grid_path, cells, grid_w, grid_h, err)) return false;

    report.grid_w = grid_w;
    report.grid_h = grid_h;
    report.max_tile_index = *std::max_element(cells.begin(), cells.end());

    PackBuilder tiles(assets::Kind::kPitch, assets::SourceKind::kRefTree);
    std::vector<Rgba> palette;

    // Tiles are numbered contiguously from zero; stop at the first gap rather than
    // globbing, so a truncated source tree is an error instead of a short pack.
    int index = 0;
    for (;; ++index) {
        const fs::path tile_path = pitch_dir / TileName(pitch_number, index);
        std::error_code ec;
        if (!fs::exists(tile_path, ec)) break;

        Image img;
        if (!LoadPng(tile_path, img, err)) return false;

        const Downscaled d = DownscaleMajority(img, scale);
        if (d.image.width == 0) {
            err = tile_path.string() + " is " + std::to_string(img.width) + "x" +
                  std::to_string(img.height) + ", not a multiple of the " +
                  std::to_string(scale) + "x scale";
            return false;
        }
        report.nonuniform_blocks += d.nonuniform_blocks;
        report.total_blocks += d.image.width * d.image.height;
        if (d.nonuniform_blocks == 0) ++report.lossless_tiles;
        report.weakest_majority = report.weakest_majority == 0
                                      ? d.weakest_majority
                                      : std::min(report.weakest_majority, d.weakest_majority);

        if (report.tile_size == 0) {
            report.tile_size = d.image.width;
        } else if (d.image.width != report.tile_size ||
                   d.image.height != report.tile_size) {
            err = "tile " + tile_path.string() + " is not " +
                  std::to_string(report.tile_size) + " square like the others";
            return false;
        }

        std::vector<uint8_t> indices;
        if (!Quantise(d.image, palette, indices, err)) return false;

        if (!tiles.Add(static_cast<uint16_t>(d.image.width),
                       static_cast<uint16_t>(d.image.height), 0, 0, indices, err))
            return false;
        tiles.MixSource(indices);
    }

    if (index == 0) {
        err = "no tiles found in " + pitch_dir.string();
        return false;
    }
    report.tile_count   = index;
    report.palette_size = static_cast<int>(palette.size());

    if (report.max_tile_index >= static_cast<uint32_t>(index)) {
        err = "tile matrix references index " + std::to_string(report.max_tile_index) +
              " but only " + std::to_string(index) + " tiles exist";
        return false;
    }

    // The matrix goes in the aux section at 16 bits per cell. Eight would fit today
    // -- the largest index observed is under 256 -- but a format that silently breaks
    // on the first pitch with 300 tiles is a format that breaks in Wave 4.
    std::vector<uint8_t> aux(cells.size() * 2);
    for (size_t i = 0; i < cells.size(); ++i) {
        aux[i * 2 + 0] = static_cast<uint8_t>(cells[i] & 0xFFu);
        aux[i * 2 + 1] = static_cast<uint8_t>((cells[i] >> 8) & 0xFFu);
    }
    tiles.SetAux(static_cast<uint16_t>(grid_w), static_cast<uint16_t>(grid_h),
                 std::move(aux));

    out_pack = tiles.Build();
    report.pack_bytes = out_pack.size();

    // The palette travels as its own pack: a 16x1 "image" of RGBA quadruples, so the
    // same container and the same validator cover it.
    PackBuilder pal(assets::Kind::kPalette, assets::SourceKind::kRefTree);
    std::vector<uint8_t> pal_bytes(palette.size() * 4);
    for (size_t i = 0; i < palette.size(); ++i) {
        pal_bytes[i * 4 + 0] = palette[i].r;
        pal_bytes[i * 4 + 1] = palette[i].g;
        pal_bytes[i * 4 + 2] = palette[i].b;
        pal_bytes[i * 4 + 3] = palette[i].a;
    }
    if (!pal.Add(static_cast<uint16_t>(palette.size() * 4), 1, 0, 0, pal_bytes, err))
        return false;
    out_palette_pack = pal.Build();

    return true;
}

} // namespace at::assetc
