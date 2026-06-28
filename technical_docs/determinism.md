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

`canonicalization_version` (currently **`stateless-masks-3`**) is the contract that makes templates
**comparable across runs**. It identifies the exact set of masking/identity rules that produced a template.

- **When it bumps:** any change to the **masking** rules or template identity — a new mask class, a changed
  marker catalog, a different normalizer — bumps the version. A consumer compares only documents sharing a
  version; mismatched versions are **re-derived, never migrated** (a template under old rules is not the same
  identity as under new rules).
- **When it does *not* bump:** changes to **classification** (level / failure-cue / structural role) do **not**
  bump it — they do not change template identity. They may still move the determinism goldens (a line's level
  flips), which is verified by re-deriving the affected golden, but they are a different change class from a
  masking bump.
- **The discipline:** a masking change that bumps the version cascades the full golden diagonal (re-derive +
  re-prove cross-toolchain). The version string is the single source of truth for "are these two templates
  even talking about the same rule set?"

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
