#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "core/match_state.hpp"

// Game data container -- see doc/implementation/A5-game-data.md.
//
// Same discipline as assets/asset_pack.hpp and core/trace.hpp:
//
//  * PURE. Encode/decode operate on byte spans. Callers do file I/O.
//  * LITTLE-ENDIAN BY EXPLICIT BYTE WRITES. Not by struct layout.
//  * FINGERPRINTED AND VERSIONED. Stale or foreign bytes are rejected.
//  * OUR SCHEMA. SWOS team.* / .tac bytes are never written or required.
//
// Attributes are whole bytes with range 0–7 (DATA.md section 3, corrected by
// B13 / R2 — AdjustPlayerSkills masks with $07777777, three bits per nibble, so
// the original's high bit is discarded on load). Packing is not worth the
// opacity; the range rule is enforced by AttrsValid below, and it is what makes
// every eight-entry attribute table in the engine correctly sized.

namespace at::data {

inline constexpr uint32_t kMagic = 0x44475441u;   // "ATGD", little-endian
inline constexpr uint16_t kFormatVersion = 1;

enum class Kind : uint16_t {
    kLeague  = 0,   // tactics + teams (+ players inline)
    kCareer  = 1,   // snapshot that references team ids
};

enum class Position : uint8_t {
    GK = 0, RB, LB, D, RW, LW, M, A
};

enum class Face : uint8_t { White = 0, Ginger = 1, Black = 2 };

enum class ShirtType : uint8_t {
    Plain = 0, ColouredSleeves = 1, VerticalStripes = 2, HorizontalStripes = 3
};

inline constexpr size_t kBallQuadrants = 35;   // 5 rows x 7 columns
inline constexpr size_t kTacticRoles   = 10;   // outfield; keeper excluded
inline constexpr size_t kSquadSize     = 16;   // 11 + 5
inline constexpr size_t kNameLen       = 24;
inline constexpr size_t kTacticNameLen = 16;
inline constexpr size_t kMaxTeams      = 256;
inline constexpr size_t kMaxTactics    = 64;
inline constexpr size_t kMaxCareerTeams = 256;

// 48 bytes: magic..reserved (40) + fingerprint (8). Same size class as ATAP.
inline constexpr size_t kHeaderSize = 48;

struct PlayerRecord {
    std::array<char, kNameLen> name{};
    uint8_t nationality = 0;
    uint8_t shirt       = 0;
    uint8_t position    = static_cast<uint8_t>(Position::M);
    uint8_t face        = static_cast<uint8_t>(Face::White);
    PlayerAttrs attrs{};
    uint8_t price_index = 0;
};

struct KitRecord {
    uint8_t shirt_type = 0;
    uint8_t stripes    = 0;
    uint8_t shirt      = 0;
    uint8_t shorts     = 0;
    uint8_t socks      = 0;
};

struct TeamRecord {
    uint16_t                   id = 0;
    std::array<char, kNameLen> name{};
    std::array<char, kNameLen> coach{};
    uint8_t                    tactics_id = 0;
    uint8_t                    tier       = 0;   // 0 premier … 4 non-league
    KitRecord                  primary{};
    KitRecord                  secondary{};
    std::array<PlayerRecord, kSquadSize> players{};
};

// When the ball is in quadrant n, role r heads for the packed (x,y) cell on the
// 16x15 authoring grid. High nibble = x, low nibble = y. Authored as if the
// upper goal is ours; B9 mirrors for the second side.
struct TacticRecord {
    std::array<char, kTacticNameLen> name{};
    uint8_t out_of_play = 0;
    std::array<std::array<uint8_t, kBallQuadrants>, kTacticRoles> cells{};
};

struct League {
    std::vector<TacticRecord> tactics;
    std::vector<TeamRecord>   teams;
};

enum class CareerGameType : uint8_t {
    Diy = 1, Preset = 2, Season = 3
};

// References teams by id. Full mutated team blobs belong in D2/SQLite, not here
// (DATA.md section 8).
struct CareerSnapshot {
    uint16_t         season_year = 0;
    CareerGameType   game_type   = CareerGameType::Season;
    int32_t          balance     = 0;
    int32_t          old_balance = 0;
    std::vector<uint16_t> team_ids;
};

// Fixed on-disk sizes (no pointers). Used by the encoder for layout math.
inline constexpr size_t kPlayerWireSize = 37;   // name24 + meta4 + attrs7 + price + reserved
inline constexpr size_t kKitWireSize    = 5;
inline constexpr size_t kTeamWireSize   =
    2 + kNameLen + kNameLen + 1 + 1 + kKitWireSize * 2 + kPlayerWireSize * kSquadSize;
inline constexpr size_t kTacticWireSize =
    kTacticNameLen + 1 + 3 + kTacticRoles * kBallQuadrants;

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
constexpr void PutI32(std::span<uint8_t> b, size_t& at, int32_t v) {
    PutU32(b, at, static_cast<uint32_t>(v));
}
constexpr void PutU64(std::span<uint8_t> b, size_t& at, uint64_t v) {
    for (int i = 0; i < 8; ++i) b[at++] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
}
inline void PutBytes(std::span<uint8_t> b, size_t& at, const char* src, size_t n) {
    for (size_t i = 0; i < n; ++i) b[at++] = static_cast<uint8_t>(src[i]);
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
constexpr int32_t GetI32(std::span<const uint8_t> b, size_t& at) {
    return static_cast<int32_t>(GetU32(b, at));
}
constexpr uint64_t GetU64(std::span<const uint8_t> b, size_t& at) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(b[at++]) << (8 * i);
    return v;
}
inline void GetBytes(std::span<const uint8_t> b, size_t& at, char* dst, size_t n) {
    for (size_t i = 0; i < n; ++i) dst[i] = static_cast<char>(b[at++]);
}

inline bool AttrOk(const PlayerAttrs& a) {
    return a.passing <= kAttrMax && a.shooting <= kAttrMax &&
           a.heading <= kAttrMax && a.tackling <= kAttrMax &&
           a.ball_control <= kAttrMax && a.speed <= kAttrMax &&
           a.finishing <= kAttrMax;
}

inline bool KitOk(const KitRecord& k) {
    return k.shirt_type <= 3 && k.stripes <= 9 && k.shirt <= 9 &&
           k.shorts <= 9 && k.socks <= 9;
}

inline bool PlayerOk(const PlayerRecord& p) {
    if (!AttrOk(p.attrs)) return false;
    if (p.position > static_cast<uint8_t>(Position::A)) return false;
    if (p.face > static_cast<uint8_t>(Face::Black)) return false;
    return true;
}

inline KitSpec ToKitSpec(const KitRecord& k) {
    KitSpec s;
    s.shirt_type = k.shirt_type;
    s.stripes    = k.stripes;
    s.shirt      = k.shirt;
    s.shorts     = k.shorts;
    s.socks      = k.socks;
    return s;
}

} // namespace detail

constexpr uint64_t Fingerprint(std::span<const uint8_t> bytes,
                               uint64_t seed = 1469598103934665603ull) {
    uint64_t h = seed;
    for (uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

inline bool LeagueOk(const League& league) {
    if (league.tactics.size() > kMaxTactics) return false;
    if (league.teams.size() > kMaxTeams) return false;
    if (league.tactics.empty() || league.teams.empty()) return false;
    for (const auto& team : league.teams) {
        if (team.tactics_id >= league.tactics.size()) return false;
        if (!detail::KitOk(team.primary) || !detail::KitOk(team.secondary)) return false;
        for (const auto& p : team.players)
            if (!detail::PlayerOk(p)) return false;
    }
    return true;
}

inline bool CareerOk(const CareerSnapshot& c) {
    if (c.team_ids.size() > kMaxCareerTeams) return false;
    const auto gt = static_cast<uint8_t>(c.game_type);
    return gt >= 1 && gt <= 3;
}

// ---------------------------------------------------------------------------
// Wire helpers
// ---------------------------------------------------------------------------

inline void WritePlayer(const PlayerRecord& p, std::span<uint8_t> out, size_t& at) {
    detail::PutBytes(out, at, p.name.data(), kNameLen);
    detail::PutU8(out, at, p.nationality);
    detail::PutU8(out, at, p.shirt);
    detail::PutU8(out, at, p.position);
    detail::PutU8(out, at, p.face);
    detail::PutU8(out, at, p.attrs.passing);
    detail::PutU8(out, at, p.attrs.shooting);
    detail::PutU8(out, at, p.attrs.heading);
    detail::PutU8(out, at, p.attrs.tackling);
    detail::PutU8(out, at, p.attrs.ball_control);
    detail::PutU8(out, at, p.attrs.speed);
    detail::PutU8(out, at, p.attrs.finishing);
    detail::PutU8(out, at, p.price_index);
    detail::PutU8(out, at, 0); // reserved
}

inline bool ReadPlayer(std::span<const uint8_t> in, size_t& at, PlayerRecord& p) {
    if (at + kPlayerWireSize > in.size()) return false;
    detail::GetBytes(in, at, p.name.data(), kNameLen);
    p.name[kNameLen - 1] = '\0';
    p.nationality         = detail::GetU8(in, at);
    p.shirt               = detail::GetU8(in, at);
    p.position            = detail::GetU8(in, at);
    p.face                = detail::GetU8(in, at);
    p.attrs.passing       = detail::GetU8(in, at);
    p.attrs.shooting      = detail::GetU8(in, at);
    p.attrs.heading       = detail::GetU8(in, at);
    p.attrs.tackling      = detail::GetU8(in, at);
    p.attrs.ball_control  = detail::GetU8(in, at);
    p.attrs.speed         = detail::GetU8(in, at);
    p.attrs.finishing     = detail::GetU8(in, at);
    p.price_index         = detail::GetU8(in, at);
    detail::GetU8(in, at); // reserved
    return detail::PlayerOk(p);
}

inline void WriteKit(const KitRecord& k, std::span<uint8_t> out, size_t& at) {
    detail::PutU8(out, at, k.shirt_type);
    detail::PutU8(out, at, k.stripes);
    detail::PutU8(out, at, k.shirt);
    detail::PutU8(out, at, k.shorts);
    detail::PutU8(out, at, k.socks);
}

inline bool ReadKit(std::span<const uint8_t> in, size_t& at, KitRecord& k) {
    if (at + kKitWireSize > in.size()) return false;
    k.shirt_type = detail::GetU8(in, at);
    k.stripes    = detail::GetU8(in, at);
    k.shirt      = detail::GetU8(in, at);
    k.shorts     = detail::GetU8(in, at);
    k.socks      = detail::GetU8(in, at);
    return detail::KitOk(k);
}

inline void WriteTactic(const TacticRecord& t, std::span<uint8_t> out, size_t& at) {
    detail::PutBytes(out, at, t.name.data(), kTacticNameLen);
    detail::PutU8(out, at, t.out_of_play);
    detail::PutU8(out, at, 0);
    detail::PutU8(out, at, 0);
    detail::PutU8(out, at, 0);
    for (size_t r = 0; r < kTacticRoles; ++r)
        for (size_t c = 0; c < kBallQuadrants; ++c)
            detail::PutU8(out, at, t.cells[r][c]);
}

inline bool ReadTactic(std::span<const uint8_t> in, size_t& at, TacticRecord& t) {
    if (at + kTacticWireSize > in.size()) return false;
    detail::GetBytes(in, at, t.name.data(), kTacticNameLen);
    t.name[kTacticNameLen - 1] = '\0';
    t.out_of_play = detail::GetU8(in, at);
    detail::GetU8(in, at);
    detail::GetU8(in, at);
    detail::GetU8(in, at);
    for (size_t r = 0; r < kTacticRoles; ++r)
        for (size_t c = 0; c < kBallQuadrants; ++c)
            t.cells[r][c] = detail::GetU8(in, at);
    return true;
}

inline void WriteTeam(const TeamRecord& team, std::span<uint8_t> out, size_t& at) {
    detail::PutU16(out, at, team.id);
    detail::PutBytes(out, at, team.name.data(), kNameLen);
    detail::PutBytes(out, at, team.coach.data(), kNameLen);
    detail::PutU8(out, at, team.tactics_id);
    detail::PutU8(out, at, team.tier);
    WriteKit(team.primary, out, at);
    WriteKit(team.secondary, out, at);
    for (const auto& p : team.players) WritePlayer(p, out, at);
}

inline bool ReadTeam(std::span<const uint8_t> in, size_t& at, TeamRecord& team) {
    if (at + 2 + kNameLen * 2 + 2 + kKitWireSize * 2 > in.size()) return false;
    team.id = detail::GetU16(in, at);
    detail::GetBytes(in, at, team.name.data(), kNameLen);
    team.name[kNameLen - 1] = '\0';
    detail::GetBytes(in, at, team.coach.data(), kNameLen);
    team.coach[kNameLen - 1] = '\0';
    team.tactics_id = detail::GetU8(in, at);
    team.tier       = detail::GetU8(in, at);
    if (!ReadKit(in, at, team.primary)) return false;
    if (!ReadKit(in, at, team.secondary)) return false;
    for (auto& p : team.players)
        if (!ReadPlayer(in, at, p)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// League encode / decode
// ---------------------------------------------------------------------------

inline size_t LeagueByteSize(const League& league) {
    return kHeaderSize +
           league.tactics.size() * kTacticWireSize +
           league.teams.size() * kTeamWireSize;
}

// Returns bytes written, or 0 on failure.
inline size_t EncodeLeague(const League& league, std::span<uint8_t> out) {
    if (!LeagueOk(league)) return 0;
    const size_t need = LeagueByteSize(league);
    if (out.size() < need) return 0;

    const uint32_t tactic_count = static_cast<uint32_t>(league.tactics.size());
    const uint32_t team_count   = static_cast<uint32_t>(league.teams.size());
    const uint32_t tactics_off  = static_cast<uint32_t>(kHeaderSize);
    const uint32_t tactics_sz   = tactic_count * static_cast<uint32_t>(kTacticWireSize);
    const uint32_t teams_off    = tactics_off + tactics_sz;
    const uint32_t teams_sz     = team_count * static_cast<uint32_t>(kTeamWireSize);

    size_t at = 0;
    detail::PutU32(out, at, kMagic);
    detail::PutU16(out, at, kFormatVersion);
    detail::PutU16(out, at, static_cast<uint16_t>(Kind::kLeague));
    detail::PutU32(out, at, tactic_count);
    detail::PutU32(out, at, team_count);
    detail::PutU32(out, at, tactics_off);
    detail::PutU32(out, at, tactics_sz);
    detail::PutU32(out, at, teams_off);
    detail::PutU32(out, at, teams_sz);
    detail::PutU32(out, at, 0); // reserved
    detail::PutU32(out, at, 0);
    detail::PutU64(out, at, 0); // fingerprint placeholder
    if (at != kHeaderSize) return 0;

    for (const auto& t : league.tactics) WriteTactic(t, out, at);
    for (const auto& team : league.teams) WriteTeam(team, out, at);

    // Fingerprint covers the body only — not the header that stores it.
    const auto body = out.subspan(kHeaderSize, need - kHeaderSize);
    const uint64_t fp = Fingerprint(body);
    size_t fp_at = 40;
    detail::PutU64(out, fp_at, fp);
    return need;
}

inline bool DecodeLeague(std::span<const uint8_t> in, League& league) {
    league = League{};
    if (in.size() < kHeaderSize) return false;
    size_t at = 0;
    if (detail::GetU32(in, at) != kMagic) return false;
    if (detail::GetU16(in, at) != kFormatVersion) return false;
    if (detail::GetU16(in, at) != static_cast<uint16_t>(Kind::kLeague)) return false;
    const uint32_t tactic_count = detail::GetU32(in, at);
    const uint32_t team_count   = detail::GetU32(in, at);
    const uint32_t tactics_off  = detail::GetU32(in, at);
    const uint32_t tactics_sz   = detail::GetU32(in, at);
    const uint32_t teams_off    = detail::GetU32(in, at);
    const uint32_t teams_sz     = detail::GetU32(in, at);
    detail::GetU32(in, at);
    detail::GetU32(in, at);
    const uint64_t fp = detail::GetU64(in, at);
    if (at != kHeaderSize) return false;

    if (tactic_count == 0 || tactic_count > kMaxTactics) return false;
    if (team_count == 0 || team_count > kMaxTeams) return false;
    if (tactics_sz != tactic_count * kTacticWireSize) return false;
    if (teams_sz != team_count * kTeamWireSize) return false;
    if (static_cast<size_t>(tactics_off) + tactics_sz > in.size()) return false;
    if (static_cast<size_t>(teams_off) + teams_sz > in.size()) return false;
    if (tactics_off < kHeaderSize) return false;

    const auto body = in.subspan(kHeaderSize, in.size() - kHeaderSize);
    if (Fingerprint(body) != fp) return false;

    league.tactics.resize(tactic_count);
    league.teams.resize(team_count);
    at = tactics_off;
    for (auto& t : league.tactics)
        if (!ReadTactic(in, at, t)) return false;
    at = teams_off;
    for (auto& team : league.teams)
        if (!ReadTeam(in, at, team)) return false;
    return LeagueOk(league);
}

inline bool ValidateLeagueBytes(std::span<const uint8_t> in) {
    League unused;
    return DecodeLeague(in, unused);
}

// ---------------------------------------------------------------------------
// Career encode / decode
// ---------------------------------------------------------------------------

inline size_t CareerByteSize(const CareerSnapshot& c) {
    return kHeaderSize + 2 + 1 + 1 + 4 + 4 + 2 + c.team_ids.size() * 2;
}

inline size_t EncodeCareer(const CareerSnapshot& c, std::span<uint8_t> out) {
    if (!CareerOk(c)) return 0;
    const size_t need = CareerByteSize(c);
    if (out.size() < need) return 0;

    const uint32_t team_count = static_cast<uint32_t>(c.team_ids.size());
    const uint32_t body_off   = static_cast<uint32_t>(kHeaderSize);
    const uint32_t body_sz    = static_cast<uint32_t>(need - kHeaderSize);

    size_t at = 0;
    // Same 48-byte header layout as the league kind; unused fields stay zero.
    detail::PutU32(out, at, kMagic);
    detail::PutU16(out, at, kFormatVersion);
    detail::PutU16(out, at, static_cast<uint16_t>(Kind::kCareer));
    detail::PutU32(out, at, 0);           // tactic_count unused
    detail::PutU32(out, at, team_count);
    detail::PutU32(out, at, body_off);    // "tactics_off" slot = body
    detail::PutU32(out, at, body_sz);     // "tactics_sz" slot = body size
    detail::PutU32(out, at, 0);           // teams_off unused
    detail::PutU32(out, at, 0);           // teams_sz unused
    detail::PutU32(out, at, 0);           // reserved
    detail::PutU32(out, at, 0);           // reserved
    detail::PutU64(out, at, 0);           // fingerprint placeholder
    if (at != kHeaderSize) return 0;

    detail::PutU16(out, at, c.season_year);
    detail::PutU8(out, at, static_cast<uint8_t>(c.game_type));
    detail::PutU8(out, at, 0);
    detail::PutI32(out, at, c.balance);
    detail::PutI32(out, at, c.old_balance);
    detail::PutU16(out, at, static_cast<uint16_t>(team_count));
    for (uint16_t id : c.team_ids) detail::PutU16(out, at, id);

    const auto body = out.subspan(kHeaderSize, need - kHeaderSize);
    const uint64_t fp = Fingerprint(body);
    size_t fp_at = 40;
    detail::PutU64(out, fp_at, fp);
    return need;
}

inline bool DecodeCareer(std::span<const uint8_t> in, CareerSnapshot& c) {
    c = CareerSnapshot{};
    if (in.size() < kHeaderSize) return false;
    size_t at = 0;
    if (detail::GetU32(in, at) != kMagic) return false;
    if (detail::GetU16(in, at) != kFormatVersion) return false;
    if (detail::GetU16(in, at) != static_cast<uint16_t>(Kind::kCareer)) return false;
    detail::GetU32(in, at); // tactic_count unused
    const uint32_t team_count = detail::GetU32(in, at);
    const uint32_t body_off   = detail::GetU32(in, at);
    const uint32_t body_sz    = detail::GetU32(in, at);
    detail::GetU32(in, at); // teams_off unused
    detail::GetU32(in, at); // teams_sz unused
    detail::GetU32(in, at); // reserved
    detail::GetU32(in, at); // reserved
    const uint64_t fp = detail::GetU64(in, at);
    if (at != kHeaderSize) return false;

    if (team_count > kMaxCareerTeams) return false;
    if (body_off != kHeaderSize) return false;
    if (static_cast<size_t>(body_off) + body_sz > in.size()) return false;
    if (body_sz != 2 + 1 + 1 + 4 + 4 + 2 + team_count * 2) return false;

    const auto body = in.subspan(kHeaderSize, body_sz);
    if (Fingerprint(body) != fp) return false;

    at = body_off;
    c.season_year = detail::GetU16(in, at);
    c.game_type   = static_cast<CareerGameType>(detail::GetU8(in, at));
    detail::GetU8(in, at);
    c.balance     = detail::GetI32(in, at);
    c.old_balance = detail::GetI32(in, at);
    if (detail::GetU16(in, at) != team_count) return false;
    c.team_ids.resize(team_count);
    for (uint32_t i = 0; i < team_count; ++i)
        c.team_ids[i] = detail::GetU16(in, at);
    return CareerOk(c);
}

// ---------------------------------------------------------------------------
// Kickoff projection
// ---------------------------------------------------------------------------

inline const TeamRecord* FindTeam(const League& league, uint16_t id) {
    for (const auto& t : league.teams)
        if (t.id == id) return &t;
    return nullptr;
}

inline const TacticRecord* FindTactic(const League& league, uint8_t id) {
    if (static_cast<size_t>(id) >= league.tactics.size()) return nullptr;
    return &league.tactics[id];
}

// Projects two squads into MatchState. Players 0–10 = home, 11–21 = away.
// Fills all 16 squad slots + tactics snapshot + pitch identity. Does not place
// anyone on the pitch — B4 owns starting positions. (B1-state-layout.md)
inline bool ApplyKickoff(const League& league, uint16_t home_id, uint16_t away_id,
                         MatchState& state) {
    if (!LeagueOk(league)) return false;
    const TeamRecord* home = FindTeam(league, home_id);
    const TeamRecord* away = FindTeam(league, away_id);
    if (!home || !away || home == away) return false;

    state = MatchState{};
    state.phase = MatchPhase::KickOff;
    state.referee.team_number = 3;
    state.booked_indicator.team_number = 3;

    const TeamRecord* sides[2] = {home, away};
    for (int side = 0; side < 2; ++side) {
        const TeamRecord& team = *sides[side];
        MatchSide& ms = state.sides[static_cast<size_t>(side)];
        TeamSheet& sheet = ms.sheet;
        std::memcpy(sheet.name.data(), team.name.data(), kNameLen);
        sheet.tactics_id = team.tactics_id;
        sheet.primary    = detail::ToKitSpec(team.primary);
        sheet.secondary  = detail::ToKitSpec(team.secondary);

        ms.control.team_number = static_cast<uint8_t>(side + 1);
        ms.control.controlled_slot =
            static_cast<int8_t>(side * 11); // default to each side's first player

        if (const TacticRecord* tac = FindTactic(league, team.tactics_id)) {
            ms.tactics.out_of_play = tac->out_of_play;
            ms.tactics.cells       = tac->cells;
        }

        for (size_t i = 0; i < kSquadSize; ++i) {
            const PlayerRecord& p = team.players[i];
            SquadPlayer& sp = ms.squad[i];
            sp.index        = static_cast<uint8_t>(i);
            sp.shirt_number = p.shirt;
            sp.position     = p.position;
            sp.face         = p.face;
            sp.attrs        = p.attrs;
            std::memcpy(sp.full_name.data(), p.name.data(), kNameLen);
            // short_name: first 15 chars of the authored name
            for (size_t c = 0; c < 15 && p.name[c] != '\0'; ++c)
                sp.short_name[c] = p.name[c];
        }

        for (size_t i = 0; i < 11; ++i) {
            const size_t slot = static_cast<size_t>(side) * 11 + i;
            Entity& e = state.players[slot];
            e.team_number    = static_cast<int16_t>(side + 1);
            e.player_ordinal = static_cast<int16_t>(i + 1);
            e.player_direction = 0;
        }
    }
    return true;
}

} // namespace at::data
