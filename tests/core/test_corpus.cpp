#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

#ifndef AT_CORPUS_DIR
#error "AT_CORPUS_DIR required"
#endif

static std::string CorpusPath(const char* rel) {
    return std::string(AT_CORPUS_DIR) + "/" + rel;
}

static uint64_t ReadChainFile(const char* rel) {
    const std::string path = CorpusPath(rel);
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path.c_str(), "rb") != 0) return 0;
#else
    f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;
#endif
    char buf[32] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    if (n == 0) return 0;
    return std::strtoull(buf, nullptr, 16);
}

TEST_CASE("corpus: kickoff input log parses and matches engine chain") {
    std::vector<uint8_t> atin, eng, ref;
    REQUIRE(ReadFile(CorpusPath("kickoff/input.atin").c_str(), atin));
    Scenario s;
    REQUIRE(DeserializeInputLog(atin, s));
    CHECK(s.seed == 0xA5A50001u);
    CHECK(s.inputs.size() == 100);

    REQUIRE(ReadFile(CorpusPath("kickoff/engine.attr").c_str(), eng));
    REQUIRE(ReadFile(CorpusPath("kickoff/reference.attr").c_str(), ref));

    std::vector<uint8_t> regen;
    REQUIRE(Generate(s, regen));
    CHECK(Diff(eng, regen).identical);
    CHECK(HashChain(eng) == ReadChainFile("kickoff/engine.chain"));
    CHECK(HashChain(ref) == ReadChainFile("kickoff/reference.chain"));

    const DiffResult r = Diff(eng, ref);
    CHECK_FALSE(r.identical);
    CHECK(r.tick == 1);
    CHECK(r.first_class == FieldClass::Position);
}

TEST_CASE("corpus: shot_curl committed pair diverges and chains match") {
    std::vector<uint8_t> atin, eng, ref;
    REQUIRE(ReadFile(CorpusPath("shot_curl/input.atin").c_str(), atin));
    Scenario s;
    REQUIRE(DeserializeInputLog(atin, s));
    CHECK(s.seed == 0xA5A50002u);

    REQUIRE(ReadFile(CorpusPath("shot_curl/engine.attr").c_str(), eng));
    REQUIRE(ReadFile(CorpusPath("shot_curl/reference.attr").c_str(), ref));
    CHECK(HashChain(eng) == ReadChainFile("shot_curl/engine.chain"));
    CHECK(HashChain(ref) == ReadChainFile("shot_curl/reference.chain"));
    CHECK_FALSE(Diff(eng, ref).identical);
}
