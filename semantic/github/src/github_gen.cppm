// insight.semantic.github_gen — the GitHub Actions dialect semantic package as PROJECTED from
// `github.dialect.yaml`. This file is the thin wrapper; every row, the manifest, the channel and
// revision vocabularies, the prose and the compile-time fences come from `github.generated.inc`,
// which `dialect_package_codegen.py` writes into the build tree on every build and which is never
// committed (ADR-17 / DN-17.D12).
//
// THE WRAPPER IS HAND-WRITTEN AND THE PROJECTION IS AN `.inc`, never a generated `.cppm`
// (DN-17.D19): a generated module interface unit must be SCANNED before it exists, which is
// unmeasured on both toolchains and sits in the named-modules failure family this workspace has
// catalogued. A textual include into an existing module purview attaches the projected content to
// this module and needs no scan of a file that is not there yet.
//
// THE DECLARED DIALECT NAME IS "github", VERBATIM — only this module, its namespace and its target
// carry `_gen`. The name is semantic content: every row's `dialect_gate` carries it into
// `semantic_identity`, so renaming at the declared level would move the digest and make a
// projection that recognizes exactly what the hand-written package recognizes look different from
// it. The suffix is a GENERATION parameter (`--module-suffix _gen`) that the tool's own
// `--selftest` fences to the module name and the namespace.
//
// `insight.semantic.github` and this module are never COMPOSED together: each is composed on its
// own (DN-17.D9). Composition flattens, so composing both would publish one dialect name twice.
//
// Self-contained exactly as the hand-written package is (ADR-17.D1): canon's public API and its
// provider contract, never a sealed detail shard. `export import insight.canon.spi` so a consumer
// can name `SemanticPackageManifest` — the type of `kManifest` — without a separate spi import.
module;

export module insight.semantic.github_gen;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // StructuralRole, LogLevel, LogFormat, IntentMarkerKind, ChildOrder
export import insight.canon.spi;

namespace insight::semantic::github_gen
{

// ── The code-tier seam ──
// The echoed-source raw-line provenance hook (matches spi::ProvenanceHook — pure
// bool(string_view) noexcept), declared here so the projected `kManifest` can take its address.
// The projection's own pin `static_assert`s that this signature is the SPI's.
//
// WHERE IT IS DEFINED: `src/github_gen_provenance.cpp`, this module's implementation unit. The
// included projection names `src/github_provenance.cpp` instead, and that is not a defect: `unit:`
// is declaration content describing the package the declaration IS, and this module's namespace is
// a generation-time re-anchoring of that same package that the declaration cannot see.
export bool is_echoed_source(std::string_view raw_line) noexcept;

} // namespace insight::semantic::github_gen

// The projection. Written by `dialect_package_codegen.py` into the build tree's `generated/`
// directory, which is on this target's private include path.
#include "github.generated.inc"
