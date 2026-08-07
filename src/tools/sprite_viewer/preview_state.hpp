#pragma once
#include "core/animation.hpp"
#include "core/match_state.hpp"
#include "render/asset_source.hpp"

// sprite_viewer's model: one `Entity`, driven by the real stepper.
//
// Everything here is pure -- no SDL, no ImGui, no I/O -- so the input-to-state
// mapping can be pinned by a doctest while the window it drives cannot be. That
// split matters more than usual for this tool: its whole reason to exist is to
// say truthfully what the game will draw, so the part that decides *which*
// animation is playing has to be checkable, and the part that blits it is the
// same DrawIndexedSprite the match renderer calls.
//
// What is deliberately NOT here: any frame table. Those live in
// core/animation_tables.hpp and are measured off the art (see the method note
// at the top of that file). The viewer reads them; it never second-guesses them.

namespace at::spriteview {

// Table mode runs StepEntityAnimation and shows whatever the tables say. Raw
// mode bypasses them and scrubs image_index directly -- which is how you *find*
// the frames for a table that does not exist yet. C3 has 30..53 and 64..100
// still unclassified (header, throw, celebration, booked, injured), so raw mode
// is not a debug affordance, it is the working mode until those are measured.
enum class Mode : uint8_t { Table, Raw };

// The fire button alternates between these two; the mood button between the
// other two. One key each, toggling, rather than four keys.
enum class OneShot : uint8_t { Header, Slide };
enum class Mood : uint8_t { Happy, Sad };

inline constexpr int kNoDirection = -1;

struct Controls {
    int  direction    = kNoDirection;  // held octant 0..7, or kNoDirection
    bool fire_pressed = false;         // edge, not level
    bool mood_pressed = false;         // edge, not level
};

struct Config {
    // One-shot lists end in kHold, so they freeze forever on their own. Without
    // a return-to-idle the preview locks on the first fire press.
    int16_t oneshot_ticks = 30;
    int16_t run_speed     = 6;   // any non-zero value picks the running table
};

struct Preview {
    Entity  e{};
    int16_t oneshot_timer = 0;
    OneShot next_oneshot  = OneShot::Header;
    Mood    next_mood     = Mood::Happy;
    int16_t raw_frame     = 0;
    bool    keeper        = false;
    uint8_t shirt_type    = 0;   // DATA.md ShirtType; picks the geometry bank
};

// Four axis keys to an octant (0 = N, clockwise). Opposite pairs cancel, so
// up+down held is the same as nothing held rather than an arbitrary winner.
inline int DirectionFromAxes(bool up, bool down, bool left, bool right) {
    if (up && down) { up = down = false; }
    if (left && right) { left = right = false; }
    if (up && right) return 1;
    if (down && right) return 3;
    if (down && left) return 5;
    if (up && left) return 7;
    if (up) return 0;
    if (right) return 2;
    if (down) return 4;
    if (left) return 6;
    return kNoDirection;
}

// Restart the current frame list from the top. The engine only rewinds when the
// cursor falls outside the new list, but a state change here is always meant to
// replay the animation from its first frame -- which is what the reference's
// setPlayerAnimationTableAndPictureIndex does. frame_delay is left alone: a list
// that sets its own speed should be previewed at that speed.
inline void RestartSequence(Entity& e) {
    e.frame_index        = -1;
    e.cycle_frames_timer = 0;
}

inline void Reset(Preview& p) {
    const bool    keeper = p.keeper;
    const uint8_t shirt  = p.shirt_type;
    p = Preview{};
    p.keeper     = keeper;
    p.shirt_type = shirt;
    // player_ordinal == 1 is what SelectAnimationTable reads to pick the keeper
    // tables, so the keeper toggle drives the table and the bank from one flag.
    p.e.player_ordinal = keeper ? 1 : 2;
    p.e.direction      = 4;   // S: facing the camera reads best as a default
    p.e.frame_delay    = 5;   // Sprite.h init default
    p.e.frame_index    = -1;
    p.e.image_index    = -1;
    p.e.visible        = 1;
    p.e.player_state   = static_cast<uint8_t>(PlayerState::Normal);
}

// Fold this frame's input into the entity. Direction is applied even while a
// one-shot is playing, so a header can be aimed; only the timer ends a one-shot.
// Fire and mood on the same frame resolve mood-last, which is arbitrary but
// stated rather than accidental.
inline void Apply(Preview& p, const Controls& c, const Config& cfg) {
    if (c.direction >= 0 && c.direction < anim::kDirections) {
        p.e.direction = static_cast<int16_t>(c.direction);
        p.e.is_moving = 1;
        p.e.speed     = cfg.run_speed;
    } else {
        p.e.is_moving = 0;
        p.e.speed     = 0;
    }

    if (c.fire_pressed) {
        p.e.player_state = static_cast<uint8_t>(p.next_oneshot == OneShot::Header
                                                    ? PlayerState::StaticHeader
                                                    : PlayerState::Tackling);
        p.next_oneshot   = p.next_oneshot == OneShot::Header ? OneShot::Slide
                                                            : OneShot::Header;
        p.oneshot_timer  = cfg.oneshot_ticks;
        RestartSequence(p.e);
    }

    if (c.mood_pressed) {
        p.e.player_state = static_cast<uint8_t>(
            p.next_mood == Mood::Happy ? PlayerState::Happy : PlayerState::Sad);
        p.next_mood     = p.next_mood == Mood::Happy ? Mood::Sad : Mood::Happy;
        p.oneshot_timer = cfg.oneshot_ticks;
        RestartSequence(p.e);
    }
}

// Advance one tick. Returns the image index to draw (< 0 = nothing).
inline int16_t Tick(Preview& p, Mode mode) {
    if (p.oneshot_timer > 0) {
        --p.oneshot_timer;
        if (p.oneshot_timer == 0) {
            p.e.player_state = static_cast<uint8_t>(PlayerState::Normal);
            RestartSequence(p.e);
        }
    }

    if (mode == Mode::Raw) {
        const int16_t max = static_cast<int16_t>(
            (p.keeper ? kKeeperFrameCount : kPlayerFrameCount) - 1);
        if (p.raw_frame < 0) p.raw_frame = 0;
        if (p.raw_frame > max) p.raw_frame = max;
        p.e.image_index = p.raw_frame;
        return p.e.image_index;
    }

    return StepEntityAnimation(p.e);
}

// Which bank the kit dialog's shirt type selects. Faces are palette-only (C3
// Finding 1), so the face control must never reach this.
inline ShirtGeometry Geometry(const Preview& p) {
    return GeometryForShirtType(p.shirt_type);
}

// Human-readable current state, for the readout. Only the states this tool can
// produce are named; anything else is a bug in Apply().
inline const char* StateName(const Preview& p) {
    switch (static_cast<PlayerState>(p.e.player_state)) {
    case PlayerState::Normal:       return p.e.is_moving ? "running" : "standing";
    case PlayerState::StaticHeader: return "header (static)";
    case PlayerState::Tackling:     return "slide tackle";
    case PlayerState::Happy:        return "happy";
    case PlayerState::Sad:          return "sad";
    default:                        return "?";
    }
}

// Whether the state currently playing has a table behind it. Happy/Sad and the
// headers fall through SelectAnimationTable to standing today, and a viewer that
// silently showed a standing player for "header" would be worse than useless --
// it would read as an art bug. The UI says so instead.
inline bool StateHasTable(const Preview& p) {
    switch (static_cast<PlayerState>(p.e.player_state)) {
    case PlayerState::Normal:
    case PlayerState::Tackling:
        return true;
    default:
        return false;
    }
}

inline const char* DirectionName(int d) {
    static const char* kNames[anim::kDirections] = {"N",  "NE", "E", "SE",
                                                    "S",  "SW", "W", "NW"};
    return (d >= 0 && d < anim::kDirections) ? kNames[d] : "-";
}

} // namespace at::spriteview
