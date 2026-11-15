# Determinism — the canon contract

Deterministic, reproducible output is a **core product constraint**, not an optimization. canon's classifying
and canonicalizing of a line must be **bit-identical** for the same ordered input across compilers, standard
libraries, and operating systems. Everything downstream (windowing, drift, detection) inherits its determinism
from canon's; if canon is not bit-identical, nothing above it can be.

---

## 1. What "deterministic" means here

For the same ordered stream of input lines, every output-affecting field — `template_str`, `template_id`,
`level`, `component`, `host`, `params`, `structural_role`, the ordinal integers — is **byte-for-byte identical**
on every target. `format` is per-line observability and excluded from the content contract (but is itself
deterministic). The proven targets are the GCC / Clang / MSVC toolchains across their standard libraries and
operating systems; the determinism corpus under `proof/` is the standing oracle, re-derived and re-verified on
every release.

---

## 2. The rules that make it hold

Every classification and masking rule in this library obeys the same constraints:

- **No floating point in any content path.** A `float → int` conversion can diverge across machines (x87 / SSE
  / FMA), so it is forbidden anywhere that feeds deterministic content. Numbers that must be compared are kept
  as integers / fixed-point: severity bands map by integer range; ordinal values are parsed **decimal-text →
  int64** scaled by a power of ten, never via `double`.
- **Byte-exact, single-pass, order-independent.** Masking classifies one token from its own bytes; the failure
  lexicon is byte-compare + ASCII case-fold; escape stripping is a byte state machine. No rule depends on a
  neighbouring line, on accumulated state, or on the order tokens were inserted into any container.
- **No unordered-map iteration order in output.** Any place that would iterate a hash container to build output
  is structurally avoided — iteration order is not portable.
- **Frozen, declared catalogs.** Every catalog (status keywords, currency markers, the failure lexicon and its
  roles, the pass/fail glyphs, the OTEL and ordinal field maps, the structural-role markers) is a **closed,
  declared** set in source. None is data-learned at runtime — a data-learned wildcard or class would, by
  construction, differ across runs and break reproducibility. New entries are added deliberately, with a
  version bump when they change identity (§3).
- **Pure helpers.** The token scanners, the glyph matchers, the anchor tests — all `noexcept`, allocation-free
  on the hot path, and free of locale/wall-clock dependence.

---

## 3. The `canonicalization_version` gate

A MetaLog document's `canonicalization_version` (the specification's §2.4 token) is the contract that
makes templates **comparable across runs**. It is built in one place,
`ComposedSemantics::canonicalization_version()` in `core/api/canon.compose.cppm`, as
`<masking generation>:<composed identity>`:

- the **masking generation** is `kCanonicalizationVersion` in `core/api/canon.api.cppm` (the current value
  lives there; every value it has held, and why, is in
  [canonicalization_generations.md](canonicalization_generations.md)). It is also hashed into the composed
  identity;
- the **composed identity** (`semantic_identity`, rendered by `identity_hex()`) is a content hash over the
  rows the composed vocabulary packages and canon's transport catalogue ship, so a package change moves
  it without a hand-bumped label.

The token is opaque: a consumer compares only documents carrying an equal token, and never parses it. A
producer that injects no composition (the metalog library's default) carries the bare masking generation.

- **When the masking generation bumps:** on a change to what canon serializes in a field a consumer must
  handle — the masking rules and template identity, and every classification rule whose output is
  serialized. The ledger names each bump's class and its witness.
- **When only the composed identity moves:** a vocabulary package's rows change. The token still moves, so
  documents from the two compositions are incomparable, while the masking generation stays.
- **The discipline:** mismatched tokens are **re-derived, never migrated** (a template under old rules is not
  the same identity as under new rules). A masking change cascades the full golden diagonal (re-derive +
  re-prove cross-toolchain).

---

## 4. How it is verified

- **The `proof/` determinism corpus** is the golden oracle: representative inputs whose canonical output is
  frozen and asserted bit-identical across the toolchain diagonal on every release. A change that moves a
  golden must re-derive it intentionally; an *unexpected* move is a determinism regression and blocks release.
- **Cross-toolchain by execution.** The goldens are re-proven on each compiler/stdlib/OS, not assumed — the
  same digest must come out of every target. (Inputs that exercise the parser's slow paths, e.g. escaped JSON,
  are kept in the corpus so the slow path is proven, not just the fast one.)
- **Single-threaded by contract.** `Tokenizer` / `LogParser` / `ArenaAllocator` are not thread-safe; parallel
  throughput is achieved with one instance per worker, never shared mutable state — so there is no interleaving
  to make non-deterministic.

---

*See also: [masking.md](masking.md) (the rules this protects) · [classification.md](classification.md) (the
non-identity signals) · [formats.md](formats.md) (decimal-text ordinal parsing, the no-float path).*
