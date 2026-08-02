#include "tracekit/tracekit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Headless: scenario / input log -> .attr file.
//   tracegen --out tests/golden/kickoff.attr
//   tracegen --scenario shot_curl --out tests/corpus/shot_curl/engine.attr
//   tracegen --atin path.atin --out out.attr [--stub-oracle]
//   tracegen --scenario kickoff --write-atin path.atin
//   tracegen --atin path.atin --transcript out.txt
//   tracegen --attr path.attr --transcript out.txt

static void Usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s [--ticks N] [--seed HEX] [--scenario kickoff|shot_curl]\n"
                 "          [--atin path] [--attr path] [--write-atin path]\n"
                 "          [--stub-oracle] [--out path] [--transcript path]\n"
                 "          [--atin-only]\n",
                 argv0);
}

int main(int argc, char** argv) {
    uint32_t ticks = 100;
    uint32_t seed  = 0;
    bool seed_set  = false;
    const char* out_path = "kickoff.attr";
    const char* scenario_name = "kickoff";
    const char* atin_path = nullptr;
    const char* attr_path = nullptr;
    const char* write_atin = nullptr;
    const char* transcript_path = nullptr;
    bool stub_oracle = false;
    bool want_attr = true;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ticks = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 0));
            seed_set = true;
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (std::strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            scenario_name = argv[++i];
        } else if (std::strcmp(argv[i], "--atin") == 0 && i + 1 < argc) {
            atin_path = argv[++i];
        } else if (std::strcmp(argv[i], "--attr") == 0 && i + 1 < argc) {
            attr_path = argv[++i];
        } else if (std::strcmp(argv[i], "--write-atin") == 0 && i + 1 < argc) {
            write_atin = argv[++i];
        } else if (std::strcmp(argv[i], "--transcript") == 0 && i + 1 < argc) {
            transcript_path = argv[++i];
        } else if (std::strcmp(argv[i], "--stub-oracle") == 0) {
            stub_oracle = true;
        } else if (std::strcmp(argv[i], "--atin-only") == 0) {
            want_attr = false;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            Usage(argv[0]);
            return 0;
        } else {
            Usage(argv[0]);
            return 2;
        }
    }

    // ATTR → transcript only (no regenerate).
    if (attr_path && transcript_path) {
        std::vector<uint8_t> bytes;
        if (!at::tracekit::ReadFile(attr_path, bytes)) {
            std::fprintf(stderr, "tracegen: bad ATTR %s\n", attr_path);
            return 1;
        }
        std::string text;
        if (!at::tracekit::WriteSparseTranscript(bytes, text)) {
            std::fprintf(stderr, "tracegen: transcript failed for %s\n", attr_path);
            return 1;
        }
        const auto span = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(text.data()), text.size());
        if (!at::tracekit::WriteFile(transcript_path, span)) {
            std::fprintf(stderr, "tracegen: failed to write %s\n", transcript_path);
            return 1;
        }
        std::printf("wrote transcript %s (%zu bytes)\n", transcript_path, text.size());
        return 0;
    }

    at::tracekit::Scenario scenario;
    if (atin_path) {
        std::vector<uint8_t> bytes;
        if (!at::tracekit::ReadFile(atin_path, bytes) ||
            !at::tracekit::DeserializeInputLog(bytes, scenario)) {
            std::fprintf(stderr, "tracegen: bad input log %s\n", atin_path);
            return 1;
        }
    } else if (std::strcmp(scenario_name, "shot_curl") == 0) {
        scenario = at::tracekit::ShotCurlScenario(ticks);
    } else if (std::strcmp(scenario_name, "kickoff") == 0) {
        scenario = at::tracekit::KickoffScenario(ticks);
    } else {
        std::fprintf(stderr, "tracegen: unknown scenario '%s'\n", scenario_name);
        return 2;
    }
    if (seed_set) scenario.seed = seed;

    if (write_atin) {
        std::vector<uint8_t> atin;
        if (!at::tracekit::SerializeInputLog(scenario, atin) ||
            !at::tracekit::WriteFile(write_atin, atin)) {
            std::fprintf(stderr, "tracegen: failed to write %s\n", write_atin);
            return 1;
        }
        std::printf("wrote input log %s (%u ticks)\n", write_atin,
                    static_cast<uint32_t>(scenario.inputs.size()));
    }

    if (transcript_path) {
        std::string text;
        if (!at::tracekit::WriteSparseTranscript(scenario, text)) {
            std::fprintf(stderr, "tracegen: transcript generate failed\n");
            return 1;
        }
        const auto span = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(text.data()), text.size());
        if (!at::tracekit::WriteFile(transcript_path, span)) {
            std::fprintf(stderr, "tracegen: failed to write %s\n", transcript_path);
            return 1;
        }
        std::printf("wrote transcript %s (%zu bytes)\n", transcript_path, text.size());
        if (!want_attr && !write_atin) return 0;
    }

    if (!want_attr) return 0;

    std::vector<uint8_t> bytes;
    at::tracekit::MutateFn mut =
        stub_oracle ? &at::tracekit::StubOracleMutate : nullptr;
    if (!at::tracekit::Generate(scenario, bytes, mut)) {
        std::fprintf(stderr, "tracegen: generate failed\n");
        return 1;
    }
    if (!at::tracekit::WriteFile(out_path, bytes)) {
        std::fprintf(stderr, "tracegen: failed to write %s\n", out_path);
        return 1;
    }
    std::printf("wrote %zu bytes to %s (%u ticks, seed 0x%08X%s)\n", bytes.size(),
                out_path, static_cast<uint32_t>(scenario.inputs.size()), scenario.seed,
                stub_oracle ? ", stub-oracle" : "");
    return 0;
}
