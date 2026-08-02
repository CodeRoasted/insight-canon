// insight.canon.detail.mask — SEALED stateless template-masking domain (1.5.2 domain
// decomposition, ADR-3.D4). The per-line masker: content → (arena-stable masked template +
// params), a PURE function of the line's own tokens. A leaf over the contract: imports api only
// (MaskConfig, ArenaAllocator) — independent of scan/strategy/parse. Never re-exported by the
// facade and never installed (PRIVATE file set).
//
// History: this domain was the stateful Drain online log-template miner (clustering tree +
// absorb_into wildcard learning). That cross-line learning made `template_id` order-dependent — the
// "phantom pair" false-diff (SRC-D-TID-3). The clustering was RIPPED; the
// stateless masker below is the sole identity source.
export module insight.canon.detail.mask;
import insight.canon.internal; // std + global C types
import insight.canon.api;      // MaskConfig, ArenaAllocator
export namespace insight::tokenization
{

// ── Stateless per-line template masker (ADR-16.D5; SRC-D-TID-1, SRC-D-TID-2) ──────
// A deterministic, run-independent function of the line's OWN masked/kept tokens —
// NO cluster state, NO cross-line learning. The same logical line yields the same
// template_str (hence the same SHA-256 template_id, computed downstream) in any run,
// any order, inside any surrounding stream: the phantom pair (a shared line two runs
// template differently) cannot form. The per-token KEEP / MASK / composite-normalize
// classification (status-value KEEP, UUID/long-hash + IPv4/hex + digit-leading MASK,
// source-location / versioned-ref / bracket-index / #-counter / embedded-identity /
// key=<numeric-value> normalization — §8 D-TID-12/SRC-D-TID-13) is DECIDED per token, never discovered from
// cross-line data (SRC-D-TID-2: discover→decide). This is the sole identity source (the clustering tree
// it replaced is RIPPED — SRC-D-TID-3).
//
// ── The composite-normalizer contracts, DECLARED ────────────────────────────────────
// These govern what `stateless_template()` OBSERVABLY returns, so they are interface
// content, not implementation detail: each names a token class and the normal form it
// collapses to. The normalizers themselves are TU-local statics in `mask.cpp`, which
// CITES this block. Where the rule's boundary matters more than its action, the boundary
// is stated too.
//
//
// SRC-D-MSK-1 — DIAGNOSTIC_COMPOSITE. A token is split on `:` and `/` and each SEGMENT is
//   masked independently when it is digit-leading. Collapses the Chromium/glog prefix
//   `[PID:DATE/TIME:LEVEL:file.cc:line]` and SUBSUMES the older source-location rule: one
//   general segment rule replaces a family of shape-specific ones.
// SRC-D-MSK-2 — EPHEMERAL_ROOT, the standalone form. A path under a CATALOGUED root
//   (`/tmp/…`) masks the instance component only: `/tmp/<*>`, never the remainder. The
//   root is the decidable thing; what sits under it is content a bug report needs kept.
// SRC-D-MSK-4 — the ephemeral-root CATALOG plus its matcher, applied at every call site
//   from ONE table. The matcher reads a trailing window of up to `kMaxRootSegments`
//   components (M2); the same catalog serves the in-token path (M3/M4) and the standalone
//   rule above (M5). One catalog, three call sites: a second copy is how two maskers
//   diverge and template identity stops being a pure function of the line.
// SRC-D-MSK-5 — BRACKET_TIMESTAMP. A WHOLE-token bracketed RFC3339 datetime
//   (`[2026-06-23T15:11:09.020Z]`) masks to `[<*>]` instead of falling through to literal
//   KEEP. The bracket is the entire difference: an unbracketed stamp is digit-leading and
//   was already masked, so only this token class moves.
// SRC-D-TID-13b — a class PREFIX inside the bracket survives the mask: `make[<*>]:`,
//   `[gw<*>]`. The prefix is vocabulary and identifies the producer; only the index is
//   high-cardinality, so masking the whole bracket would destroy the distinction the
//   template exists to carry.
// SRC-D-TID-17 — `key=<numeric-value>` masks the VALUE and keeps the KEY. The key is the
//   field's name and is low-cardinality; the value is the instance.
// SRC-D-TID-5 — a line's own tokens are the ONLY input. Equating two spellings of a
//   varying word (a synonym, a reworded message) would require cross-line learning, which
//   is what the ripped clustering did and what made template_id order-dependent. That work
//   belongs to the deferred registry (SRC-D-TID-14), never to this masker: the cost of the
//   miss is a Vanished+New pair, which is honest; the cost of the fix here is a false
//   identity, which is not.
//
// Result is arena-stable until out_arena.reset() (or destruction); `params` are the
// raw tokens at fully-masked (<*>) positions, as views into `content` (which the
// caller must keep stable for the params' lifetime).
struct StatelessTemplate
{
    std::string_view template_str;
    std::span<const std::string_view> params;
};

[[nodiscard]] StatelessTemplate
stateless_template(std::string_view content, ArenaAllocator& out_arena, const MaskConfig& config);

} // namespace insight::tokenization
