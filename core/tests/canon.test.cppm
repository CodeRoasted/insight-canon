// invariant: shared white-box test infrastructure — ONE test module per domain, mirroring the
// package's own module layout.
// invariant: every test unit imports THIS instead of spelling out the full import block.
// invariant: it re-exports the complete module surface, the public facade plus the sealed detail
// shards, so a test unit needs no further import beyond the harness.
export module insight.canon.test;
export import std;
export import insight.canon;
export import insight.canon.detail.scan;
export import insight.canon.detail.strategy;
export import insight.canon.detail.mask;
export import insight.canon.detail.parse;
// invariant: the PROVIDER CONTRACT between core and the vocabulary packages — core tests build
// SYNTHETIC manifests and rows to exercise the algorithms VOCABULARY-FREE.
// invariant: the facade does not surface the provider interface, and a white-box core test
// legitimately does; package suites import it through their own package module instead.
export import insight.canon.spi;
// invariant: the conformance kit is a PUBLIC module unit rather than a sealed shard, because canon
// ships it INSTALLED.
// invariant: a core test exercising the kit's own algorithms still belongs to the same
// import-this-and-nothing-else contract as the rest of the surface.
export import insight.canon.conformance;

// invariant: a core test whose property is SEMANTIC-UNAWARE feeds the tokenizer a DEGENERATE,
// zero-package composition.
// invariant: core tests never link the semantic packages, which would invert the dependency arrow.
// invariant: the returned composition MUST OUTLIVE the tokenizer it feeds.
// invariant: bind it to a named local declared BEFORE the tokenizer and never pass the temporary
// inline, because the tokenizer holds a const-ref.
export namespace insight::test_support
{
[[nodiscard]] inline insight::semantic::ComposedSemantics degenerate_composition()
{
    return insight::semantic::compose({});
}
} // namespace insight::test_support
