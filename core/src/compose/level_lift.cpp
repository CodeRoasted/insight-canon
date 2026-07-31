module;

module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api; // LogLevel / LogFormat
import insight.canon.spi; // LevelLiftRow

// level_lift.cpp — the LEVEL-LIFT algorithm over the composed vocabulary (ADR-22).
// Canon owns the algorithm; the composed `LevelLiftRow` set (from the semantic packages) is the
// DATA — the same split `semantic_walkers.cpp` applies to classify / recognize /
// recognize_location.
//
// Ported byte-for-byte from `insight::semantic::github`'s package-local `level_from_message`
// (github_strategy.cpp), which walked that package's own `kLevelLifts` array inside `parse()`. One
// thing changed and nothing else: the rows come from `ComposedSemantics` instead of one package's
// static array. The observable result is preserved exactly — the resolved view holds the same rows
// in the same declared order, and a row gated to the `github` dialect is only in the view of a
// stream that declared it, which is the structural equivalent of the package walk being reachable
// only through the GHA strategy.
//
// NO GATE PARAMETER (ADR-22). The dialect is evaluated once, at `resolve_stream`, and
// filtered into this view. The intermediate shape — a `format` argument fed from
// `LogParser::routed_format()` — was a live determinism hazard: the routed format is the per-line
// detector winner under a sticky-strategy fast path, so which DECLARED rows fired was a function of
// content.
//
// Homed in its own impl unit rather than folded into compose.cpp: composition (sort, concatenate,
// fail closed, hash) and recognition (walk a table for one line) are different responsibilities,
// and only the second is on the per-line path.

namespace insight::tokenization
{

LogLevel lift_level(std::string_view content,
                    const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::LevelLiftRow& row : composed.level_lifts())
        if (content.starts_with(row.prefix))
            return row.level;
    return LogLevel::Unknown;
}

} // namespace insight::tokenization
