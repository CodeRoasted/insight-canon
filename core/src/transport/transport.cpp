module;

module insight.canon.transport;
import insight.canon.internal;
import insight.canon.api;

// refs: ADR-23.D3, ADR-23.D4
// invariant: canon owns every transform ALGORITHM and the catalogue owns only rows, so a new
// transform kind is a catalogue-version bump, never a package-local peel.
namespace insight::transport
{

namespace
{

    [[nodiscard]] constexpr bool is_space(char chr) noexcept
    {
        return chr == ' ' || chr == '\t';
    }

    // refs: ADR-22, ADR-23
    // invariant: the declared width is a CLAIM about the bytes, never a promise: the stamp is
    // shape-checked and the row declines rather than deducing.
    // note: a row whose bytes do not match peels nothing; that is the rule's effect.
    [[nodiscard]] constexpr bool has_stamp_at_head(std::string_view line,
                                                   std::size_t width) noexcept
    {
        // assert: the live case is `width == 0`, which would otherwise match every line carrying no
        // datetime at all.
        constexpr std::size_t kMinDatetimeLen{19U};
        return width >= kMinDatetimeLen &&
               insight::utils::rfc3339_datetime_length(line, 0U) == width;
    }

    // refs: ADR-23.D6, ADR-23.O2
    // invariant: `peel` and `peel_raw` share ONE algorithm and differ only in what the parameter
    // proves and the return type states, never in bytes.
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
                return;

            if (row.extract == TransportExtract::EventObservationTime)
            {
                // invariant: enrichment only — never an ordering key, never asserted monotone,
                // never a replay input.
                peeled.observation_time =
                    insight::utils::parse_iso8601(peeled.content.substr(0U, width));
            }
            peeled.content.remove_prefix(width);
            strip_separator(row, peeled);
            break;
        }
        case TransportTransformKind::LinePrefixBracketedTimestamp:
        {
            // invariant: `rfc3339_datetime_length` is the single owner of the full-datetime byte
            // grammar; this kind is variable width and leaves `prefix_width` unread.
            if (peeled.content.empty() || peeled.content.front() != '[')
                return;
            const std::size_t datetime_len{
                insight::utils::rfc3339_datetime_length(peeled.content, 1U)};
            if (datetime_len == 0U)
                return;
            const std::size_t close{1U + datetime_len};
            if (close >= peeled.content.size() || peeled.content[close] != ']')
                return;

            if (row.extract == TransportExtract::EventObservationTime)
            {
                peeled.observation_time =
                    insight::utils::parse_iso8601(peeled.content.substr(1U, datetime_len));
            }
            peeled.content.remove_prefix(close + 1U);
            strip_separator(row, peeled);
            break;
        }
        case TransportTransformKind::LinePrefixByteOrderMark:
        {
            // refs: DN-25.D3
            // invariant: all three bytes are shape-checked, so `EF BB` alone and the UTF-16 marks
            // survive untouched; removed once and never in a loop, a second mark is content.
            // note: nothing is extracted and no separator is stripped — a mark carries no datum.
            static constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF"};
            if (!peeled.content.starts_with(kUtf8Bom))
                return;
            peeled.content.remove_prefix(kUtf8Bom.size());
            break;
        }
        }
    }

    // invariant: pure integer — no <ctime>, no locale, no wall-clock reaches this decomposition.
    // note: Hinnant's civil_from_days; the era and cycle constants are the algorithm's.
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
        const unsigned day_of_year{day_of_era -
                                   ((365 * year_of_era) + (year_of_era / 4) - (year_of_era / 100))};
        const unsigned shifted_month{((5 * day_of_year) + 2) / 153};
        const unsigned day{(day_of_year - (((153 * shifted_month) + 2) / 5)) + 1};
        const unsigned month{shifted_month < 10 ? shifted_month + 3 : shifted_month - 9};
        return {.year = year + (month <= 2 ? 1 : 0), .month = month, .day = day};
    }

    // pre: `pos` and `pos + 1` are inside `out`.
    constexpr void put_two_digits(std::span<char> out, std::size_t& pos, unsigned value) noexcept
    {
        out[pos] = static_cast<char>('0' + (value / 10U));
        out[pos + 1U] = static_cast<char>('0' + (value % 10U));
        pos += 2U;
    }

    // refs: ADR-23
    // invariant: an unknown transform name fails closed — a MISTAKE must not share a code path
    // with an ABSENT stack, which is a choice.
    // note: the text names no shelf — canon ships public and a reader cannot open one.
    [[noreturn]] void fail_unknown_transform(std::string_view name)
    {
        std::cerr << "FATAL: insight::transport::resolve_transport_stack — unknown transport "
                     "transform \""
                  << name << "\". The catalogue (" << kTransportCatalogVersion << ") declares: ";
        for (std::size_t i{0}; i < kTransportCatalogRows.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << kTransportCatalogRows[i].name << '"';
        std::cerr << ".\nA transport stack is caller-declared provenance, never guessed: canon "
                     "VERIFIES, it does not infer. An unknown transform is a MISTAKE and fails "
                     "closed here; an ABSENT stack is a CHOICE and peels nothing. Declare one of "
                     "the names above, or declare no stack at all.\n"
                     "(A driver may accept its own spelling for the empty stack — `sift` takes "
                     "`--transport none` — but no such token reaches this catalogue.)\n";
        std::terminate();
    }

} // namespace

RawPeeledLine TransportStack::peel_raw(std::string_view line) const noexcept
{
    RawPeeledLine peeled{.content = line, .observation_time = std::nullopt};
    // invariant: outside-in, in declaration order — the outermost delivery layer was applied last
    // on the way out, so it comes off first on the way in.
    for (const TransportTransformRow* row : rows_)
        apply_row(*row, peeled);
    return peeled;
}

PeeledLine TransportStack::peel(const insight::tokenization::NormalizedLine& line) const noexcept
{
    // post: the peel only ever SHORTENS from the head, so re-expressing the result as a suffix
    // preserves the stage-1 evidence by construction.
    const RawPeeledLine raw{peel_raw(line.bytes())};
    const std::size_t offset{static_cast<std::size_t>(raw.content.data() - line.bytes().data())};
    return PeeledLine{.content = line.undeclared_suffix(offset),
                      .observation_time = raw.observation_time};
}

std::size_t render_transport_prefix(const TransportTransformRow& row, insight::Timestamp stamp,
                                    std::span<char> out) noexcept
{
    switch (row.kind)
    {
    case TransportTransformKind::LinePrefixTimestamp:
        return 0U;
    case TransportTransformKind::LinePrefixBracketedTimestamp:
        break;
    case TransportTransformKind::LinePrefixByteOrderMark:
        return 0U;
    }

    if (out.size() < kBracketedTimestampPrefixBytes)
        return 0U;

    // invariant: composed in a stack scratch and copied once, so a 0 return never leaves the caller
    // a buffer holding a half-formed prefix.
    static constexpr std::int64_t kMillisPerSecond{1000};
    static constexpr std::int64_t kMillisPerDay{86'400'000};
    static constexpr std::int64_t kSecondsPerMinute{60};
    static constexpr std::int64_t kMinutesPerHour{60};
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
        return 0U;

    const std::int64_t millis{millis_in_day % kMillisPerSecond};
    const std::int64_t seconds_in_day{millis_in_day / kMillisPerSecond};
    const std::int64_t second{seconds_in_day % kSecondsPerMinute};
    const std::int64_t minute{(seconds_in_day / kSecondsPerMinute) % kMinutesPerHour};
    const std::int64_t hour{seconds_in_day / (kSecondsPerMinute * kMinutesPerHour)};

    std::array<char, kBracketedTimestampPrefixBytes> scratch{};
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
    std::ranges::copy(scratch, out.begin());
    return scratch.size();
}

bool render_transport_prefix(const TransportTransformRow& row, insight::Timestamp stamp,
                             std::string& out)
{
    std::array<char, kBracketedTimestampPrefixBytes> rendered{};
    const std::size_t written{render_transport_prefix(row, stamp, std::span<char>{rendered})};
    if (written == 0U)
        return false;
    out.append(rendered.data(), written);
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
