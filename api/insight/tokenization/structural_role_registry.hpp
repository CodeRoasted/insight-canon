#pragma once

#include <string_view>

#include "insight/core/types.hpp"

namespace insight::tokenization
{

// StructuralRoleRegistry — classifies a LINE's role in the sequence (what the line
// DOES), as opposed to the SemanticClass of a token inside it (what a value MEANS).
// Two orthogonal ontologies kept in two separate registries — that separation is
// the F12 countermeasure to value-vs-line-role conflation (Salience epic §4.2).
//
// Roles here are ANNOUNCED only: the line declares itself with a marker
// (`##[group]`, `##[error]`). Positional roles ("is on the dominant path") are
// DERIVED by the structural layer from the sequence graph — those are layer
// outputs, NOT registry entries; keeping that line bright is what keeps "registry"
// honest. The catalog is a seed; extend `classify` as scenarios surface markers.
//
// Bridge direction is one-way: the structural layer MAY read semantic annotations
// (a kept `EXIT_CODE != 0` is a strong Terminator candidate) — never the reverse.
// This seed recognizes the announced GitHub-Actions/Azure markers; the exit-code
// bridge and level-based refinements are deliberate later additions.
class StructuralRoleRegistry
{
  public:
    [[nodiscard]] static StructuralRole classify(std::string_view content) noexcept
    {
        if (content.starts_with("##[group]") || content.starts_with("::group::"))
            return StructuralRole::GroupBegin;
        if (content.starts_with("##[endgroup]") || content.starts_with("::endgroup::"))
            return StructuralRole::GroupEnd;
        if (content.starts_with("##[error]") || content.starts_with("::error::"))
            return StructuralRole::Terminator;
        return StructuralRole::None;
    }
};

} // namespace insight::tokenization
