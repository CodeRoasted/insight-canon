# Masking — token classification → template identity

Masking turns a line's `content` into a **stable template** (`template_str`) and its identity
(`template_id`). It is what lets the engine say "this is the same *kind* of line as before" while a request id,
a timestamp, or a latency value varies. This doc is the authoritative current-state reference for **what canon
masks, what it keeps, and which markers it knows** (`canonicalization_version = stateless-masks-3`).

---

## 1. The model — stateless, per-line, keep-class / mask-instance

- **Stateless & per-line.** A template is a pure function of **one line's own tokens** — no cross-line
  learning, no clustering memory. The same logical line yields the same template in any run, any order, on any
  machine. (This is why identity is reproducible and why a "phantom pair" from learned wildcards cannot occur.)
- **Keep-class / mask-instance.** The unifying idea behind every rule: keep the **stable class marker**, mask
  the **varying instance**. `#42 → #<*>` (keep the counter marker, mask the number); `file.cc:408 →
  file.cc:<*>` (keep the source file, mask the line); `order=123 → order=<*>` (keep the key, mask the value).
- **The discriminator is `digit-leading`.** A token (or sub-part) that begins with a digit is a
  number/measurement/version/timestamp — intrinsically high-cardinality — and masks; a token that begins with
  a letter is a word/keyword and is kept. One rule subsumes numbers-with-separators, decimals, number+unit, and
  versions, with **no unit lexicon**: `512MB`/`6.2s`/`76.5%` mask (digit-leading); `sha256`/`utf8`/`x86` keep
  (letter-leading).

The wildcard placeholder is **`<*>`**. Tokens are split on **whitespace only** for masking (the structural
tokenizer used by classification is separate — see [classification.md](classification.md)). Kept/normalized
tokens contribute their text to `template_str`; **fully-masked** tokens contribute a `<*>` *and* push their raw
value into `params` (a normalized composite that embeds `<*>` contributes **no** param — it is a kept class,
not a masked instance).

---

## 2. The total precedence order

Each whitespace token is classified by the **first** matching rule:

| # | Rule | Disposition |
|---|---|---|
| 1 | **Status-value KEEP** — an all-digit token, ≤ 3 digits, immediately after a **status keyword** | KEEP literal |
| 2 | **Composite** — the token carries a structural delimiter; one of the normalizers (§4) matches | KEEP normalized (embeds `<*>`) |
| 3 | **UUID / long hash** | MASK `<*>` |
| 4 | **IPv4** (when `mask_ip_addresses`) | MASK `<*>` |
| 5 | **`0x`-hex** (when `mask_hex_addresses`) | MASK `<*>` |
| 6 | **Digit-leading numeric** | MASK `<*>` |
| 7 | **Literal** — none of the above | KEEP literal |

Rule 1 wins first on purpose: it protects the **green→red distinction** that downstream diffing depends on —
`exit code 0` and `exit code 1` must stay *different* templates, so a short status value after a status keyword
is never masked (see §3). The composite layer (rule 2) is gated by a cheap pre-check: a token is only tried
against the normalizers if it contains one of `: / [ # - =` or a declared marker prefix; everything else skips
straight to the fixed masks.

---

## 3. The declared catalogs (the "which markers")

Every catalog below is **frozen and declared** — a closed set, extended only on measured evidence, never
data-learned. This is what keeps masking decidable and deterministic.

| Catalog | Contents | Purpose |
|---|---|---|
| **Status keywords** | `code`, `status`, `exit`, `signal` (case-insensitive) | Rule 1 — the keyword before a short numeric value that must stay split (status codes, exit codes). |
| **Max status digits** | `3` | Rule 1 — a status value masks if longer (it's an id, not a code). |
| **Currency markers** | `$` (ASCII; structured to add `€`/`£`/`¥` as declared byte sequences if a corpus shows them) | §4 marker-number — a leading currency symbol glued to a number. |
| **Min hash length** | `16` | Rule 3 — a hex-only run this long is an instance hash, not a word. |
| **Wildcard** | `<*>` | The mask placeholder. |

`mask_ip_addresses` and `mask_hex_addresses` are the two `MaskConfig` knobs (both default **on**) gating rules
4 and 5.

---

## 4. The composite normalizers (keep-class, mask-instance)

A composite token carries a structural delimiter; each normalizer keeps the stable part and masks the varying
instance. Tried in order; first match wins.

| Normalizer | Matches | Keeps / masks | Example |
|---|---|---|---|
| **source-location** | `<path-like>:<digits>[:<digits>]` (prefix contains `.` or `/`) | keep the file/path, mask each `:<digit-run>` | `tokenizer.cpp:4500:30:` → `tokenizer.cpp:<*>:<*>:` |
| **versioned-ref** | `<name>/<numeric-version>` (digit after the last `/`, only punctuation may trail) | keep the name, mask the version | `zlib/1.3` → `zlib/<*>` |
| **bracket-index** | `<word>[<short-alpha?><digits>]` | keep the word + class marker, mask the index | `make[2]:` → `make[<*>]:` · `[gw0]` → `[gw<*>]` |
| **hash-counter** | `#<digits>` (no alnum may trail) | keep `#`, mask the index | `step #26` → `step #<*>` |
| **marker-number** | `<currency-marker><digit-core>[.<digits>]` | keep the marker, mask the number | `$463.50` → `$<*>` |
| **embedded-identity** | a UUID (`8-4-4-4-12`) or a hex run ≥ 16, *inside* a larger token | mask the id in place, keep surrounding structure | `/var/tmp/f7f6…2680/cache.tzst` → `/var/tmp/<*>/cache.tzst` |
| **kv-value** | `<key>=<digit-leading-value>` (strips a leading currency marker first) | keep the key (+ marker), mask the value | `order=100000` → `order=<*>` · `total=$18` → `total=$<*>` |

The kv-value normalizer carries the **same status carve-out** as rule 1: `status=200` / `code=0` stay literal
(a status keyword + short numeric value), so the green→red flip survives in `key=value` form too. It masks
**numeric** values only — `user=alice` (letter-leading) stays literal, because masking *all* values would
collapse `status=ok` and `status=failed` (telling an instance key from a categorical key needs cardinality,
which a stateless per-line masker cannot see — see §6).

---

## 5. Template identity

`template_id` is the **first 16 bytes of SHA-256(`template_str`)** — a 128-bit content address of the masked
line. It renders on the wire as `h:` + 32 lowercase hex digits (the only place the id materializes as text).
Same masked sequence → same id, byte-for-byte, on every machine.

Two lines collapse to one template **iff** their masked token sequences are byte-identical. That is the entire
contract: masking decides identity, identity decides "same kind of line," and everything downstream
(frequency, novelty, drift) rides on it.

---

## 6. The boundary — what canon deliberately does NOT mask

Masking is **syntactic and decidable by construction**. It masks classes it can recognize from a single line's
bytes; it stops where recognition would require cross-line knowledge. This boundary is a design guarantee, not
a gap to be closed with more rules:

- **Arbitrary varying words.** `User alice` / `User bob` — letter-leading, not a number/hash. canon keeps
  them. Knowing `alice` and `bob` are "the same field varying" needs cardinality across lines.
- **Word-prefixed ids.** `ORD-123`, `pod-x7f`, `order=ORD-123` — syntactically indistinguishable from a
  versioned keyword (`arm64`, `gpt-4`, `utf-8`): same `<alpha><sep?><suffix>` shape. Only *cardinality*
  separates an id from a keyword, and a stateless masker cannot see cardinality. Masking these by a syntactic
  rule would either over-mask real keywords (`arm64 → arm<*>`) or require an ad-hoc prefix allow/deny list.
- **Categorical numbers that should split.** An HTTP `404` vs `500` is handled by **extending the status-KEEP
  context** (rule 1 / the kv carve-out), never by weakening the digit-leading mask.

The decidability test for adding a rule: *is there a low-cardinality keyword of this exact shape worth
protecting?* If no (e.g. `$<digits>` — there is no `$`-prefixed keyword), the class is decidably a number and
earns a rule. If yes (e.g. `<word>-<digits>`), it is undecidable per line and stays out — the home for that is
a future data-informed, cardinality-aware, **frozen** classifier (the deferred `SemanticClassRegistry`), not
more syntactic rules. A standing cardinality monitor watches for any format where over-splitting erodes
compression past the healthy band — that signals masking is too weak *for that format*, surfaced as a measured
warning rather than guessed at.

---

*See also: [classification.md](classification.md) (the failure/level signals computed on the same line) ·
[determinism.md](determinism.md) (why every rule here is byte-exact and what the `canonicalization_version`
gate protects) · [formats.md](formats.md) (ordinals are carried beside the template, never masked into it).*
