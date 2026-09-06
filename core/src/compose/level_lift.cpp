module;

module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

namespace insight::tokenization
{

// pre: `composed` is a view already resolved for the stream; no dialect gate is tested here.
// note: this walk is on the per-line path; composition is not.
// refs: ADR-22.D6
LogLevel lift_level(std::string_view content,
                    const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::LevelLiftRow& row : composed.level_lifts())
        if (content.starts_with(row.prefix))
            return row.level;
    return LogLevel::Unknown;
}

} // namespace insight::tokenization
