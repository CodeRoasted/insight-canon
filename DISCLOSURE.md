# DISCLOSURE — sixteen historic release refs of this repo publish third-party corpus bytes, by a recorded ruling

**These are real operational logs, in historic refs only.** Sixteen historic tags of this
repository carry a 17-file `corpora/loghub/slice/` tree of real production system logs from a
published academic corpus, with the identifying content such logs carry. `HEAD` has been clean
since 2026-07-09; tag `v1.9.6` onward carries nothing. If you are asking *"is this known?"* —
yes, it is known, it is bounded, and the boundary is a checkable predicate.

**Ruling:** Founder, Emmanuel Prunet, 2026-08-22

**Ground:** removal refused, fix-forward. Tag deletion removes no bytes and drafts fifteen
published Releases; a history rewrite breaks every peeled tag and the reproducibility of every
past cut. Ruled 2026-08-18, signed 2026-08-21 for the hub instance
(`coderoast-hub/samples/loghub/samples/DISCLOSURE.md`); this record extends the same signed
acceptance to this repository's historic refs.

## The exact set

The 15 tags `v1.5.4` … `v1.7.5` each carrying the 17-file `corpora/loghub/slice/` tree (16 `*_2k.log` files + attribution). No other ref of this repository carries any of it.

## The upstream publication of record

| | |
|---|---|
| **Publisher** | LogHub — S. He, J. Zhu, P. He, M. R. Lyu (ISSRE 2023) |
| **Stable identifier** | Zenodo record `8196385` — https://doi.org/10.5281/zenodo.8196385 |
| **Licence** | Creative Commons Attribution 4.0 International (CC-BY-4.0) |
| **Byte-identity** | The `*_2k.log` files in those refs are **verbatim** members of that record — blob-identical to the disclosed hub instance, measured 2026-08-22. |

Byte-identity is the load-bearing condition: our copy adds nothing to an exposure the corpus
authors deliberately chose when they published it. A reader who does not trust us can fetch the
Zenodo record and compare.

## The mechanical boundary

**No ref newer than `v1.7.5` may carry the slice.** This is a checkable predicate over the tag
set, enforced at publication time; the accepted exposure is the closed set above and can never
silently grow.

## Correction — 2026-10-01: these files are logpai/loghub's, not CC-BY, and LogHub is withdrawn

Signed by the Founder, Emmanuel Prunet, on 2026-10-01.

The 16 `*_2k.log` files of the `corpora/loghub/slice/` tree, in this repository's `main` from
2026-06-17 (`3aed9e1`) to 2026-07-09 (`c2fea10`) and in its tags `v1.5.4` to `v1.7.5`, are
logpai/loghub's own sample files (its commit `dd61d0952749ee7963bde24220d1be5ede023033`). They are
not members of Zenodo record `8196385`, and they are not CC-BY-4.0: they are under logpai's notice
(free for research or academic work, on condition that any use or distribution refers to
https://github.com/logpai/loghub, cites the loghub paper and includes the notice), which they did
not carry. In 15 of the 16, the carriage return of every CRLF line end was stripped when they were
committed. The licence and byte-identity rows above are wrong on both counts.

CodeRoast now publishes only its own logs, and LogHub has been withdrawn from every public surface.
These refs are not rewritten; this note is their disclosure.
