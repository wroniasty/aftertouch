#include "tracekit/tracekit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Headless: scenario -> .attr file. Regen goldens with:
//   tracegen --out tests/golden/kickoff.attr

static void Usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s [--ticks N] [--seed HEX] [--out path]\n"
                 "  default: kickoff scenario, 100 ticks, seed 0xA5A50001\n",
                 argv0);
}

int main(int argc, char** argv) {
    uint32_t ticks = 100;
    uint32_t seed  = 0xA5A50001u;
    const char* out_path = "kickoff.attr";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ticks = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            Usage(argv[0]);
            return 0;
        } else {
            Usage(argv[0]);
            return 2;
        }
    }

    auto scenario = at::tracekit::KickoffScenario(ticks);
    scenario.seed = seed;

    std::vector<uint8_t> bytes;
    if (!at::tracekit::Generate(scenario, bytes)) {
        std::fprintf(stderr, "tracegen: generate failed\n");
        return 1;
    }
    if (!at::tracekit::WriteFile(out_path, bytes)) {
        std::fprintf(stderr, "tracegen: failed to write %s\n", out_path);
        return 1;
    }
    std::printf("wrote %zu bytes to %s (%u ticks, seed 0x%08X)\n", bytes.size(),
                out_path, ticks, seed);
    return 0;
}
