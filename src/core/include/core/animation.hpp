#pragma once
#include "core/animation_tables.hpp"
#include "core/match_state.hpp"

// C3 §3.3 — the frame stepper.
//
// This lives in `at_core`, inside the hashed simulation, and that is a deliberate
// choice rather than an accident of where the fields happen to sit. PLAYER_SPRITES.md
// §12: `frameSwitchCounter` gates gameplay, not just visuals -- header contact keys off
// it -- so an animation that ran in the renderer would put a gameplay input outside the
// deterministic tick and outside the replay. `Entity` has carried the fields since B1;
// this is what finally writes them.
//
// The frame list is a tiny interpreted sequence, not code (PLAYER_SPRITES.md §7):
//
//   >= 0        show this frame
//   -1 .. -99   set the frame delay to -value for what follows
//   -100, <=-102 relative jump: frame_index += value + 100
//   -101        hold on the current frame (one-shots)
//   -999        loop back to the start (cycles)
//
// Opcodes are consumed until one produces an image, so a list can change speed and emit
// a frame in the same tick.

namespace at {

// Which animation an entity should be playing, from its own state. Kept separate from
// the stepper so a caller can ask without advancing anything.
inline const anim::DirTable& SelectAnimationTable(const Entity& e) {
    const bool keeper = e.player_ordinal == 1;
    switch (static_cast<PlayerState>(e.player_state)) {
    case PlayerState::Tackling:
        return anim::kSliding;
    case PlayerState::Tackled:
    case PlayerState::Down:
    case PlayerState::Injured:
        return anim::kSliding;   // prone; the frame is chosen by facing below
    default:
        break;
    }
    // Idle fallback, as in updateAnimationTableAndDestinationReached: a Normal player
    // who is not moving stands rather than running on the spot.
    const bool moving = e.is_moving != 0 || e.speed != 0;
    if (!moving) return anim::kStanding;
    return keeper ? anim::kKeeperRunning : anim::kRunning;
}

// Advance one tick. Writes frame_index / cycle_frames_timer / frame_switch_counter /
// image_index. Returns the image index actually shown (< 0 = unchanged this tick).
inline int16_t StepEntityAnimation(Entity& e) {
    if (e.frame_delay <= 0) e.frame_delay = 5;   // Sprite.h init default

    const anim::DirTable& table = SelectAnimationTable(e);
    const anim::FrameList list  = anim::At(table, e.direction);
    if (list.empty()) return e.image_index;

    // A table change restarts the sequence; without this a one-shot that ended on a
    // hold would keep its stale cursor and freeze the next animation on entry.
    if (e.frame_index < 0 || e.frame_index >= static_cast<int16_t>(list.size()))
        e.frame_index = -1;

    if (e.cycle_frames_timer > 0) {
        --e.cycle_frames_timer;
        if (e.cycle_frames_timer > 0) return e.image_index;
    }

    // Bounded: a malformed list that never emits an image must not spin forever.
    for (int guard = 0; guard < 64; ++guard) {
        ++e.frame_index;
        if (e.frame_index >= static_cast<int16_t>(list.size())) {
            e.frame_index = 0;
        }
        const int16_t v = list[static_cast<size_t>(e.frame_index)];
        if (v >= 0) {
            e.image_index = v;
            ++e.frame_switch_counter;
            e.cycle_frames_timer = e.frame_delay;
            return e.image_index;
        }
        if (v == anim::kLoop) {
            e.frame_index = -1;
            continue;
        }
        if (v == anim::kHold) {
            --e.frame_index;
            e.cycle_frames_timer = e.frame_delay;
            return e.image_index;
        }
        if (v <= -100) {
            e.frame_index = static_cast<int16_t>(e.frame_index + (v + 100));
            if (e.frame_index < -1) e.frame_index = -1;
            continue;
        }
        e.frame_delay = static_cast<int16_t>(-v);
        e.cycle_frames_timer = e.frame_delay;
    }
    return e.image_index;
}

// The ball spins by distance travelled, so a still ball holds its frame and a driven
// one blurs. Four rotation frames; the delay falls as speed rises.
inline void StepBallAnimation(Entity& ball) {
    const int speed = ball.speed < 0 ? -ball.speed : ball.speed;
    if (speed == 0) return;
    const int delay = speed >= 24 ? 1 : (speed >= 12 ? 2 : (speed >= 5 ? 3 : 5));
    if (ball.cycle_frames_timer > 0) {
        --ball.cycle_frames_timer;
        return;
    }
    ball.cycle_frames_timer = static_cast<int16_t>(delay);
    ball.frame_index = static_cast<int16_t>((ball.frame_index + 1) & 3);
    ball.image_index = ball.frame_index;
    ++ball.frame_switch_counter;
}

// Every player plus the ball, once per tick.
inline void StepAnimations(MatchState& s) {
    for (int i = 0; i < kPitchPlayers; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        if (IsOffPitch(e)) continue;
        StepEntityAnimation(e);
    }
    StepBallAnimation(s.ball);
}

} // namespace at
