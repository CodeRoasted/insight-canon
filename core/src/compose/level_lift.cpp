module;

module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api; // LogLevel / LogFormat
import insight.canon.spi; // LevelLiftRow

// level_lift.cpp — the LEVEL-LIFT algorithm over the composed vocabulary (ADR 0063 clause 2).
// Canon owns the algorithm; the composed `LevelLiftRow` set (from the semantic packages) is the
// DATA — the same split `semantic_walkers.cpp` applies to classify / recognize /
// recognize_location.
//
// Ported byte-for-byte from `insight::semantic::github`'s package-local `level_from_message`
// (github_strategy.cpp), which walked that package's own `kLevelLifts` array inside `parse()`. Two
// things changed and nothing else: the rows come from `ComposedSemantics` instead of one package's
// static array, and the walk consults each row's `format_gate` — which the package walk did not
// need, because the only way to reach it was through the GHA strategy, so the gate was satisfied
// structurally. Both preserve the observable result exactly: the composed table holds the same rows
// in the same declared order, and a row gated to GitHubActions can only fire on a GHA-routed line
// either way.
//
// Homed in its own impl unit rather than folded into compose.cpp: composition (sort, concatenate,
// fail closed, hash) and recognition (walk a table for one line) are different responsibilities,
// and only the second is on the per-line path.

namespace insight::tokenization
{

LogLevel lift_level(std::string_view content, LogFormat format,
                    const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::LevelLiftRow& row : composed.level_lifts())
        if (insight::semantic::detail::gate_matches(row.format_gate, format) &&
            content.starts_with(row.prefix))
            return row.level;
    return LogLevel::Unknown;
}

} // namespace insight::tokenization
