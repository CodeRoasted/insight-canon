#!/usr/bin/env python3
"""Localize a det_proof digest divergence to a single (file, section) block.

The det_public_proof digest is a single stream hashed to one sha256, so a whole-digest mismatch
(e.g. the MSVC cross-OS probe vs the Linux golden) says only "they differ", not WHERE. This splits
both texts on the digest's own headers — `## file <name>` and `### <section>` (templates / events /
det_math) — hashes each block, and prints a per-block MATCH/DIVERGE table plus a unified diff of the
first diverging block. That turns "the digest differs" into "the EVENTS section of ci_build.log
differs", which points straight at the cause (Daidalos's "per-stage hashes localize it instantly").

Usage:  localize_digest.py <candidate-digest.txt> <reference-digest.txt>
Exit:   0 if every block matches, 1 if any diverges (so it can gate a script if ever wanted; the CI
        measure step ignores the code — it stays non-gating per the determinism-model timebox).
"""
import sys
import hashlib
import difflib


def _basename(path: str) -> str:
    # Match det_proof's basename_of (strip both separators) so the section KEY is the basename, not a
    # platform-specific input path — otherwise Linux `ci_build.log` vs Windows `D:\...\ci_build.log`
    # would key as different files and mask that the payloads are identical. The `## file` line itself
    # is still compared (it's inside the (file-header) block), so a real header divergence still shows.
    sep = max(path.rfind("/"), path.rfind("\\"))
    return path[sep + 1:] if sep >= 0 else path


def sections(path: str) -> dict:
    cur_file, cur_sec, out = "(prologue)", "(header)", {}
    with open(path, encoding="utf-8") as handle:
        for line in handle.read().split("\n"):
            if line.startswith("## file "):
                cur_file, cur_sec = _basename(line[len("## file "):].strip()), "(file-header)"
            elif line.startswith("### "):
                cur_sec = line[4:].split(" ")[0].strip()
            out.setdefault((cur_file, cur_sec), []).append(line)
    return out


def block_hash(body) -> str:
    return hashlib.sha256("\n".join(body).encode()).hexdigest()[:12] if body is not None else "-missing-"


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: localize_digest.py <candidate.txt> <reference.txt>", file=sys.stderr)
        return 2
    cand, ref = sections(sys.argv[1]), sections(sys.argv[2])
    keys = list(dict.fromkeys(list(ref) + list(cand)))  # reference order, candidate-only appended
    first_div = None
    print(f"  {'file':22s} {'section':12s} verdict")
    for key in keys:
        ref_h, cand_h = block_hash(ref.get(key)), block_hash(cand.get(key))
        ok = ref_h == cand_h
        print(f"  {key[0]:22s} {key[1]:12s} {'MATCH' if ok else 'DIVERGE'}  ref={ref_h} cand={cand_h}")
        if not ok and first_div is None:
            first_div = key
    if first_div is None:
        print("\n  all blocks match — divergence is below section granularity (whitespace/encoding?)")
        return 0
    print(f"\n::warning::first diverging stage = {first_div[0]} / {first_div[1]} — hand THIS to Daidalos")
    print("  --- first 40 unified-diff lines (ref < / cand >) ---")
    diff = difflib.unified_diff(ref.get(first_div, []), cand.get(first_div, []),
                                "reference", "candidate", lineterm="")
    for line in list(diff)[:40]:
        print("  " + line)
    return 1


if __name__ == "__main__":
    sys.exit(main())
