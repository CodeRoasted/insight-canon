// insight.canon.detail.mask — SEALED stateless template-masking domain (1.5.2 domain
// decomposition, §11.9.11). The per-line masker: content → (arena-stable masked template +
// params), a PURE function of the line's own tokens. A leaf over the contract: imports api only
// (MaskConfig, ArenaAllocator) — independent of scan/strategy/parse. Never re-exported by the
// facade and never installed (PRIVATE file set).
//
// History: this domain was the stateful Drain online log-template miner (clustering tree +
// absorb_into wildcard learning). That cross-line learning made `template_id` order-dependent — the
// "phantom pair" false-diff (stateless_template_id.md SRC-D-TID-3). The clustering was RIPPED; the
// stateless masker below is the sole identity source.
export module insight.canon.detail.mask;
import insight.canon.internal; // std + global C types
import insight.canon.api;      // MaskConfig, ArenaAllocator
export namespace insight::tokenization
{

// ── Stateless per-line template masker (stateless_template_id.md D-TID-1/2) ──────
// A deterministic, run-independent function of the line's OWN masked/kept tokens —
// NO cluster state, NO cross-line learning. The same logical line yields the same
// template_str (hence the same SHA-256 template_id, computed downstream) in any run,
// any order, inside any surrounding stream: the phantom pair (a shared line two runs
// template differently) cannot form. The per-token KEEP / MASK / composite-normalize
// classification (status-value KEEP, UUID/long-hash + IPv4/hex + digit-leading MASK,
// source-location / versioned-ref / bracket-index / #-counter / embedded-identity /
// key=<numeric-value> normalization — §8 D-TID-12/13) is DECIDED per token, never discovered from
// cross-line data (D-TID-2: discover→decide). This is the sole identity source (the clustering tree
// it replaced is RIPPED — SRC-D-TID-3).
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
