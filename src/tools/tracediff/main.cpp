#include "tracekit/tracekit.hpp"

#include <cstdio>
#include <cstring>

// Exit 0 if identical, 1 if divergent, 2 on usage/IO error.
// Prints the first divergence tick so A6 / scripts can consume it without a GUI.

static void Usage(const char* argv0) {
    std::fprintf(stderr, "usage: %s <a.attr> <b.attr>\n", argv0);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        Usage(argv[0]);
        return 2;
    }

    std::vector<uint8_t> a, b;
    if (!at::tracekit::ReadFile(argv[1], a)) {
        std::fprintf(stderr, "tracediff: cannot read %s\n", argv[1]);
        return 2;
    }
    if (!at::tracekit::ReadFile(argv[2], b)) {
        std::fprintf(stderr, "tracediff: cannot read %s\n", argv[2]);
        return 2;
    }

    const auto r = at::tracekit::Diff(a, b);
    if (r.identical) {
        std::printf("identical (%zu bytes)\n", a.size());
        return 0;
    }
    std::printf("diverge at tick %u (%s)\n", r.tick,
                r.reason ? r.reason : "unknown");
    return 1;
}
