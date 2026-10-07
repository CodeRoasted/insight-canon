// refs: ADR-17, ADR-17.D1, ADR-17.D14
// invariant: the ruleset is `package_managers.dialect.yaml`; this unit is its module purview and
// declares no row of its own, and the package has no code tier.
// refs: DN-17.D19
// invariant: every row, the manifest, the revision vocabulary and the compile-time fences arrive
// from `package_managers.generated.inc`, built per compile and never committed.
// refs: DN-17.D16
// note: an `.inc` in a wrapper: a generated module unit must be scanned before it exists
module;

export module insight.semantic.package_managers;
import insight.canon.internal;
import insight.canon.api;
export import insight.canon.spi;

#include "package_managers.generated.inc"
