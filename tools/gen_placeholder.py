#!/usr/bin/env python3
"""Generate committed placeholder ATAP packs — A4 §2.5.

Dimensionally identical to the imported contract: 101 player frames at 12×15,
16×16 pitch tiles, 42×55 index matrix. Coloured rectangles only; no original art.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "placeholder"

MAGIC = 0x50415441  # ATAP
VERSION = 1
KIND_SPRITES = 0
KIND_PITCH = 1
SOURCE_PLACEHOLDER = 0
HEADER_SIZE = 48
ENTRY_SIZE = 16

PLAYER_FRAMES = 101
PLAYER_W, PLAYER_H = 12, 15
BALL_W, BALL_H = 8, 8
TILE = 16
GRID_W, GRID_H = 42, 55
TILE_COUNT = 4


def fnv(data: bytes, seed: int = 1469598103934665603) -> int:
    h = seed
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def put_u16(v: int) -> bytes:
    return struct.pack("<H", v & 0xFFFF)


def put_u32(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)


def put_u64(v: int) -> bytes:
    return struct.pack("<Q", v & 0xFFFFFFFFFFFFFFFF)


def build_pack(
    kind: int,
    entries: list[tuple[int, int, int, int, bytes]],
    source: int,
    fingerprint: int,
    aux: bytes = b"",
    aux_w: int = 0,
    aux_h: int = 0,
) -> bytes:
    table_end = HEADER_SIZE + ENTRY_SIZE * len(entries)
    blob = b"".join(px for *_, px in entries)
    blob_off = table_end
    aux_off = blob_off + len(blob) if aux else 0

    hdr = b"".join(
        [
            put_u32(MAGIC),
            put_u16(VERSION),
            put_u16(kind),
            put_u32(len(entries)),
            put_u32(HEADER_SIZE),
            put_u32(blob_off),
            put_u32(len(blob)),
            put_u32(aux_off),
            put_u32(len(aux)),
            put_u16(aux_w),
            put_u16(aux_h),
            bytes([source, 0, 0, 0]),
            put_u64(fingerprint),
        ]
    )
    assert len(hdr) == HEADER_SIZE

    table = bytearray()
    off = 0
    for w, h, ax, ay, px in entries:
        table += put_u16(w)
        table += put_u16(h)
        table += put_u16(ax & 0xFFFF)
        table += put_u16(ay & 0xFFFF)
        table += put_u32(off)
        table += put_u32(len(px))
        off += len(px)

    return bytes(hdr) + bytes(table) + blob + aux


def solid_frame(w: int, h: int, index: int, ax: int, ay: int) -> tuple:
    px = bytes([index & 0xFF] * (w * h))
    # Leave a 1-pixel transparent border (index 0) so canvases are not flat noise.
    if w > 2 and h > 2:
        row = bytearray(px)
        for y in range(h):
            for x in range(w):
                if x == 0 or y == 0 or x == w - 1 or y == h - 1:
                    row[y * w + x] = 0
        px = bytes(row)
    return (w, h, ax, ay, px)


def write_player(path: Path, fill: int, fingerprint_tag: bytes) -> int:
    entries = []
    for i in range(PLAYER_FRAMES):
        # Anchor near centre of the 12×15 canvas; slight per-frame nudge for indexing.
        ax = 6
        ay = 7 + (i % 3)
        entries.append(solid_frame(PLAYER_W, PLAYER_H, fill + (i % 5), ax, ay))
    fp = fnv(fingerprint_tag)
    for *_, px in entries:
        fp = fnv(px, fp)
    data = build_pack(KIND_SPRITES, entries, SOURCE_PLACEHOLDER, fp)
    path.write_bytes(data)
    return fp


def write_ball(path: Path) -> int:
    entries = [solid_frame(BALL_W, BALL_H, 15, 4, 4)]
    fp = fnv(b"ball")
    fp = fnv(entries[0][4], fp)
    path.write_bytes(build_pack(KIND_SPRITES, entries, SOURCE_PLACEHOLDER, fp))
    return fp


def write_pitch(path: Path) -> int:
    entries = []
    for t in range(TILE_COUNT):
        entries.append(solid_frame(TILE, TILE, 2 + t, 0, 0))
    # Row-major u16 tile indices, cycling through the four tiles.
    aux = bytearray()
    for row in range(GRID_H):
        for col in range(GRID_W):
            aux += put_u16((row + col) % TILE_COUNT)
    fp = fnv(b"pitch1")
    for *_, px in entries:
        fp = fnv(px, fp)
    fp = fnv(bytes(aux), fp)
    path.write_bytes(
        build_pack(KIND_PITCH, entries, SOURCE_PLACEHOLDER, fp, bytes(aux), GRID_W, GRID_H)
    )
    return fp


def write_manifest(path: Path, packs: list[tuple[str, int]], combined_fp: int) -> None:
    # ATM1 header + pack_count + name[32]+fp per pack
    body = bytearray()
    body += put_u32(0x314D5441)  # ATM1
    body += put_u16(1)
    body += bytes([SOURCE_PLACEHOLDER, 0])
    body += put_u64(combined_fp)
    body += put_u32(len(packs))
    for name, fp in packs:
        raw = name.encode("ascii")[:32]
        body += raw + bytes(32 - len(raw))
        body += put_u64(fp)
    path.write_bytes(bytes(body))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    packs: list[tuple[str, int]] = []
    packs.append(("slotA_blk0.atp", write_player(args.out / "slotA_blk0.atp", 10, b"slotA")))
    packs.append(("slotB_blk0.atp", write_player(args.out / "slotB_blk0.atp", 40, b"slotB")))
    packs.append(("ball.atp", write_ball(args.out / "ball.atp")))
    packs.append(("pitch1.atp", write_pitch(args.out / "pitch1.atp")))

    combined = 1469598103934665603
    for name, fp in packs:
        combined = fnv(name.encode() + put_u64(fp), combined)

    write_manifest(args.out / "manifest.atm", packs, combined)
    (args.out / "README.md").write_text(
        "# Placeholder art\n\n"
        "Generated by `tools/gen_placeholder.py`. Dimensionally identical to imported "
        "packs (101×12×15 players, 16×16 tiles, 42×55 matrix). Not original artwork.\n",
        encoding="utf-8",
    )
    print(f"wrote {len(packs)} packs + manifest to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
