module;

module insight.canon.transport;
import insight.canon.internal;
import insight.canon.api;

// transport.cpp — the transform ALGORITHMS and the fail-closed declaration resolution (ADR 0044
// §4/§6). Canon owns every algorithm; the catalogue owns only rows. A new transform kind is a
// catalogue-version bump with its algorithm landing here, never a package-local peel.

namespace insight::transport
{

namespace
{

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < 10U;
    }
    [[nodiscard]] constexpr bool is_space(char chr) noexcept
    {
        return chr == ' ' || chr == '\t';
    }

    // The `LinePrefixTimestamp` declared byte grammar: an RFC 3339 stamp `YYYY-MM-DDTHH:MM:SS…`
    // occupying exactly `width` bytes at line head.
    //
    // Shape-checked rather than width-trusted, and that is the DECLARED RULE, not a detection:
    // "remove these bytes IF they are a stamp of this shape" is one total rule whose effect is
    // nothing on a line that does not carry one (ADR 0044 §2). Trusting the width blindly would
    // corrupt every non-conforming line instead of leaving it alone, which is a worse reading of
    // the same declaration — and it would make a single unstamped line silently shift the whole
    // template.
    [[nodiscard]] constexpr bool has_stamp_at_head(std::string_view line,
                                                   std::size_t width) noexcept
    {
        // "YYYY-MM-DDTHH:MM:SS" — the invariant head of every RFC 3339 form; the sub-second tail
        // and the zone designator are inside the declared width and are not re-validated here.
        constexpr std::size_t kMinShapeLen{19U};
        if (line.size() < width || width < kMinShapeLen)
            return false;
        return is_digit(line[0]) && is_digit(line[1]) && is_digit(line[2]) && is_digit(line[3]) &&
               line[4] == '-' && is_digit(line[5]) && is_digit(line[6]) && line[7] == '-' &&
               is_digit(line[8]) && is_digit(line[9]) && line[10] == 'T' && is_digit(line[11]) &&
               is_digit(line[12]) && line[13] == ':' && is_digit(line[14]) && is_digit(line[15]) &&
               line[16] == ':' && is_digit(line[17]) && is_digit(line[18]);
    }

    // Apply ONE row. Returns the shortened view; sets `observation_time` when the row declares that
    // extract AND the stamp actually parses.
    void apply_row(const TransportTransformRow& row, PeeledLine& peeled) noexcept
    {
        switch (row.kind)
        {
        case TransportTransformKind::LinePrefixTimestamp:
        {
            const std::size_t width{row.prefix_width};
            if (!has_stamp_at_head(peeled.content, width))
                return; // the rule's effect on these bytes is nothing (§2)

            if (row.extract == TransportExtract::EventObservationTime)
            {
                // Enrichment only. See TransportExtract::EventObservationTime: never an ordering
                // key, never asserted monotone, never a replay input.
                peeled.observation_time =
                    insight::utils::parse_iso8601(peeled.content.substr(0U, width));
            }
            peeled.content.remove_prefix(width);
            if (row.strip_leading_space)
                while (!peeled.content.empty() && is_space(peeled.content.front()))
                    peeled.content.remove_prefix(1U);
            break;
        }
        }
    }

    [[noreturn]] void fail_unknown_transform(std::string_view name)
    {
        std::cerr << "FATAL: insight::transport::resolve_transport_stack — unknown transport "
                     "transform \""
                  << name << "\". The catalogue (" << kTransportCatalogVersion << ") declares: ";
        for (std::size_t i{0}; i < kTransportCatalogRows.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << kTransportCatalogRows[i].name << '"';
        std::cerr << ".\nA transport stack is caller-declared provenance (ADR 0044 §6), never "
                     "guessed: canon VERIFIES, it does not infer. An unknown transform is a "
                     "MISTAKE and fails closed here; an ABSENT stack is a CHOICE and peels "
                     "nothing. Declare one of the names above, or none.\n";
        std::terminate();
    }

} // namespace

PeeledLine TransportStack::peel(std::string_view line) const noexcept
{
    PeeledLine peeled{.content = line, .observation_time = std::nullopt};
    // Outside-in, in declaration order: the outermost delivery layer was applied last on the way
    // out, so it comes off first on the way in.
    for (const TransportTransformRow* row : rows_)
        apply_row(*row, peeled);
    return peeled;
}

TransportStack resolve_transport_stack(const IngestDeclaration& declaration)
{
    std::vector<const TransportTransformRow*> resolved;
    resolved.reserve(declaration.stack.size());
    for (const std::string_view name : declaration.stack)
    {
        const TransportTransformRow* row{find_transform(name)};
        if (row == nullptr)
            fail_unknown_transform(name);
        resolved.push_back(row);
    }
    return TransportStack{std::move(resolved)};
}

} // namespace insight::transport
