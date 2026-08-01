#pragma once
#include "core/fixed.hpp"
#include "core/rng.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace at {

// Explicit pads keep has_unique_object_representations — required for HashState.
// See doc/implementation/A2-determinism-primitives.md §2.6 and
// doc/implementation/B1-state-layout.md.

inline constexpr int kPitchPlayers  = 22;
inline constexpr int kMatchSquadSize = 16;
inline constexpr int kMatchTacticRoles = 10;
inline constexpr int kMatchBallQuadrants = 35;
inline constexpr int kMatchNameLen = 24;
inline constexpr int kShortNameLen = 16; // 15 usable + pad for unique-rep

// ---------------------------------------------------------------------------
// PlayerState — STATE.md §2
// ---------------------------------------------------------------------------

enum class PlayerState : uint8_t {
    Normal             = 0,
    Tackling           = 1,
    // 2 unused in the reference
    Tackled            = 3,
    GoalieCatchingBall = 4,
    ThrowIn            = 5,
    GoalieDivingHigh   = 6,
    GoalieDivingLow    = 7,
    StaticHeader       = 8,
    JumpHeader         = 9,
    Down               = 10,
    GoalieClaimed      = 11,
    Booked             = 12,
    Injured            = 13,
    Sad                = 14,
    Happy              = 15,
    Unknown            = 255
};

enum class MatchPhase : uint8_t {
    KickOff, InPlay, Goal, HalfTime, FullTime
};

// SIMULATION.md §2 — coarse mode (gameStatePl).
enum class GameStatePl : uint8_t {
    InProgress      = 100,
    Stopped         = 101,
    WaitingOnPlayer = 102
};

// SIMULATION.md §2 — situation (gameState). Full 0–31 table.
enum class GameState : uint8_t {
    PlayersToInitialPositions = 0,
    GoalOutLeft               = 1,
    GoalOutRight              = 2,
    KeeperHoldsBall           = 3,
    CornerLeft                = 4,
    CornerRight               = 5,
    FreeKickLeft1             = 6,
    FreeKickLeft2             = 7,
    FreeKickLeft3             = 8,
    FreeKickCentre            = 9,
    FreeKickRight1            = 10,
    FreeKickRight2            = 11,
    FreeKickRight3            = 12,
    Foul                      = 13,
    Penalty                   = 14,
    ThrowInForwardRight       = 15,
    ThrowInCentreRight        = 16,
    ThrowInBackRight          = 17,
    ThrowInForwardLeft        = 18,
    ThrowInCentreLeft         = 19,
    ThrowInBackLeft           = 20,
    StartingGame              = 21,
    CameraGoingToShowers      = 22,
    GoingToHalfTime           = 23,
    PlayersGoingToShower      = 24,
    ResultOnHalfTime          = 25,
    ResultAfterGame           = 26,
    FirstExtraStarting        = 27,
    FirstExtraEnded           = 28,
    FirstHalfEnded            = 29,
    GameEnded                 = 30,
    Penalties                 = 31
};

// Pitch geometry constants used by clock injury-time and OOP helpers (SIMULATION §5).
inline constexpr int16_t kCentreSpotX     = 336;
inline constexpr int16_t kCentreSpotY     = 449;
inline constexpr int16_t kPlayableMinX    = 81;
inline constexpr int16_t kPlayableMaxX    = 590;
inline constexpr int16_t kPlayableMinY    = 129;
inline constexpr int16_t kPlayableMaxY    = 769;
inline constexpr int16_t kPenaltyBoxTopY  = 216;
inline constexpr int16_t kPenaltyBoxBotY  = 682;
inline constexpr int16_t kGoalMouthMinX   = 303;
inline constexpr int16_t kGoalMouthMaxX   = 367;

// ---------------------------------------------------------------------------
// Entity — Sprite semantics (STATE.md §2). Named fields only.
// Citation: Sprite.x → pos, Sprite.deltaX → delta, Sprite.speed → speed, …
// ---------------------------------------------------------------------------

struct Entity {
    Vec3    pos{};
    Vec3    delta{};                 // per-tick increment (Sprite.deltaX/Y/Z)
    int16_t dest_x = 0;              // whole units
    int16_t dest_y = 0;
    int16_t team_number = 0;         // 0 CPU, 1/2 sides, 3 referee
    int16_t player_ordinal = 0;      // 1–11; 1 = GK; 0 non-players
    int16_t frame_offset = 0;
    int16_t starting_direction = 0;
    int16_t frame_index = 0;
    int16_t frame_delay = 0;
    int16_t cycle_frames_timer = 0;
    int16_t frame_switch_counter = 0;
    int16_t direction = 0;           // octant 0–7
    int16_t speed = 0;
    int16_t full_direction = 0;      // 0–255
    int16_t player_direction = -1;   // -1 non-players
    int16_t visible = 1;
    int16_t image_index = -1;        // < 0 = none
    int16_t save_sprite = 0;
    int16_t on_screen = 0;
    int16_t is_moving = 0;
    int16_t tackle_state = 0;
    int16_t heading = 0;             // header contact (asm name; not unk009)
    int16_t dest_reached_state = 0;
    int16_t cards = 0;               // -1 = sent off
    int16_t injury_level = 0;
    int16_t tackling_timer = 0;      // signed; negative = ending
    int16_t sent_away = 0;
    int32_t ball_distance = 0;
    uint8_t player_state = static_cast<uint8_t>(PlayerState::Normal);
    int8_t  player_down_timer = 0;
    uint8_t _pad0 = 0;
    uint8_t _pad1 = 0;
};

static_assert(std::has_unique_object_representations_v<Entity>);

// ---------------------------------------------------------------------------
// Attrs / kits / sheet (A5 projection surface)
// ---------------------------------------------------------------------------

struct PlayerAttrs {
    uint8_t passing      = 0;
    uint8_t shooting     = 0;
    uint8_t heading      = 0;
    uint8_t tackling     = 0;
    uint8_t ball_control = 0;
    uint8_t speed        = 0;
    uint8_t finishing    = 0;
    uint8_t _pad         = 0;
};

static_assert(sizeof(PlayerAttrs) == 8);
static_assert(std::has_unique_object_representations_v<PlayerAttrs>);

struct KitSpec {
    uint8_t shirt_type = 0;
    uint8_t stripes    = 0;
    uint8_t shirt      = 0;
    uint8_t shorts     = 0;
    uint8_t socks      = 0;
    uint8_t _pad0      = 0;
    uint8_t _pad1      = 0;
    uint8_t _pad2      = 0;
};

static_assert(sizeof(KitSpec) == 8);
static_assert(std::has_unique_object_representations_v<KitSpec>);

struct TeamSheet {
    std::array<char, kMatchNameLen> name{};
    uint8_t                    tactics_id = 0;
    uint8_t                    _pad0      = 0;
    uint8_t                    _pad1      = 0;
    uint8_t                    _pad2      = 0;
    KitSpec                    primary{};
    KitSpec                    secondary{};
};

static_assert(sizeof(TeamSheet) == 44);
static_assert(std::has_unique_object_representations_v<TeamSheet>);

// ---------------------------------------------------------------------------
// TeamControl — TeamGeneralInfo semantics (STATE.md §3). Pointer-free.
// ---------------------------------------------------------------------------

struct TeamControl {
    uint8_t player_number = 0;         // 0 = CPU team
    uint8_t player_coach_number = 0;
    uint8_t is_pl_coach = 0;
    uint8_t team_number = 1;           // 1 or 2

    int8_t  controlled_slot = -1;      // index into players[22], -1 = none
    int8_t  pass_to_slot = -1;
    int8_t  last_heading_tackling_slot = -1;
    int8_t  passing_kicking_slot = -1;

    int16_t update_player_index = 0;
    int16_t player_has_ball = 0;

    // Seven-field input surface (INPUT.md §1) + fire_counter / header_or_tackle.
    int16_t allowed_directions = 0;
    int16_t current_allowed_direction = -1; // -1 = nothing held
    int16_t direction = 0;
    uint8_t quick_fire = 0;
    uint8_t normal_fire = 0;
    uint8_t fire_pressed = 0;
    uint8_t fire_this_frame = 0;
    int16_t header_or_tackle = 0;
    int16_t fire_counter = 0;

    // One field, two reference names: controlledPlDirection / allowedPlDirection.
    int16_t controlled_pl_direction = 0;
    int16_t shooting = 0;

    // Proximity bands (CONTROL.md §2)
    uint8_t pl_very_close_to_ball = 0;
    uint8_t pl_close_to_ball = 0;
    uint8_t pl_not_far_from_ball = 0;
    uint8_t ball_less_equal_4 = 0;
    uint8_t ball_4_to_8 = 0;
    uint8_t ball_8_to_12 = 0;
    uint8_t ball_12_to_17 = 0;
    uint8_t ball_above_17 = 0;
    uint8_t prev_pl_very_close_to_ball = 0;
    uint8_t _pad_prox = 0;
    uint8_t _pad_prox1 = 0;
    uint8_t _pad_prox2 = 0;

    int16_t goalkeeper_saved_comment_timer = 0;
    int16_t goalkeeper_diving_right = 0;
    int16_t goalkeeper_diving_left = 0;
    int16_t ball_out_of_play_or_keeper = 0;
    int16_t goalie_playing_or_out = 0;

    int16_t passing_ball = 0;
    int16_t passing_to_player = 0;     // ordinal / role index as reference uses
    int16_t player_switch_timer = 0;
    int16_t ball_in_play = 0;
    int16_t ball_out_of_play = 0;
    int16_t ball_x = 0;                // team's cached ball position
    int16_t ball_y = 0;
    int16_t pass_kick_timer = 0;

    int16_t ball_can_be_controlled = 0;
    int16_t ball_controlling_player_direction = 0;

    int16_t spin_timer = -1;           // -1 = aftertouch inactive
    int16_t left_spin = 0;
    int16_t right_spin = 0;
    int16_t long_pass = 0;
    int16_t long_spin_pass = 0;
    int16_t pass_in_progress = 0;

    int16_t ai_timer = 0;
    int16_t ai_aftertouch_strength = 0;
    int16_t ai_ball_spin_direction = 0;
    int16_t won_the_ball_timer = 0;     // set to 12 on contest win
    int16_t goalkeeper_playing = 0;
    int16_t reset_controls = 0;
    uint8_t secondary_fire = 0;
    uint8_t _pad_end0 = 0;
    uint8_t _pad_end1 = 0;
    uint8_t _pad_end2 = 0;
};

static_assert(std::has_unique_object_representations_v<TeamControl>);

// ---------------------------------------------------------------------------
// SquadPlayer — PlayerInfo (STATE.md §5)
// ---------------------------------------------------------------------------

struct SquadPlayer {
    uint8_t substituted = 0;
    uint8_t index = 0;
    uint8_t goals_scored = 0;
    uint8_t shirt_number = 0;
    uint8_t position = 0;
    uint8_t face = 0;
    uint8_t is_injured = 0;
    uint8_t cards = 0;
    std::array<char, kShortNameLen> short_name{};
    PlayerAttrs attrs{};
    uint8_t goalie_skill = 0;
    uint8_t injuries_bitfield = 0;
    uint8_t half_played = 0;
    uint8_t face2 = 0;
    std::array<char, kMatchNameLen> full_name{};
};

static_assert(std::has_unique_object_representations_v<SquadPlayer>);

// ---------------------------------------------------------------------------
// Tactics snapshot — copied at kickoff (B9 consumes; core never includes data/)
// ---------------------------------------------------------------------------

struct TacticsSnapshot {
    uint8_t out_of_play = 0;
    uint8_t _pad0 = 0;
    uint8_t _pad1 = 0;
    uint8_t _pad2 = 0;
    std::array<std::array<uint8_t, kMatchBallQuadrants>, kMatchTacticRoles> cells{};
    // 4 + 350 = 354; pad to 4-byte so TeamStats (uint32) does not insert holes.
    uint8_t _pad_tail0 = 0;
    uint8_t _pad_tail1 = 0;
};

static_assert(std::has_unique_object_representations_v<TacticsSnapshot>);

// ---------------------------------------------------------------------------
// TeamStats — SIMULATION.md §7 (our layout, not DOS TeamStatsData)
// ---------------------------------------------------------------------------

struct TeamStats {
    uint32_t possession     = 0;
    uint32_t corners_won    = 0;
    uint32_t fouls_conceded = 0;
    uint32_t bookings       = 0;
    uint32_t sendings_off   = 0;
    uint32_t goal_attempts  = 0;
    uint32_t on_target      = 0;
};

static_assert(sizeof(TeamStats) == 28);
static_assert(std::has_unique_object_representations_v<TeamStats>);

// ---------------------------------------------------------------------------
// MatchClock — fixed-step Amiga-profile clock (B2 / SIMULATION.md §3)
// ---------------------------------------------------------------------------

struct MatchClock {
    uint8_t game_length = 0;          // 0..3 → kGameLenSecondsTable
    uint8_t period = 0;               // 0 first half, 1 second half
    uint8_t last_team_played = 0;     // 0 none, 1/2
    uint8_t match_started = 0;
    uint8_t allow_extra_time = 0;     // competition flag; default off
    uint8_t _pad0 = 0;
    uint8_t _pad1 = 0;
    uint8_t _pad2 = 0;
    int16_t displayed_minute = 0;
    int16_t game_seconds = 0;         // -1 during injury-time entry
    int16_t seconds_accumulator = 0;
    int16_t end_game_counter = 0;     // injury-time whistle deferral
    int16_t stoppage_event_timer = 0;
    uint8_t _pad3 = 0;                // 8 + 10 = 18 → pad to 20 for align 4
    uint8_t _pad4 = 0;
};

static_assert(std::has_unique_object_representations_v<MatchClock>);

// ---------------------------------------------------------------------------
// MatchGlobals — named globals only (STATE.md §7).
// ---------------------------------------------------------------------------

struct MatchGlobals {
    uint8_t  game_state = 0;           // GameState
    uint8_t  game_state_pl = 0;        // GameStatePl
    uint8_t  team_starting = 0;
    uint8_t  team_playing_up = 0;
    uint8_t  team_switch_counter = 0;  // A3 §2.4 alternation phase
    uint8_t  hide_ball = 0;
    uint8_t  camera_direction = 0;
    uint8_t  player_turn_flags = 0;
    uint8_t  last_team_played_before_break = 0;
    uint8_t  break_camera_mode = 0;    // written by restarts; unread in reference
    uint8_t  ref_state = 0;
    uint8_t  which_card = 0;
    int8_t   booked_player = -1;       // pitch slot or -1
    uint8_t  last_team_booked = 0;
    int16_t  foul_x = 0;
    int16_t  foul_y = 0;
    int16_t  ball_next_x = 0;
    int16_t  ball_next_y = 0;
    int16_t  ball_next_y_ground_y = 0;
    int16_t  ref_timer = 0;
    uint8_t  substitute_in_progress = 0;
    uint8_t  team_that_substitutes = 0;
    int16_t  wait_for_player_to_go_in_timer = 0;
    int16_t  show_fans_counter = 0;
    int16_t  marked_player_home = -1;  // TeamGame.markedPlayer per side
    int16_t  marked_player_away = -1;
    uint8_t  num_own_goals_home = 0;
    uint8_t  num_own_goals_away = 0;
    uint8_t  _pad0 = 0;
    uint8_t  _pad1 = 0;
};

static_assert(std::has_unique_object_representations_v<MatchGlobals>);

// ---------------------------------------------------------------------------
// MatchSide — sheet + control + squad + tactics
// ---------------------------------------------------------------------------

struct MatchSide {
    TeamSheet       sheet{};
    TeamControl     control{};
    std::array<SquadPlayer, kMatchSquadSize> squad{};
    TacticsSnapshot tactics{};
    TeamStats       stats{};
};

static_assert(std::has_unique_object_representations_v<MatchSide>);

// ---------------------------------------------------------------------------
// MatchState — fixed arena
// ---------------------------------------------------------------------------

struct MatchState {
    uint32_t   tick      = 0;
    MatchPhase phase     = MatchPhase::KickOff;
    uint8_t    last_roll = 0;   // last gameplay RNG draw (A2 gate)
    uint8_t    _pad0     = 0;
    uint8_t    _pad1     = 0;

    Entity                              ball{};
    std::array<Entity, kPitchPlayers>   players{};
    Entity                              referee{};
    Entity                              booked_indicator{};

    std::array<uint8_t, 2> score{0, 0};
    uint8_t                _pad2 = 0;
    uint8_t                _pad3 = 0;

    std::array<MatchSide, 2> sides{};
    MatchGlobals             globals{};
    MatchClock               clock{};

    RngStream gameplay_rng{};
    RngStream presentation_rng{};
    RngStream resolve_rng{};

    // --- accessors (discipline for B2+) ------------------------------------

    Entity&       Ball()             { return ball; }
    const Entity& Ball() const       { return ball; }
    Entity&       Player(int i)      { return players[static_cast<size_t>(i)]; }
    const Entity& Player(int i) const {
        return players[static_cast<size_t>(i)];
    }
    Entity&       Referee()          { return referee; }
    const Entity& Referee() const    { return referee; }
    Entity&       BookedIndicator()  { return booked_indicator; }
    const Entity& BookedIndicator() const { return booked_indicator; }

    MatchSide&       Side(int s)       { return sides[static_cast<size_t>(s)]; }
    const MatchSide& Side(int s) const { return sides[static_cast<size_t>(s)]; }

    TeamControl&       Control(int s)       { return Side(s).control; }
    const TeamControl& Control(int s) const { return Side(s).control; }

    SquadPlayer&       Squad(int s, int i) {
        return Side(s).squad[static_cast<size_t>(i)];
    }
    const SquadPlayer& Squad(int s, int i) const {
        return Side(s).squad[static_cast<size_t>(i)];
    }

    // Controlled pitch entity for a side, or nullptr if slot is unset/OOB.
    Entity* Controlled(int s) {
        const int8_t slot = Control(s).controlled_slot;
        if (slot < 0 || slot >= kPitchPlayers) return nullptr;
        return &players[static_cast<size_t>(slot)];
    }
    const Entity* Controlled(int s) const {
        const int8_t slot = Control(s).controlled_slot;
        if (slot < 0 || slot >= kPitchPlayers) return nullptr;
        return &players[static_cast<size_t>(slot)];
    }
};

static_assert(std::is_trivially_copyable_v<MatchState>);
static_assert(std::has_unique_object_representations_v<MatchState>);

// Legacy alias used by a few call sites during the A2 era.
using EntityState = Entity;

} // namespace at
