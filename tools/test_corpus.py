#!/usr/bin/env python3
"""Parse every committed corpus input log; verify hash chains against ATTR files.

When a reference checkout is present and AT_REFERENCE is set, optionally warn that
real regeneration should be preferred over the stub oracle (A3 §5).
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "tests" / "corpus"


def hash_chain(attr: bytes) -> int:
    if len(attr) < 24:
        return 0
    magic, _ver, stride, _seed, count = struct.unpack_from("<IHHII", attr, 0)
    if magic != 0x52545441:
        return 0
    need = 24 + count * stride
    if len(attr) < need:
        return 0
    chain = 1469598103934665603
    for i in range(count):
        off = 24 + i * stride
        (rh,) = struct.unpack_from("<Q", attr, off + stride - 8)
        chain ^= rh
        chain = (chain * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return chain


def parse_atin(data: bytes) -> tuple[int, int]:
    magic, ver, _prof, _ft, seed, count = struct.unpack_from("<IHBBII", data, 0)
    if magic != 0x4E495441 or ver != 1:
        raise ValueError("bad ATIN")
    if len(data) < 16 + count * 4:
        raise ValueError("truncated ATIN")
    return seed, count


def main() -> int:
    if not CORPUS.is_dir():
        print("test_corpus.py: tests/corpus missing", file=sys.stderr)
        return 1
    errors = 0
    for scenario in sorted(p for p in CORPUS.iterdir() if p.is_dir()):
        atin = scenario / "input.atin"
        if not atin.is_file():
            print(f"{scenario.name}: missing input.atin")
            errors += 1
            continue
        seed, count = parse_atin(atin.read_bytes())
        print(f"{scenario.name}: atin ok (seed=0x{seed:08x}, ticks={count})")
        for kind in ("engine", "reference"):
            attr = scenario / f"{kind}.attr"
            chain_path = scenario / f"{kind}.chain"
            if not attr.is_file() or not chain_path.is_file():
                print(f"{scenario.name}: missing {kind}.attr/chain")
                errors += 1
                continue
            got = hash_chain(attr.read_bytes())
            want = int(chain_path.read_text(encoding="utf-8").strip(), 16)
            if got != want:
                print(f"{scenario.name}: {kind} chain mismatch {got:016x} != {want:016x}")
                errors += 1
            else:
                print(f"{scenario.name}: {kind}.chain ok")
    if errors:
        print(f"FAILED ({errors})", file=sys.stderr)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
