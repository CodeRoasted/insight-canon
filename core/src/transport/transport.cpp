module;

module insight.canon.transport;
import insight.canon.internal;
import insight.canon.api;

// transport.cpp — the transform ALGORITHMS and the fail-closed declaration resolution (ADR-23
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
    // nothing on a line that does not carry one (ADR-23). Trusting the width blindly would
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

    // Apply ONE row over a plain view. Returns the shortened view; sets `observation_time` when
    // the row declares that extract AND the stamp actually parses. The ONE algorithm both public
    // doors share — `peel` and `peel_raw` differ only in what their parameter PROVES and their
    // return type STATES, never in bytes (a byte divergence between the two would be the
    // two-implementations defect this whole contract is about).
    // Shared by both prefix kinds: the post-stamp separator/indentation strip (greedy `[ \t]+`,
    // leading only). For the bracketed kind this reproduces the deleted JenkinsStrategy's bundled
    // behavior #3 BYTE-EXACTLY (adr/0046 verdict (a); the merit question stays parked in flaws.md).
    void strip_separator(const TransportTransformRow& row, RawPeeledLine& peeled) noexcept
    {
        if (!row.strip_leading_space)
            return;
        while (!peeled.content.empty() && is_space(peeled.content.front()))
            peeled.content.remove_prefix(1U);
    }

    void apply_row(const TransportTransformRow& row, RawPeeledLine& peeled) noexcept
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
            strip_separator(row, peeled);
            break;
        }
        case TransportTransformKind::LinePrefixBracketedTimestamp:
        {
            // The deleted JenkinsStrategy's `timestamper_prefix_end` position logic, verbatim in
            // effect (G-T5-PEEL scores the equivalence against the frozen oracle spelling): `[` at
            // byte 0, the SHARED full-datetime grammar starting at byte 1 (one owner —
            // rfc3339_datetime_length; the strictness carve-outs fail it by construction), `]`
            // immediately after, nothing in between. Variable width; `prefix_width` unread.
            if (peeled.content.empty() || peeled.content.front() != '[')
                return; // the rule's effect on these bytes is nothing (§2)
            const std::size_t datetime_len{
                insight::utils::rfc3339_datetime_length(peeled.content, 1U)};
            if (datetime_len == 0U)
                return;
            const std::size_t close{1U + datetime_len};
            if (close >= peeled.content.size() || peeled.content[close] != ']')
                return;

            if (row.extract == TransportExtract::EventObservationTime)
            {
                // The bracket interior — same enrichment-only contract as above.
                peeled.observation_time =
                    insight::utils::parse_iso8601(peeled.content.substr(1U, datetime_len));
            }
            peeled.content.remove_prefix(close + 1U);
            strip_separator(row, peeled);
            break;
        }
        }
    }

    // Proleptic-Gregorian calendar decomposition of a day count since 1970-01-01 (Howard
    // Hinnant's `civil_from_days`, the standard integer-only algorithm — the era/cycle constants
    // below are the algorithm's own and carry its citation rather than local names: 146097 days
    // per 400-year era, 719468 days from 0000-03-01 to 1970-01-01, 153-day 5-month cycles).
    // Pure integer — no <ctime>, no locale, no wall-clock (determinism MUST M8).
    struct CivilDate
    {
        std::int64_t year;
        unsigned month;
        unsigned day;
    };

    [[nodiscard]] constexpr CivilDate civil_from_days(std::int64_t days_since_epoch) noexcept
    {
        const std::int64_t shifted{days_since_epoch + 719468};
        const std::int64_t era{(shifted >= 0 ? shifted : shifted - 146096) / 146097};
        const auto day_of_era{static_cast<unsigned>(shifted - (era * 146097))};
        const unsigned year_of_era{
            ((day_of_era - (day_of_era / 1460)) + (day_of_era / 36524) - (day_of_era / 146096)) /
            365};
        const std::int64_t year{static_cast<std::int64_t>(year_of_era) + (era * 400)};
        const unsigned day_of_year{
            day_of_era - ((365 * year_of_era) + (year_of_era / 4) - (year_of_era / 100))};
        const unsigned shifted_month{((5 * day_of_year) + 2) / 153};
        const unsigned day{(day_of_year - (((153 * shifted_month) + 2) / 5)) + 1};
        const unsigned month{shifted_month < 10 ? shifted_month + 3 : shifted_month - 9};
        return {.year = year + (month <= 2 ? 1 : 0), .month = month, .day = day};
    }

    // Two zero-padded decimal digits into `out` at `pos` (pos advances). Caller guarantees range.
    constexpr void put_two_digits(std::span<char> out, std::size_t& pos, unsigned value) noexcept
    {
        out[pos] = static_cast<char>('0' + (value / 10U));
        out[pos + 1U] = static_cast<char>('0' + (value % 10U));
        pos += 2U;
    }

    [[noreturn]] void fail_unknown_transform(std::string_view name)
    {
        std::cerr << "FATAL: insight::transport::resolve_transport_stack — unknown transport "
                     "transform \""
                  << name << "\". The catalogue (" << kTransportCatalogVersion << ") declares: ";
        for (std::size_t i{0}; i < kTransportCatalogRows.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << kTransportCatalogRows[i].name << '"';
        std::cerr << ".\nA transport stack is caller-declared provenance (ADR-23), never "
                     "guessed: canon VERIFIES, it does not infer. An unknown transform is a "
                     "MISTAKE and fails closed here; an ABSENT stack is a CHOICE and peels "
                     "nothing. Declare one of the names above, or none.\n";
        std::terminate();
    }

} // namespace

RawPeeledLine TransportStack::peel_raw(std::string_view line) const noexcept
{
    RawPeeledLine peeled{.content = line, .observation_time = std::nullopt};
    // Outside-in, in declaration order: the outermost delivery layer was applied last on the way
    // out, so it comes off first on the way in.
    for (const TransportTransformRow* row : rows_)
        apply_row(*row, peeled);
    return peeled;
}

PeeledLine TransportStack::peel(const insight::tokenization::NormalizedLine& line) const noexcept
{
    // One algorithm, run over the NORMALIZED bytes; the result is re-expressed as a suffix of the
    // NormalizedLine, which is what entitles this door to hand back a NormalizedContent — the peel
    // only ever SHORTENS from the head, so the narrowing preserves the stage-1 evidence by
    // construction. From this seat the suffix width comes from DECLARED catalogue rows; the door's
    // name records the OTHER producer's limitation (an inferred strip), not this caller's.
    const RawPeeledLine raw{peel_raw(line.bytes())};
    const std::size_t offset{static_cast<std::size_t>(raw.content.data() - line.bytes().data())};
    return PeeledLine{.content = line.undeclared_suffix(offset),
                      .observation_time = raw.observation_time};
}

bool render_transport_prefix(const TransportTransformRow& row, insight::Timestamp stamp,
                             std::string& out)
{
    switch (row.kind)
    {
    case TransportTransformKind::LinePrefixTimestamp:
        // NO writer dual, deliberately (T5 §3.3): the GHA API stamp is the platform's, baked into
        // the GHA IntentFormat's own writer — see the interface contract.
        return false;
    case TransportTransformKind::LinePrefixBracketedTimestamp:
        break;
    }

    // The ONE fixed lexical form: `[YYYY-MM-DDTHH:MM:SS.mmmZ]` + one separator space — 27 bytes,
    // filled into a stack scratch and appended in one call (allocation-free on this function's
    // own account; the caller's buffer amortizes to steady-state capacity).
    static constexpr std::int64_t kMillisPerSecond{1000};
    static constexpr std::int64_t kMillisPerDay{86'400'000};
    static constexpr std::int64_t kSecondsPerMinute{60};
    static constexpr std::int64_t kMinutesPerHour{60};
    static constexpr std::size_t kPrefixBytes{27U};
    static constexpr std::int64_t kMaxRenderableYear{9999};

    const std::int64_t total_millis{
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::floor<std::chrono::milliseconds>(stamp.time_since_epoch()))
            .count()};
    std::int64_t days{total_millis / kMillisPerDay};
    std::int64_t millis_in_day{total_millis % kMillisPerDay};
    if (millis_in_day < 0)
    {
        millis_in_day += kMillisPerDay;
        --days;
    }
    const CivilDate date{civil_from_days(days)};
    if (date.year < 0 || date.year > kMaxRenderableYear)
        return false; // outside the four-digit window the fixed form can spell — never a wrong prefix

    const std::int64_t millis{millis_in_day % kMillisPerSecond};
    const std::int64_t seconds_in_day{millis_in_day / kMillisPerSecond};
    const std::int64_t second{seconds_in_day % kSecondsPerMinute};
    const std::int64_t minute{(seconds_in_day / kSecondsPerMinute) % kMinutesPerHour};
    const std::int64_t hour{seconds_in_day / (kSecondsPerMinute * kMinutesPerHour)};

    std::array<char, kPrefixBytes> scratch{};
    std::size_t pos{0};
    scratch[pos++] = '[';
    put_two_digits(scratch, pos, static_cast<unsigned>(date.year / 100));
    put_two_digits(scratch, pos, static_cast<unsigned>(date.year % 100));
    scratch[pos++] = '-';
    put_two_digits(scratch, pos, date.month);
    scratch[pos++] = '-';
    put_two_digits(scratch, pos, date.day);
    scratch[pos++] = 'T';
    put_two_digits(scratch, pos, static_cast<unsigned>(hour));
    scratch[pos++] = ':';
    put_two_digits(scratch, pos, static_cast<unsigned>(minute));
    scratch[pos++] = ':';
    put_two_digits(scratch, pos, static_cast<unsigned>(second));
    scratch[pos++] = '.';
    scratch[pos++] = static_cast<char>('0' + (millis / 100));
    put_two_digits(scratch, pos, static_cast<unsigned>(millis % 100));
    scratch[pos++] = 'Z';
    scratch[pos++] = ']';
    scratch[pos++] = ' ';
    out.append(scratch.data(), scratch.size());
    return true;
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
