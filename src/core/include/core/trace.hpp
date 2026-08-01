#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/fixed.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"

// The trace record: one fixed-width, little-endian record per tick, plus a header.
// See doc/implementation/A3-trace-harness.md and B1-state-layout.md.
//
// ATTR serialises the full MatchState field-by-field (plus MatchInput).

namespace at::trace {

inline constexpr uint32_t kMagic = 0x52545441u; // "ATTR"

// Version 4: B3 MatchSurface. v1–v3 readers are retired.
inline constexpr uint16_t kFormatVersion = 4;

enum class Profile : uint8_t { kAmiga = 0, kPc = 1 };

struct Header {
    uint32_t seed          = 0;
    uint32_t record_count  = 0;
    Profile  profile       = Profile::kAmiga;
    uint8_t  first_team    = 0;
    uint8_t  tick_hz       = 50;
};

inline constexpr size_t kHeaderSize = 24;

// Wire sizes (no struct pads). Keep in lockstep with Put*/Get* below.
inline constexpr size_t kEntityWireSize =
    6 * 4 + // pos + delta
    2 * 2 + // dest
    2 * 2 + // team_number, player_ordinal
    2 * 2 + // frame_offset, starting_direction
    4 * 2 + // frame cursor
    4 * 2 + // direction, speed, full_direction, player_direction
    4 * 2 + // visible, image_index, save_sprite, on_screen
    4 * 2 + // is_moving, tackle_state, heading, dest_reached_state
    4 * 2 + // cards, injury_level, tackling_timer, sent_away
    4 +     // ball_distance
    1 + 1;  // player_state, player_down_timer

inline constexpr size_t kArenaEntityCount = 1 + kPitchPlayers + 1 + 1; // ball, players, ref, booked

inline constexpr size_t kTeamControlWireSize =
    4 + 4 +     // identity + slots
    2 * 2 +     // update_player_index, player_has_ball
    3 * 2 + 4 + // directions + fire bytes
    2 * 2 +     // header_or_tackle, fire_counter
    2 * 2 +     // controlled_pl_direction, shooting
    9 +         // proximity
    5 * 2 +     // gk / ball-out flags
    3 * 2 +     // pass / switch
    5 * 2 +     // ball in/out/xy + pass_kick
    2 * 2 +     // ball_can_be_controlled, controlling dir
    6 * 2 +     // spin / pass flags
    6 * 2 +     // AI + won + gk_playing + reset
    1;          // secondary_fire

inline constexpr size_t kSquadPlayerWireSize =
    8 + kShortNameLen + 7 + 4 + kMatchNameLen; // meta, short, attrs(7), meta2, full

inline constexpr size_t kTacticsWireSize =
    1 + kMatchTacticRoles * kMatchBallQuadrants;

inline constexpr size_t kTeamSheetWireSize =
    kMatchNameLen + 1 + 5 + 5; // name, tactics_id, kits

inline constexpr size_t kGlobalsWireSize =
    8 + 4 + 1 + 1 + // uint8 block + booked_player + last_team_booked
    6 * 2 +         // foul + predictors + ref_timer
    2 +             // sub flags
    2 * 2 +         // wait + fans
    2 * 2 +         // marked
    2;              // own goals

inline constexpr size_t kTeamStatsWireSize = 7 * 4; // seven uint32 counters

inline constexpr size_t kClockWireSize =
    8 +     // uint8 block
    5 * 2 + // minute, seconds, accumulator, end_game, stoppage
    2;      // trailing pads

inline constexpr size_t kSurfaceWireSize = 3 * 2 + 2; // three int16 + pad

inline constexpr size_t kSideWireSize =
    kTeamSheetWireSize + kTeamControlWireSize +
    kMatchSquadSize * kSquadPlayerWireSize + kTacticsWireSize +
    kTeamStatsWireSize;

// tick(4)+phase(1)+input(4)+score(2)+last_roll(1) + arena + sides×2 + globals +
// clock + surface + 3×RngStream(4) + hash(8)
inline constexpr size_t kRecordPrefixSize = 4 + 1 + 4 + 2 + 1;
inline constexpr size_t kRecordSize =
    kRecordPrefixSize +
    kArenaEntityCount * kEntityWireSize +
    2 * kSideWireSize +
    kGlobalsWireSize +
    kClockWireSize +
    kSurfaceWireSize +
    3 * 4 +
    8;

// Alias kept for tracediff entity classification (first Fix block of an entity).
inline constexpr size_t kEntitySize = kEntityWireSize;

namespace detail {

constexpr void PutU8(std::span<uint8_t> b, size_t& at, uint8_t v) {
    b[at++] = v;
}
constexpr void PutU16(std::span<uint8_t> b, size_t& at, uint16_t v) {
    b[at++] = static_cast<uint8_t>(v & 0xFFu);
    b[at++] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}
constexpr void PutI16(std::span<uint8_t> b, size_t& at, int16_t v) {
    PutU16(b, at, static_cast<uint16_t>(v));
}
constexpr void PutU32(std::span<uint8_t> b, size_t& at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at++] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
}
constexpr void PutU64(std::span<uint8_t> b, size_t& at, uint64_t v) {
    for (int i = 0; i < 8; ++i) b[at++] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
}
constexpr void PutFix(std::span<uint8_t> b, size_t& at, Fix v) {
    PutU32(b, at, static_cast<uint32_t>(v.Raw()));
}
constexpr void PutI8(std::span<uint8_t> b, size_t& at, int8_t v) {
    PutU8(b, at, static_cast<uint8_t>(v));
}
constexpr void PutBytes(std::span<uint8_t> b, size_t& at, const char* p, size_t n) {
    for (size_t i = 0; i < n; ++i) PutU8(b, at, static_cast<uint8_t>(p[i]));
}

constexpr uint8_t GetU8(std::span<const uint8_t> b, size_t& at) {
    return b[at++];
}
constexpr uint16_t GetU16(std::span<const uint8_t> b, size_t& at) {
    const uint16_t lo = b[at++];
    const uint16_t hi = b[at++];
    return static_cast<uint16_t>(lo | (hi << 8));
}
constexpr int16_t GetI16(std::span<const uint8_t> b, size_t& at) {
    return static_cast<int16_t>(GetU16(b, at));
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
constexpr Fix GetFix(std::span<const uint8_t> b, size_t& at) {
    return Fix::FromRaw(static_cast<int32_t>(GetU32(b, at)));
}
constexpr int8_t GetI8(std::span<const uint8_t> b, size_t& at) {
    return static_cast<int8_t>(GetU8(b, at));
}
constexpr void GetBytes(std::span<const uint8_t> b, size_t& at, char* p, size_t n) {
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<char>(GetU8(b, at));
}

constexpr void PutEntity(std::span<uint8_t> b, size_t& at, const Entity& e) {
    PutFix(b, at, e.pos.x);
    PutFix(b, at, e.pos.y);
    PutFix(b, at, e.pos.z);
    PutFix(b, at, e.delta.x);
    PutFix(b, at, e.delta.y);
    PutFix(b, at, e.delta.z);
    PutI16(b, at, e.dest_x);
    PutI16(b, at, e.dest_y);
    PutI16(b, at, e.team_number);
    PutI16(b, at, e.player_ordinal);
    PutI16(b, at, e.frame_offset);
    PutI16(b, at, e.starting_direction);
    PutI16(b, at, e.frame_index);
    PutI16(b, at, e.frame_delay);
    PutI16(b, at, e.cycle_frames_timer);
    PutI16(b, at, e.frame_switch_counter);
    PutI16(b, at, e.direction);
    PutI16(b, at, e.speed);
    PutI16(b, at, e.full_direction);
    PutI16(b, at, e.player_direction);
    PutI16(b, at, e.visible);
    PutI16(b, at, e.image_index);
    PutI16(b, at, e.save_sprite);
    PutI16(b, at, e.on_screen);
    PutI16(b, at, e.is_moving);
    PutI16(b, at, e.tackle_state);
    PutI16(b, at, e.heading);
    PutI16(b, at, e.dest_reached_state);
    PutI16(b, at, e.cards);
    PutI16(b, at, e.injury_level);
    PutI16(b, at, e.tackling_timer);
    PutI16(b, at, e.sent_away);
    PutU32(b, at, static_cast<uint32_t>(e.ball_distance));
    PutU8(b, at, e.player_state);
    PutI8(b, at, e.player_down_timer);
}

constexpr Entity GetEntity(std::span<const uint8_t> b, size_t& at) {
    Entity e;
    e.pos.x = GetFix(b, at);
    e.pos.y = GetFix(b, at);
    e.pos.z = GetFix(b, at);
    e.delta.x = GetFix(b, at);
    e.delta.y = GetFix(b, at);
    e.delta.z = GetFix(b, at);
    e.dest_x = GetI16(b, at);
    e.dest_y = GetI16(b, at);
    e.team_number = GetI16(b, at);
    e.player_ordinal = GetI16(b, at);
    e.frame_offset = GetI16(b, at);
    e.starting_direction = GetI16(b, at);
    e.frame_index = GetI16(b, at);
    e.frame_delay = GetI16(b, at);
    e.cycle_frames_timer = GetI16(b, at);
    e.frame_switch_counter = GetI16(b, at);
    e.direction = GetI16(b, at);
    e.speed = GetI16(b, at);
    e.full_direction = GetI16(b, at);
    e.player_direction = GetI16(b, at);
    e.visible = GetI16(b, at);
    e.image_index = GetI16(b, at);
    e.save_sprite = GetI16(b, at);
    e.on_screen = GetI16(b, at);
    e.is_moving = GetI16(b, at);
    e.tackle_state = GetI16(b, at);
    e.heading = GetI16(b, at);
    e.dest_reached_state = GetI16(b, at);
    e.cards = GetI16(b, at);
    e.injury_level = GetI16(b, at);
    e.tackling_timer = GetI16(b, at);
    e.sent_away = GetI16(b, at);
    e.ball_distance = static_cast<int32_t>(GetU32(b, at));
    e.player_state = GetU8(b, at);
    e.player_down_timer = GetI8(b, at);
    return e;
}

constexpr void PutKit(std::span<uint8_t> b, size_t& at, const KitSpec& k) {
    PutU8(b, at, k.shirt_type);
    PutU8(b, at, k.stripes);
    PutU8(b, at, k.shirt);
    PutU8(b, at, k.shorts);
    PutU8(b, at, k.socks);
}

constexpr KitSpec GetKit(std::span<const uint8_t> b, size_t& at) {
    KitSpec k;
    k.shirt_type = GetU8(b, at);
    k.stripes    = GetU8(b, at);
    k.shirt      = GetU8(b, at);
    k.shorts     = GetU8(b, at);
    k.socks      = GetU8(b, at);
    return k;
}

constexpr void PutSheet(std::span<uint8_t> b, size_t& at, const TeamSheet& s) {
    PutBytes(b, at, s.name.data(), kMatchNameLen);
    PutU8(b, at, s.tactics_id);
    PutKit(b, at, s.primary);
    PutKit(b, at, s.secondary);
}

constexpr void GetSheet(std::span<const uint8_t> b, size_t& at, TeamSheet& s) {
    GetBytes(b, at, s.name.data(), kMatchNameLen);
    s.tactics_id = GetU8(b, at);
    s.primary    = GetKit(b, at);
    s.secondary  = GetKit(b, at);
}

constexpr void PutControl(std::span<uint8_t> b, size_t& at, const TeamControl& c) {
    PutU8(b, at, c.player_number);
    PutU8(b, at, c.player_coach_number);
    PutU8(b, at, c.is_pl_coach);
    PutU8(b, at, c.team_number);
    PutI8(b, at, c.controlled_slot);
    PutI8(b, at, c.pass_to_slot);
    PutI8(b, at, c.last_heading_tackling_slot);
    PutI8(b, at, c.passing_kicking_slot);
    PutI16(b, at, c.update_player_index);
    PutI16(b, at, c.player_has_ball);
    PutI16(b, at, c.allowed_directions);
    PutI16(b, at, c.current_allowed_direction);
    PutI16(b, at, c.direction);
    PutU8(b, at, c.quick_fire);
    PutU8(b, at, c.normal_fire);
    PutU8(b, at, c.fire_pressed);
    PutU8(b, at, c.fire_this_frame);
    PutI16(b, at, c.header_or_tackle);
    PutI16(b, at, c.fire_counter);
    PutI16(b, at, c.controlled_pl_direction);
    PutI16(b, at, c.shooting);
    PutU8(b, at, c.pl_very_close_to_ball);
    PutU8(b, at, c.pl_close_to_ball);
    PutU8(b, at, c.pl_not_far_from_ball);
    PutU8(b, at, c.ball_less_equal_4);
    PutU8(b, at, c.ball_4_to_8);
    PutU8(b, at, c.ball_8_to_12);
    PutU8(b, at, c.ball_12_to_17);
    PutU8(b, at, c.ball_above_17);
    PutU8(b, at, c.prev_pl_very_close_to_ball);
    PutI16(b, at, c.goalkeeper_saved_comment_timer);
    PutI16(b, at, c.goalkeeper_diving_right);
    PutI16(b, at, c.goalkeeper_diving_left);
    PutI16(b, at, c.ball_out_of_play_or_keeper);
    PutI16(b, at, c.goalie_playing_or_out);
    PutI16(b, at, c.passing_ball);
    PutI16(b, at, c.passing_to_player);
    PutI16(b, at, c.player_switch_timer);
    PutI16(b, at, c.ball_in_play);
    PutI16(b, at, c.ball_out_of_play);
    PutI16(b, at, c.ball_x);
    PutI16(b, at, c.ball_y);
    PutI16(b, at, c.pass_kick_timer);
    PutI16(b, at, c.ball_can_be_controlled);
    PutI16(b, at, c.ball_controlling_player_direction);
    PutI16(b, at, c.spin_timer);
    PutI16(b, at, c.left_spin);
    PutI16(b, at, c.right_spin);
    PutI16(b, at, c.long_pass);
    PutI16(b, at, c.long_spin_pass);
    PutI16(b, at, c.pass_in_progress);
    PutI16(b, at, c.ai_timer);
    PutI16(b, at, c.ai_aftertouch_strength);
    PutI16(b, at, c.ai_ball_spin_direction);
    PutI16(b, at, c.won_the_ball_timer);
    PutI16(b, at, c.goalkeeper_playing);
    PutI16(b, at, c.reset_controls);
    PutU8(b, at, c.secondary_fire);
}

constexpr void GetControl(std::span<const uint8_t> b, size_t& at, TeamControl& c) {
    c.player_number = GetU8(b, at);
    c.player_coach_number = GetU8(b, at);
    c.is_pl_coach = GetU8(b, at);
    c.team_number = GetU8(b, at);
    c.controlled_slot = GetI8(b, at);
    c.pass_to_slot = GetI8(b, at);
    c.last_heading_tackling_slot = GetI8(b, at);
    c.passing_kicking_slot = GetI8(b, at);
    c.update_player_index = GetI16(b, at);
    c.player_has_ball = GetI16(b, at);
    c.allowed_directions = GetI16(b, at);
    c.current_allowed_direction = GetI16(b, at);
    c.direction = GetI16(b, at);
    c.quick_fire = GetU8(b, at);
    c.normal_fire = GetU8(b, at);
    c.fire_pressed = GetU8(b, at);
    c.fire_this_frame = GetU8(b, at);
    c.header_or_tackle = GetI16(b, at);
    c.fire_counter = GetI16(b, at);
    c.controlled_pl_direction = GetI16(b, at);
    c.shooting = GetI16(b, at);
    c.pl_very_close_to_ball = GetU8(b, at);
    c.pl_close_to_ball = GetU8(b, at);
    c.pl_not_far_from_ball = GetU8(b, at);
    c.ball_less_equal_4 = GetU8(b, at);
    c.ball_4_to_8 = GetU8(b, at);
    c.ball_8_to_12 = GetU8(b, at);
    c.ball_12_to_17 = GetU8(b, at);
    c.ball_above_17 = GetU8(b, at);
    c.prev_pl_very_close_to_ball = GetU8(b, at);
    c.goalkeeper_saved_comment_timer = GetI16(b, at);
    c.goalkeeper_diving_right = GetI16(b, at);
    c.goalkeeper_diving_left = GetI16(b, at);
    c.ball_out_of_play_or_keeper = GetI16(b, at);
    c.goalie_playing_or_out = GetI16(b, at);
    c.passing_ball = GetI16(b, at);
    c.passing_to_player = GetI16(b, at);
    c.player_switch_timer = GetI16(b, at);
    c.ball_in_play = GetI16(b, at);
    c.ball_out_of_play = GetI16(b, at);
    c.ball_x = GetI16(b, at);
    c.ball_y = GetI16(b, at);
    c.pass_kick_timer = GetI16(b, at);
    c.ball_can_be_controlled = GetI16(b, at);
    c.ball_controlling_player_direction = GetI16(b, at);
    c.spin_timer = GetI16(b, at);
    c.left_spin = GetI16(b, at);
    c.right_spin = GetI16(b, at);
    c.long_pass = GetI16(b, at);
    c.long_spin_pass = GetI16(b, at);
    c.pass_in_progress = GetI16(b, at);
    c.ai_timer = GetI16(b, at);
    c.ai_aftertouch_strength = GetI16(b, at);
    c.ai_ball_spin_direction = GetI16(b, at);
    c.won_the_ball_timer = GetI16(b, at);
    c.goalkeeper_playing = GetI16(b, at);
    c.reset_controls = GetI16(b, at);
    c.secondary_fire = GetU8(b, at);
}

constexpr void PutAttrs(std::span<uint8_t> b, size_t& at, const PlayerAttrs& a) {
    PutU8(b, at, a.passing);
    PutU8(b, at, a.shooting);
    PutU8(b, at, a.heading);
    PutU8(b, at, a.tackling);
    PutU8(b, at, a.ball_control);
    PutU8(b, at, a.speed);
    PutU8(b, at, a.finishing);
}

constexpr void GetAttrs(std::span<const uint8_t> b, size_t& at, PlayerAttrs& a) {
    a.passing = GetU8(b, at);
    a.shooting = GetU8(b, at);
    a.heading = GetU8(b, at);
    a.tackling = GetU8(b, at);
    a.ball_control = GetU8(b, at);
    a.speed = GetU8(b, at);
    a.finishing = GetU8(b, at);
}

constexpr void PutSquad(std::span<uint8_t> b, size_t& at, const SquadPlayer& p) {
    PutU8(b, at, p.substituted);
    PutU8(b, at, p.index);
    PutU8(b, at, p.goals_scored);
    PutU8(b, at, p.shirt_number);
    PutU8(b, at, p.position);
    PutU8(b, at, p.face);
    PutU8(b, at, p.is_injured);
    PutU8(b, at, p.cards);
    PutBytes(b, at, p.short_name.data(), kShortNameLen);
    PutAttrs(b, at, p.attrs);
    PutU8(b, at, p.goalie_skill);
    PutU8(b, at, p.injuries_bitfield);
    PutU8(b, at, p.half_played);
    PutU8(b, at, p.face2);
    PutBytes(b, at, p.full_name.data(), kMatchNameLen);
}

constexpr void GetSquad(std::span<const uint8_t> b, size_t& at, SquadPlayer& p) {
    p.substituted = GetU8(b, at);
    p.index = GetU8(b, at);
    p.goals_scored = GetU8(b, at);
    p.shirt_number = GetU8(b, at);
    p.position = GetU8(b, at);
    p.face = GetU8(b, at);
    p.is_injured = GetU8(b, at);
    p.cards = GetU8(b, at);
    GetBytes(b, at, p.short_name.data(), kShortNameLen);
    GetAttrs(b, at, p.attrs);
    p.goalie_skill = GetU8(b, at);
    p.injuries_bitfield = GetU8(b, at);
    p.half_played = GetU8(b, at);
    p.face2 = GetU8(b, at);
    GetBytes(b, at, p.full_name.data(), kMatchNameLen);
}

constexpr void PutTactics(std::span<uint8_t> b, size_t& at, const TacticsSnapshot& t) {
    PutU8(b, at, t.out_of_play);
    for (const auto& row : t.cells)
        for (uint8_t cell : row) PutU8(b, at, cell);
}

constexpr void GetTactics(std::span<const uint8_t> b, size_t& at, TacticsSnapshot& t) {
    t.out_of_play = GetU8(b, at);
    for (auto& row : t.cells)
        for (uint8_t& cell : row) cell = GetU8(b, at);
}

constexpr void PutStats(std::span<uint8_t> b, size_t& at, const TeamStats& t) {
    PutU32(b, at, t.possession);
    PutU32(b, at, t.corners_won);
    PutU32(b, at, t.fouls_conceded);
    PutU32(b, at, t.bookings);
    PutU32(b, at, t.sendings_off);
    PutU32(b, at, t.goal_attempts);
    PutU32(b, at, t.on_target);
}

constexpr void GetStats(std::span<const uint8_t> b, size_t& at, TeamStats& t) {
    t.possession     = GetU32(b, at);
    t.corners_won    = GetU32(b, at);
    t.fouls_conceded = GetU32(b, at);
    t.bookings       = GetU32(b, at);
    t.sendings_off   = GetU32(b, at);
    t.goal_attempts  = GetU32(b, at);
    t.on_target      = GetU32(b, at);
}

constexpr void PutSide(std::span<uint8_t> b, size_t& at, const MatchSide& s) {
    PutSheet(b, at, s.sheet);
    PutControl(b, at, s.control);
    for (const auto& p : s.squad) PutSquad(b, at, p);
    PutTactics(b, at, s.tactics);
    PutStats(b, at, s.stats);
}

constexpr void GetSide(std::span<const uint8_t> b, size_t& at, MatchSide& s) {
    GetSheet(b, at, s.sheet);
    GetControl(b, at, s.control);
    for (auto& p : s.squad) GetSquad(b, at, p);
    GetTactics(b, at, s.tactics);
    GetStats(b, at, s.stats);
}

constexpr void PutClock(std::span<uint8_t> b, size_t& at, const MatchClock& c) {
    PutU8(b, at, c.game_length);
    PutU8(b, at, c.period);
    PutU8(b, at, c.last_team_played);
    PutU8(b, at, c.match_started);
    PutU8(b, at, c.allow_extra_time);
    PutU8(b, at, c._pad0);
    PutU8(b, at, c._pad1);
    PutU8(b, at, c._pad2);
    PutI16(b, at, c.displayed_minute);
    PutI16(b, at, c.game_seconds);
    PutI16(b, at, c.seconds_accumulator);
    PutI16(b, at, c.end_game_counter);
    PutI16(b, at, c.stoppage_event_timer);
    PutU8(b, at, c._pad3);
    PutU8(b, at, c._pad4);
}

constexpr void GetClock(std::span<const uint8_t> b, size_t& at, MatchClock& c) {
    c.game_length = GetU8(b, at);
    c.period = GetU8(b, at);
    c.last_team_played = GetU8(b, at);
    c.match_started = GetU8(b, at);
    c.allow_extra_time = GetU8(b, at);
    c._pad0 = GetU8(b, at);
    c._pad1 = GetU8(b, at);
    c._pad2 = GetU8(b, at);
    c.displayed_minute = GetI16(b, at);
    c.game_seconds = GetI16(b, at);
    c.seconds_accumulator = GetI16(b, at);
    c.end_game_counter = GetI16(b, at);
    c.stoppage_event_timer = GetI16(b, at);
    c._pad3 = GetU8(b, at);
    c._pad4 = GetU8(b, at);
}

constexpr void PutSurface(std::span<uint8_t> b, size_t& at, const MatchSurface& s) {
    PutI16(b, at, s.pitch_ball_speed_factor);
    PutI16(b, at, s.ball_speed_bounce_factor);
    PutI16(b, at, s.ball_bounce_factor);
    PutI16(b, at, s._pad);
}

constexpr void GetSurface(std::span<const uint8_t> b, size_t& at, MatchSurface& s) {
    s.pitch_ball_speed_factor  = GetI16(b, at);
    s.ball_speed_bounce_factor = GetI16(b, at);
    s.ball_bounce_factor       = GetI16(b, at);
    s._pad                     = GetI16(b, at);
}

constexpr void PutGlobals(std::span<uint8_t> b, size_t& at, const MatchGlobals& g) {
    PutU8(b, at, g.game_state);
    PutU8(b, at, g.game_state_pl);
    PutU8(b, at, g.team_starting);
    PutU8(b, at, g.team_playing_up);
    PutU8(b, at, g.team_switch_counter);
    PutU8(b, at, g.hide_ball);
    PutU8(b, at, g.camera_direction);
    PutU8(b, at, g.player_turn_flags);
    PutU8(b, at, g.last_team_played_before_break);
    PutU8(b, at, g.break_camera_mode);
    PutU8(b, at, g.ref_state);
    PutU8(b, at, g.which_card);
    PutI8(b, at, g.booked_player);
    PutU8(b, at, g.last_team_booked);
    PutI16(b, at, g.foul_x);
    PutI16(b, at, g.foul_y);
    PutI16(b, at, g.ball_next_x);
    PutI16(b, at, g.ball_next_y);
    PutI16(b, at, g.ball_next_y_ground_y);
    PutI16(b, at, g.ref_timer);
    PutU8(b, at, g.substitute_in_progress);
    PutU8(b, at, g.team_that_substitutes);
    PutI16(b, at, g.wait_for_player_to_go_in_timer);
    PutI16(b, at, g.show_fans_counter);
    PutI16(b, at, g.marked_player_home);
    PutI16(b, at, g.marked_player_away);
    PutU8(b, at, g.num_own_goals_home);
    PutU8(b, at, g.num_own_goals_away);
}

constexpr void GetGlobals(std::span<const uint8_t> b, size_t& at, MatchGlobals& g) {
    g.game_state = GetU8(b, at);
    g.game_state_pl = GetU8(b, at);
    g.team_starting = GetU8(b, at);
    g.team_playing_up = GetU8(b, at);
    g.team_switch_counter = GetU8(b, at);
    g.hide_ball = GetU8(b, at);
    g.camera_direction = GetU8(b, at);
    g.player_turn_flags = GetU8(b, at);
    g.last_team_played_before_break = GetU8(b, at);
    g.break_camera_mode = GetU8(b, at);
    g.ref_state = GetU8(b, at);
    g.which_card = GetU8(b, at);
    g.booked_player = GetI8(b, at);
    g.last_team_booked = GetU8(b, at);
    g.foul_x = GetI16(b, at);
    g.foul_y = GetI16(b, at);
    g.ball_next_x = GetI16(b, at);
    g.ball_next_y = GetI16(b, at);
    g.ball_next_y_ground_y = GetI16(b, at);
    g.ref_timer = GetI16(b, at);
    g.substitute_in_progress = GetU8(b, at);
    g.team_that_substitutes = GetU8(b, at);
    g.wait_for_player_to_go_in_timer = GetI16(b, at);
    g.show_fans_counter = GetI16(b, at);
    g.marked_player_home = GetI16(b, at);
    g.marked_player_away = GetI16(b, at);
    g.num_own_goals_home = GetU8(b, at);
    g.num_own_goals_away = GetU8(b, at);
}

constexpr void PutRng(std::span<uint8_t> b, size_t& at, const RngStream& r) {
    PutU8(b, at, r.seed);
    PutU8(b, at, r.xor_key);
    PutU8(b, at, r.xor_index);
    PutU8(b, at, r._pad);
}

constexpr void GetRng(std::span<const uint8_t> b, size_t& at, RngStream& r) {
    r.seed = GetU8(b, at);
    r.xor_key = GetU8(b, at);
    r.xor_index = GetU8(b, at);
    r._pad = GetU8(b, at);
}

} // namespace detail

constexpr uint64_t HashBytes(std::span<const uint8_t> bytes) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

constexpr size_t SerializeHeader(const Header& h, std::span<uint8_t> out) {
    if (out.size() < kHeaderSize) return 0;
    size_t at = 0;
    detail::PutU32(out, at, kMagic);
    detail::PutU16(out, at, kFormatVersion);
    detail::PutU16(out, at, static_cast<uint16_t>(kRecordSize));
    detail::PutU32(out, at, h.seed);
    detail::PutU32(out, at, h.record_count);
    detail::PutU8(out, at, static_cast<uint8_t>(h.profile));
    detail::PutU8(out, at, h.first_team);
    detail::PutU8(out, at, h.tick_hz);
    detail::PutU8(out, at, 0);
    detail::PutU32(out, at, 0);
    return at;
}

constexpr bool DeserializeHeader(std::span<const uint8_t> in, Header& h) {
    if (in.size() < kHeaderSize) return false;
    size_t at = 0;
    if (detail::GetU32(in, at) != kMagic) return false;
    if (detail::GetU16(in, at) != kFormatVersion) return false;
    if (detail::GetU16(in, at) != static_cast<uint16_t>(kRecordSize)) return false;
    h.seed         = detail::GetU32(in, at);
    h.record_count = detail::GetU32(in, at);
    h.profile      = static_cast<Profile>(detail::GetU8(in, at));
    h.first_team   = detail::GetU8(in, at);
    h.tick_hz      = detail::GetU8(in, at);
    return true;
}

constexpr size_t SerializeRecord(const MatchState& s, const MatchInput& in,
                                 std::span<uint8_t> out) {
    if (out.size() < kRecordSize) return 0;
    size_t at = 0;
    detail::PutU32(out, at, s.tick);
    detail::PutU8(out, at, static_cast<uint8_t>(s.phase));
    detail::PutU8(out, at, static_cast<uint8_t>(static_cast<int8_t>(in.p1.dir)));
    detail::PutU8(out, at, static_cast<uint8_t>(in.p1.fire ? 1 : 0));
    detail::PutU8(out, at, static_cast<uint8_t>(static_cast<int8_t>(in.p2.dir)));
    detail::PutU8(out, at, static_cast<uint8_t>(in.p2.fire ? 1 : 0));
    detail::PutU8(out, at, s.score[0]);
    detail::PutU8(out, at, s.score[1]);
    detail::PutU8(out, at, s.last_roll);

    detail::PutEntity(out, at, s.ball);
    for (const Entity& p : s.players) detail::PutEntity(out, at, p);
    detail::PutEntity(out, at, s.referee);
    detail::PutEntity(out, at, s.booked_indicator);

    for (const MatchSide& side : s.sides) detail::PutSide(out, at, side);
    detail::PutGlobals(out, at, s.globals);
    detail::PutClock(out, at, s.clock);
    detail::PutSurface(out, at, s.surface);
    detail::PutRng(out, at, s.gameplay_rng);
    detail::PutRng(out, at, s.presentation_rng);
    detail::PutRng(out, at, s.resolve_rng);

    const uint64_t h = HashBytes(out.subspan(0, at));
    detail::PutU64(out, at, h);
    return at;
}

constexpr bool DeserializeRecord(std::span<const uint8_t> in, MatchState& s,
                                 MatchInput& input) {
    if (in.size() < kRecordSize) return false;
    size_t at = 0;
    s.tick  = detail::GetU32(in, at);
    s.phase = static_cast<MatchPhase>(detail::GetU8(in, at));
    input.p1.dir  = static_cast<Dir>(static_cast<int8_t>(detail::GetU8(in, at)));
    input.p1.fire = detail::GetU8(in, at) != 0;
    input.p2.dir  = static_cast<Dir>(static_cast<int8_t>(detail::GetU8(in, at)));
    input.p2.fire = detail::GetU8(in, at) != 0;
    s.score[0] = detail::GetU8(in, at);
    s.score[1] = detail::GetU8(in, at);
    s.last_roll = detail::GetU8(in, at);

    s.ball = detail::GetEntity(in, at);
    for (Entity& p : s.players) p = detail::GetEntity(in, at);
    s.referee = detail::GetEntity(in, at);
    s.booked_indicator = detail::GetEntity(in, at);

    for (MatchSide& side : s.sides) detail::GetSide(in, at, side);
    detail::GetGlobals(in, at, s.globals);
    detail::GetClock(in, at, s.clock);
    detail::GetSurface(in, at, s.surface);
    detail::GetRng(in, at, s.gameplay_rng);
    detail::GetRng(in, at, s.presentation_rng);
    detail::GetRng(in, at, s.resolve_rng);

    const uint64_t want = HashBytes(in.subspan(0, at));
    const uint64_t got  = detail::GetU64(in, at);
    return want == got;
}

constexpr uint64_t RecordHash(std::span<const uint8_t> record) {
    size_t at = kRecordSize - 8;
    return detail::GetU64(record, at);
}

} // namespace at::trace
