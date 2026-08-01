#include "tracekit/tracekit.hpp"

#include <cstdio>
#include <cstring>

// Exit 0 if identical, 1 if divergent, 2 on usage/IO error.
// Prints first divergence tick, field class, and a short drift profile so A6 /
// scripts can consume the number without a GUI (A3 §2.5, §2.6).

static void Usage(const char* argv0) {
    std::fprintf(stderr, "usage: %s [--drift] <a.attr> <b.attr>\n", argv0);
}

int main(int argc, char** argv) {
    bool show_drift = false;
    const char* path_a = nullptr;
    const char* path_b = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--drift") == 0) {
            show_drift = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            Usage(argv[0]);
            return 0;
        } else if (!path_a) {
            path_a = argv[i];
        } else if (!path_b) {
            path_b = argv[i];
        } else {
            Usage(argv[0]);
            return 2;
        }
    }
    if (!path_a || !path_b) {
        Usage(argv[0]);
        return 2;
    }

    std::vector<uint8_t> a, b;
    if (!at::tracekit::ReadFile(path_a, a)) {
        std::fprintf(stderr, "tracediff: cannot read %s\n", path_a);
        return 2;
    }
    if (!at::tracekit::ReadFile(path_b, b)) {
        std::fprintf(stderr, "tracediff: cannot read %s\n", path_b);
        return 2;
    }

    const auto r = at::tracekit::Diff(a, b);
    if (r.identical) {
        std::printf("identical (%zu bytes)\n", a.size());
        return 0;
    }
    std::printf("diverge at tick %u (%s) class=%s byte=%zu\n", r.tick,
                r.reason ? r.reason : "unknown",
                at::tracekit::FieldClassName(r.first_class), r.first_byte);

    if (show_drift && !r.drift.empty()) {
        const auto& first = r.drift.front();
        const auto& last  = r.drift.back();
        std::printf("drift: ticks=%zu first_l1=%llu last_l1=%llu\n", r.drift.size(),
                    static_cast<unsigned long long>(first.l1_position),
                    static_cast<unsigned long long>(last.l1_position));
        const size_t n = r.drift.size() < 8 ? r.drift.size() : 8;
        for (size_t i = 0; i < n; ++i) {
            const auto& s = r.drift[i];
            std::printf("  tick %u  l1=%llu  bytes=%u\n", s.tick,
                        static_cast<unsigned long long>(s.l1_position), s.bytes_differ);
        }
        if (r.drift.size() > n) {
            std::printf("  ... (%zu more)\n", r.drift.size() - n);
        }
    }
    return 1;
}
