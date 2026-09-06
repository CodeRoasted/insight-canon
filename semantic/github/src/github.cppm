// refs: ADR-17, ADR-17.D1, DN-17.D12
// invariant: the ruleset is `github.dialect.yaml`; this unit is its module purview and its
// code-tier seam, and declares no row of its own.
// refs: DN-17.D19
// invariant: every row, the manifest, both vocabularies and the compile-time fences arrive from
// `github.generated.inc`, built per compile and never committed.
// refs: DN-17.D16
// note: an `.inc` in a wrapper: a generated module unit must be scanned before it exists
module;

export module insight.semantic.github;
import insight.canon.internal;
import insight.canon.api;
export import insight.canon.spi;

namespace insight::semantic::github
{

// refs: DN-17.D16
// assert: declaring the hook HERE is what makes a declaration naming a missing symbol a compile
// error at the projection's `&is_echoed_source`, never a link error.
// note: why the code tier is only this hook: `dialect.code_tier.echoed_source.why` in the YAML
export bool is_echoed_source(std::string_view raw_line) noexcept;

} // namespace insight::semantic::github

#include "github.generated.inc"
