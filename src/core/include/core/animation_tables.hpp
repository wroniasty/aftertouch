#pragma once
#include <cstdint>
#include <span>

// C3 — the frame lists.
//
// These index the 101-frame player bank (and the 58-frame keeper bank) that `assetc`
// imports. We do not have the original's animation tables, so every list here was
// MEASURED off the art rather than transcribed, and the method matters as much as the
// numbers because it is what makes them checkable:
//
//  * FACING came from the head. Count skin pixels (palette 4/5/6) against hair
//    (9/12/13) in the head band: a player facing away shows no face at all, one facing
//    the camera shows a full one, and the horizontal offset between the skin and hair
//    centroids says which way he is turned. That splits frames 0..29 into ten groups of
//    three with a monotone face-visibility ordering.
//  * MIRROR PAIRING confirmed it. Comparing every frame against every other frame's
//    horizontal mirror pairs the groups off — 6..8 with 9..11, 12..14 with 15..17,
//    18..20 with 21..23 — which is exactly the six non-axis directions. The four groups
//    that are their own mirror (0..2, 3..5, 24..26, 27..29) are therefore the two axis
//    directions, and the reason there are four rather than two is that mirroring a
//    front-on runner does not change his direction, it changes which leg leads. So N
//    and S get six-frame cycles and everyone else gets three.
//  * THE NEUTRAL POSE came from the feet. Within each triple exactly one frame has the
//    socks touching (spread 1) and the others are mid-stride (3..4). That frame is the
//    standing pose for its direction.
//  * THE PRONE SETS came from the body axis: the vector from the sock centroid to the
//    head centroid, quantised to an octant.
//
// What is NOT measured is marked `[PROVISIONAL]` and falls back to standing rather
// than guessing, because a wrong frame table looks exactly like a rendering bug.
//
// Direction is the octant convention shared by the whole codebase: 0 = N, clockwise.

namespace at::anim {

inline constexpr int kDirections = 8;

// Frame-list opcodes (PLAYER_SPRITES.md §7). Values >= 0 are frames to show.
inline constexpr int16_t kHold = -101;   // freeze on the current frame (one-shots)
inline constexpr int16_t kLoop = -999;   // restart the list (cycles)
// -1 .. -99 set the frame delay; -100 and <= -102 jump by (value + 100).

using FrameList = std::span<const int16_t>;

// ---------------------------------------------------------------------------
// Outfield
// ---------------------------------------------------------------------------

// Running. N and S carry the six-frame cycle; the diagonals and sides carry three.
inline constexpr int16_t kRunN[]  = {0, 1, 2, 24, 25, 26, kLoop};
inline constexpr int16_t kRunNE[] = {21, 22, 23, kLoop};
inline constexpr int16_t kRunE[]  = {6, 7, 8, kLoop};
inline constexpr int16_t kRunSE[] = {15, 16, 17, kLoop};
inline constexpr int16_t kRunS[]  = {3, 4, 5, 27, 28, 29, kLoop};
inline constexpr int16_t kRunSW[] = {12, 13, 14, kLoop};
inline constexpr int16_t kRunW[]  = {9, 10, 11, kLoop};
inline constexpr int16_t kRunNW[] = {18, 19, 20, kLoop};

// Standing: the legs-together frame of that direction's cycle.
inline constexpr int16_t kStandN[]  = {1, kHold};
inline constexpr int16_t kStandNE[] = {21, kHold};
inline constexpr int16_t kStandE[]  = {6, kHold};
inline constexpr int16_t kStandSE[] = {17, kHold};
inline constexpr int16_t kStandS[]  = {4, kHold};
inline constexpr int16_t kStandSW[] = {14, kHold};
inline constexpr int16_t kStandW[]  = {9, kHold};
inline constexpr int16_t kStandNW[] = {18, kHold};

// Sliding tackle: one prone frame per direction, held for the slide's duration.
// 54..63 is a directional prone set; SE and SW are absent from it and borrow the E and
// W bodies, which reads correctly at this size. [PROVISIONAL: the 64..73 set is a
// second prone family — knocked-down, most likely — and which family the game used for
// which state is not yet measured.]
inline constexpr int16_t kSlideN[]  = {62, kHold};
inline constexpr int16_t kSlideNE[] = {56, kHold};
inline constexpr int16_t kSlideE[]  = {60, kHold};
inline constexpr int16_t kSlideSE[] = {60, kHold};
inline constexpr int16_t kSlideS[]  = {63, kHold};
inline constexpr int16_t kSlideSW[] = {61, kHold};
inline constexpr int16_t kSlideW[]  = {61, kHold};
inline constexpr int16_t kSlideNW[] = {57, kHold};

// Knocked down / on the ground. Two frames, direction-independent: 54 lies head-south,
// 55 head-north, so the choice tracks which way he was facing when he went over.
inline constexpr int16_t kDownNorthward[] = {55, kHold};
inline constexpr int16_t kDownSouthward[] = {54, kHold};

// ---------------------------------------------------------------------------
// Keeper
// ---------------------------------------------------------------------------
//
// The keeper bank's frames 0..23 are structurally identical to the outfield bank's --
// same widths, same face-visibility pattern, same foot spreads -- so the same reading
// applies. It has no 24..29, because 24+ are the dives, so N and S run on three frames.
inline constexpr int16_t kKeeperRunN[] = {0, 1, 2, kLoop};
inline constexpr int16_t kKeeperRunS[] = {3, 4, 5, kLoop};

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------

struct DirTable {
    const int16_t* lists[kDirections];
    uint8_t        sizes[kDirections];
};

#define AT_LIST(x) x, static_cast<uint8_t>(sizeof(x) / sizeof(int16_t))

inline constexpr DirTable kRunning = {
    {kRunN, kRunNE, kRunE, kRunSE, kRunS, kRunSW, kRunW, kRunNW},
    {6 + 1, 3 + 1, 3 + 1, 3 + 1, 6 + 1, 3 + 1, 3 + 1, 3 + 1}};

inline constexpr DirTable kStanding = {
    {kStandN, kStandNE, kStandE, kStandSE, kStandS, kStandSW, kStandW, kStandNW},
    {2, 2, 2, 2, 2, 2, 2, 2}};

inline constexpr DirTable kSliding = {
    {kSlideN, kSlideNE, kSlideE, kSlideSE, kSlideS, kSlideSW, kSlideW, kSlideNW},
    {2, 2, 2, 2, 2, 2, 2, 2}};

inline constexpr DirTable kKeeperRunning = {
    {kKeeperRunN, kRunNE, kRunE, kRunSE, kKeeperRunS, kRunSW, kRunW, kRunNW},
    {3 + 1, 3 + 1, 3 + 1, 3 + 1, 3 + 1, 3 + 1, 3 + 1, 3 + 1}};

#undef AT_LIST

inline FrameList At(const DirTable& t, int direction) {
    const int d = (direction < 0 || direction >= kDirections) ? 0 : direction;
    return FrameList(t.lists[d], t.sizes[d]);
}

} // namespace at::anim
