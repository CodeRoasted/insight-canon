module;

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;

// invariant: the last-resort catch-all for unstructured text.
// invariant: zero-copy — the input is already arena-stable when parse is invoked, so the body is
// a subview of it after a pointer-arithmetic left trim.
// invariant: the masker does the templating downstream, with its per-token numeric, address and hex
// rules.
namespace insight::tokenization
{

std::expected<ParsedLine, std::string> RawTextStrategy::parse(std::string_view line,
                                                              ArenaAllocator& /*arena*/) const
{
    // invariant: leading ASCII whitespace is trimmed so indented continuation lines group with
    // their peers; pure pointer arithmetic, no copy.
    std::size_t start{0};
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
        ++start;

    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = EventTime::parsed(std::nullopt);
    parsed.component = {};
    parsed.content = line.substr(start);
    // invariant: even unstructured lines usually LEAD with a level or a marker, so the level is
    // recovered here and the dominant-level signal survives into the document.
    parsed.level = utils::infer_leading_log_level(parsed.content);
    return parsed;
}

LogFormat RawTextStrategy::format() const noexcept
{
    return LogFormat::RawText;
}

double RawTextStrategy::confidence(std::string_view /*line*/) const noexcept
{
    // invariant: the confidence MUST stay a constant zero, and two distinct behaviours depend on
    // it.
    // invariant: this catch-all never wins the structured majority vote, and it never arms the
    // parser's sticky fast path.
    // invariant: any positive value would let the fast path latch this strategy and greedily
    // template every FOLLOWING line as raw text, starving the structured strategies.
    // invariant: so it is reached only through the detector's explicit fallback, and only for a
    // non-empty line, never by scoring.
    return 0.0;
}

} // namespace insight::tokenization
