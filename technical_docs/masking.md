# Masking — token classification → template identity

Masking turns a line's `content` into a **stable template** (`template_str`) and its identity
(`template_id`). It is what lets the engine say "this is the same *kind* of line as before" while a request id,
a timestamp, or a latency value varies. This doc is the authoritative current-state reference for **what canon
masks, what it keeps, and which markers it knows** (`canonicalization_version = stateless-masks-7`).

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
| 5 | **Digit-leading numeric** | MASK `<*>` — this also carries `0x`-hex: a `0x…` token starts with a digit |
| 6 | **Literal** — none of the above | KEEP literal |

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
| **Ephemeral roots** | `/tmp`, `/var/tmp`, `/var/folders`, `.conan2/p/b`, `/nix/store` — each carrying a declared **anchor** + **scope** (§3.1) | §4 ephemeral-root — a path segment directly under a per-run build/temp root is an instance and masks; the root is kept. |
| **Min hash length** | `16` | Rule 3 / §4 embedded-identity — a hex-only run this long is an instance hash, not a word. |
| **Wildcard** | `<*>` | The mask placeholder. |

`mask_ip_addresses` is the one `MaskConfig` knob (default **on**) gating a rule — rule 4. It gates for a
reason the retired hex knob never did: its grammar admits a leading `[`, and a bracketed token is not
digit-leading, so `[10.20.30.40]` masks with the knob on and stays **literal** with it off.

### 3.1 The ephemeral-root catalog — the root is the decidable thing

**No hex/length rule can separate an ephemeral token from content:** the same 40-character SHA is a *pinned
dependency* in one path and *per-run junk* in another — same length, same alphabet, opposite class. What **is**
decidable is the **root** — an enumerable, byte-exact catalog of build/temp directories whose immediate child
is a per-run instance *by construction* (a conan build dir, a nix store hash, a randomized `/tmp` dir). Each
entry declares two axes; both are **explicit, never inferred** — a mis-declared root over-masks, and
over-masking destroys signal irrecoverably, so the dangerous choices are named on purpose:

- **anchor** — `TokenStart`: the root's first component is the token's first path component (a leading `/…`).
  `Floating`: the root matches at **any** component boundary, so a mid-path root stays visible.
- **scope** — `Subtree`: everything under the root is ephemeral, so the whole remainder collapses to `<*>` (a
  namespace of ephemeral *trees*). `Instance`: exactly the one component under the root masks and the tail
  resumes normal classification (a content-addressed *store* whose subtree is structurally stable).

| Root | anchor | scope |
|---|---|---|
| `/tmp` · `/var/tmp` · `/var/folders` | `TokenStart` | `Subtree` |
| `.conan2/p/b` | `Floating` | `Instance` |
| `/nix/store` | `TokenStart` | `Instance` |

The catalog is a **single source of truth** consulted from **two** call sites: the standalone ephemeral-root
normalizer (§4) **and**, as a per-segment predicate, from inside the source-location normalizer's segment walk
— so an instance directory inside a compiler diagnostic masks even though it is letter-leading, while the
`file:line` tail survives (§4). `bazel-out` is deliberately **excluded**: its component is the build
*configuration* (`k8-fastbuild`, `ppc-opt`), which is stable per config and carries drift signal we want
surfaced — it holds no hash, so masking it would destroy signal to fix nothing. Adding a root is a **core**
masking change (it is syntactic, not ecosystem vocabulary) and bumps `canonicalization_version`.

---

## 4. The composite normalizers (keep-class, mask-instance)

A composite token carries a structural delimiter; each normalizer keeps the stable part and masks the varying
instance. Tried in order; first match wins.

| Normalizer | Matches | Keeps / masks | Example |
|---|---|---|---|
| **source-location** | `<path-like>:<digits>[:<digits>]` (prefix contains `.` or `/`) | keep the file/path, mask each `:<digit-run>` | `tokenizer.cpp:4500:30:` → `tokenizer.cpp:<*>:<*>:` |
| **ephemeral-root** | a path (`/`) whose segments match a declared ephemeral root (§3.1), reached only when no earlier rule claimed the token | keep the root, mask the instance component; **Subtree** collapses the whole remainder, **Instance** keeps the tail | `/tmp/pw-electron-userdata-Kw9v4a` → `/tmp/<*>` (Subtree) · `~/.conan2/p/b/insig247…/lib/x.so` → `~/.conan2/p/b/<*>/lib/x.so` (Instance) |
| **versioned-ref** | `<name>/<numeric-version>` (digit after the last `/`, only punctuation may trail) | keep the name, mask the version | `zlib/1.3` → `zlib/<*>` |
| **bracket-index** | `<word>[<short-alpha?><digits>]` | keep the word + class marker, mask the index | `make[2]:` → `make[<*>]:` · `[gw0]` → `[gw<*>]` |
| **hash-counter** | `#<digits>` (no alnum may trail) | keep `#`, mask the index | `step #26` → `step #<*>` |
| **marker-number** | `<currency-marker><digit-core>[.<digits>]` | keep the marker, mask the number | `$463.50` → `$<*>` |
| **embedded-identity** | a UUID (`8-4-4-4-12`) or a hex run ≥ 16, *inside* a larger token not under a declared ephemeral root | mask the id in place, keep surrounding structure | `~/.cache/gradle/f7f6…2680/lib.jar` → `~/.cache/gradle/<*>/lib.jar` |
| **kv-value** | `<key>=<digit-leading-value>` (strips a leading currency marker first) | keep the key (+ marker), mask the value | `order=100000` → `order=<*>` · `total=$18` → `total=$<*>` |

**The ephemeral-root catalog is also consulted from inside source-location.** A per-run instance directory in
a compiler diagnostic masks even though it is letter-leading (it would otherwise be *kept* as a class anchor),
while the `file:line` tail — what makes a diagnostic actionable — survives:
`/home/runner/.conan2/p/b/insig247e3d1dffc33/…/span_unpack.cpp:72:5:` →
`/home/runner/.conan2/p/b/<*>/…/span_unpack.cpp:<*>:<*>:`. Two runs whose only difference is that per-run
directory (`insig247…` vs `insigea56…`) now collapse to **one** template instead of manufacturing a phantom
new/vanished pair every run — the defect this catalog exists to kill. The scope is clamped to `Instance` here
regardless of what the entry declares, so the location tail is never masked.

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
