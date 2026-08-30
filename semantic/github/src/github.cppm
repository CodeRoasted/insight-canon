// insight.semantic.github — the GitHub Actions / Azure Pipelines dialect semantic package
// (ADR-17), PROJECTED from `github.dialect.yaml`. This file is the whole hand-written half: every
// row, the manifest, the channel and revision vocabularies, the prose and the compile-time fences
// come from `github.generated.inc`, which `dialect_package_codegen.py` writes into the build tree
// on every build and which is never committed (ADR-17 / DN-17.D12). The declaration IS the
// ruleset; this unit is its module purview and its code-tier seam.
//
// THE WRAPPER IS HAND-WRITTEN AND THE PROJECTION IS AN `.inc`, never a generated `.cppm`
// (DN-17.D19): a generated module interface unit must be SCANNED before it exists, which is
// unmeasured on both toolchains and sits in the named-modules failure family this workspace has
// catalogued. A textual include into an existing module purview attaches the projected content to
// this module and needs no scan of a file that is not there yet.
//
// WHERE THE PROJECTION IS FOUND, in the two configurations this package ships in, because the
// answer differs and both are load-bearing:
//   * BUILD TREE — `github.generated.inc` is written to `<build>/generated/` and that directory is
//     on this target's include path. CMake carries it to a consumer that recompiles this interface
//     unit through `IMPORTED_CXX_MODULES_INCLUDE_DIRECTORIES`.
//   * INSTALLED PACKAGE — the projection is installed NEXT TO this file, in the same module
//     directory, so the quoted include below resolves by the includer's own directory before any
//     search path is consulted. Cross-package module shipping installs the interface unit for the
//     CONSUMER to recompile (§10.7/§10.9), and a consumer's compiler has no build tree of ours to
//     look in.
//
// Self-contained exactly as a semantic package must be (ADR-17.D1): canon's public API and its
// provider contract, never a sealed detail shard. `export import insight.canon.spi` so a consumer
// can name `SemanticPackageManifest` — the type of `kManifest` — without a separate spi import.
module;

export module insight.semantic.github;
import insight.canon.internal;
import insight.canon.api;
export import insight.canon.spi;

namespace insight::semantic::github
{

// ── The code-tier seam ──
// The echoed-source raw-line provenance hook (matches spi::ProvenanceHook — pure
// bool(string_view) noexcept), declared here so the projected `kManifest` can take its address.
// The projection's own pin `static_assert`s that this signature is the SPI's.
//
// WHERE IT IS DEFINED: `src/github_provenance.cpp`, this module's implementation unit — the name
// the declaration's `unit:` carries, and the projection reproduces it verbatim.
//
// WHY THE CODE TIER IS ONLY THIS HOOK is argued at `dialect.code_tier.echoed_source.why` in
// `github.dialect.yaml` and emitted by the projection above the signature pin. It is not restated
// here: both texts land in this one translation unit, so a second copy is a second home.
export bool is_echoed_source(std::string_view raw_line) noexcept;

} // namespace insight::semantic::github

// The projection.
#include "github.generated.inc"
