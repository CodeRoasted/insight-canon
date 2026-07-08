// insight.canon.test — shared white-box test infrastructure (§11.9.11, the logcraft.test pattern).
// All test TUs import this instead of spelling out the full import block.
// Re-exports the complete canon module surface (public facade + the sealed detail shards), so a
// test TU needs no further imports beyond gtest (textual, third-party).
export module insight.canon.test;
export import std;
export import insight.canon;
export import insight.canon.detail.scan;
export import insight.canon.detail.strategy;
export import insight.canon.detail.mask;
export import insight.canon.detail.parse;
// The provider contract (ADR 0024 §2.4) — core tests construct SYNTHETIC manifests / rows to exercise the
// composition + recognition ALGORITHMS vocabulary-free (the facade does not surface spi; a white-box core
// test legitimately does). Package suites import spi via their own package module instead.
export import insight.canon.spi;

// Shared core-test composition helper (ADR 0024). A core test whose property is SEMANTIC-UNAWARE
// (the universal formats tokenize; no dialect rows fire) feeds the Tokenizer a degenerate, zero-package
// composition — core tests never link the semantic packages (that would invert the dependency arrow).
// The returned ComposedSemantics MUST outlive the Tokenizer it feeds: bind it to a named local/member
// declared BEFORE the Tokenizer, never pass the temporary inline (the Tokenizer holds a const-ref).
export namespace insight::test_support
{
[[nodiscard]] inline insight::semantic::ComposedSemantics degenerate_composition()
{
    return insight::semantic::compose({});
}
} // namespace insight::test_support
