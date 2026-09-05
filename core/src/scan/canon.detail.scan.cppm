// refs: ADR-3.D4
// invariant: SEALED — never re-exported by the facade and never installed (a PRIVATE file set);
// the bottom of the canon detail DAG, importing internal only.
// invariant: interface-only — every entity here is inline or constexpr, so the module has no
// implementation unit.
module;
// note: MSVC never defines __SSE2__; it announces x64 as _M_X64 — detect the target.
// invariant: INSIGHT_CANON_SSE2 gates BOTH the include and every use below, so a non-x86 target
// compiles the scalar fallback instead of failing on an undeclared intrinsic.
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
// note: a feature-test gate read only by #ifdef, never as a value — hence no value.
#define INSIGHT_CANON_SSE2
#include <emmintrin.h>
#endif

export module insight.canon.detail.scan;
import insight.canon.internal;

// invariant: every predicate here accepts a STRICT SUBSET of what its strategy's parse() accepts at
// the same anchor — parse() catches a false positive, nothing catches a false negative.
// invariant: each is_*_prefix predicate is true iff `str` BEGINS with the shape its name describes,
// at offset 0 unless the name says otherwise.
export namespace insight::tokenization
{

[[nodiscard]] constexpr bool is_digit(char chr) noexcept
{
    static constexpr unsigned kDecimalRadix{10U};
    return static_cast<unsigned>(chr) - '0' < kDecimalRadix;
}

[[nodiscard]] constexpr bool is_upper(char chr) noexcept
{
    static constexpr unsigned kAlphabetSize{26U};
    return static_cast<unsigned>(chr) - 'A' < kAlphabetSize;
}

[[nodiscard]] constexpr bool is_lower(char chr) noexcept
{
    static constexpr unsigned kAlphabetSize{26U};
    return static_cast<unsigned>(chr) - 'a' < kAlphabetSize;
}

// invariant: composed from the upper/lower pair, so a non-ASCII (signed-negative) byte is false by
// construction.
// note: THE alpha predicate here — mask and format_detector route to it, never a copy.
[[nodiscard]] constexpr bool is_alpha(char chr) noexcept
{
    return is_upper(chr) || is_lower(chr);
}

// invariant: space and tab only — the strategy patterns spell \s+, and only these two occur at
// the prefixes they check.
[[nodiscard]] constexpr bool is_space(char chr) noexcept
{
    return chr == ' ' || chr == '\t';
}

// refs: SRC-D-MSK-4, SRC-D-MSK-6
// invariant: FROZEN, DECLARED byte pairs — a shell is NOT part of the value it surrounds, and no
// mask rule may treat it as one.
// invariant: ONE catalog for every reader — a second copy is how two maskers diverge and template
// identity stops being a pure function of the line.
// note: only the OPENING byte was ever the defect — a closer leaves byte 0 a digit.
struct WrapperPair
{
    char open;
    char close;
};
inline constexpr std::array<WrapperPair, 6> kWrapperPairs{{
    {.open = '[', .close = ']'},
    {.open = '(', .close = ')'},
    {.open = '{', .close = '}'},
    {.open = '<', .close = '>'},
    {.open = '"', .close = '"'},
    {.open = '\'', .close = '\''},
}};

[[nodiscard]] constexpr bool is_wrapper_open(char chr) noexcept
{
    return std::ranges::any_of(kWrapperPairs,
                               [chr](const WrapperPair& pair) { return pair.open == chr; });
}

[[nodiscard]] constexpr bool is_wrapper_close(char chr) noexcept
{
    return std::ranges::any_of(kWrapperPairs,
                               [chr](const WrapperPair& pair) { return pair.close == chr; });
}

// refs: ADR-16.D5, SRC-D-TID-9, SRC-D-TID-12
// invariant: each field is the byte-exact equivalent of the scan it replaces, so the
// KEEP/MASK/NORMALIZE decision per token — and hence the template id — is unchanged.
// invariant: byte-only, single-token, no float and order-independent, so the profile is
// bit-identical across stdlibs and on MSVC.
// note: one walk of the token's bytes replaces three the mask dispatch used to make.
struct TokenShape
{
    bool empty{true};
    bool all_digits{false};
    bool digit_leading{false};
    // invariant: embedded_identity is the ONLY rule the wrapper bytes newly reach — every other
    // normalizer already carries its own trigger.
    // note: the trigger set is `: / # - =` plus every kWrapperPairs byte.
    bool has_separator{false};

    explicit constexpr TokenShape(std::string_view tok) noexcept : empty(tok.empty())
    {
        if (empty)
            return;
        std::size_t lead{0};
        if (tok[0] == '+' || tok[0] == '-')
            ++lead;
        digit_leading = lead < tok.size() && is_digit(tok[lead]);
        all_digits = true;
        for (const char chr : tok)
        {
            all_digits = all_digits && is_digit(chr);
            has_separator = has_separator || chr == ':' || chr == '/' || chr == '#' || chr == '-' ||
                            chr == '=' || is_wrapper_open(chr) || is_wrapper_close(chr);
        }
    }
};

[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view str, std::size_t pos) noexcept
{
    while (pos < str.size() && is_space(str[pos]))
        ++pos;
    return pos;
}

// post: advances `pos` over at most max_n digits, and is true iff at least min_n were consumed.
[[nodiscard]] constexpr bool consume_digits(std::string_view str, std::size_t& pos,
                                            std::size_t min_n, std::size_t max_n) noexcept
{
    const std::size_t start{pos};
    while (pos < str.size() && (pos - start) < max_n && is_digit(str[pos]))
        ++pos;
    return (pos - start) >= min_n;
}

// invariant: namespace-scope so a strategy parse() body importing this module reads the SAME bound;
// a function-local copy would be a second constant.
constexpr std::size_t kBsdMinLen{15U};
constexpr std::size_t kHdfsMinLen{16U};

[[nodiscard]] constexpr bool match_iso_date_at(std::string_view str, std::size_t pos) noexcept
{
    static constexpr std::size_t kIsoDateLen{10U};
    static constexpr std::size_t kIsoMon1{5U};
    static constexpr std::size_t kIsoMon2{6U};
    static constexpr std::size_t kIsoSep2{7U};
    static constexpr std::size_t kIsoDd1{8U};
    static constexpr std::size_t kIsoDd2{9U};
    if (pos + kIsoDateLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && is_digit(str[pos + 2]) &&
           is_digit(str[pos + 3]) && str[pos + 4] == '-' && is_digit(str[pos + kIsoMon1]) &&
           is_digit(str[pos + kIsoMon2]) && str[pos + kIsoSep2] == '-' &&
           is_digit(str[pos + kIsoDd1]) && is_digit(str[pos + kIsoDd2]);
}

[[nodiscard]] constexpr bool match_time_at(std::string_view str, std::size_t pos) noexcept
{
    static constexpr std::size_t kTimeLen{8U};
    static constexpr std::size_t kTimeColon2{5U};
    static constexpr std::size_t kTimeSec1{6U};
    static constexpr std::size_t kTimeSec2{7U};
    if (pos + kTimeLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           is_digit(str[pos + 3]) && is_digit(str[pos + 4]) && str[pos + kTimeColon2] == ':' &&
           is_digit(str[pos + kTimeSec1]) && is_digit(str[pos + kTimeSec2]);
}

[[nodiscard]] constexpr bool is_bsd_syslog_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kTimeLen{8U};
    if (str.size() < kBsdMinLen)
        return false;
    if (!is_upper(str[0]) || !is_lower(str[1]) || !is_lower(str[2]))
        return false;
    std::size_t pos{3};
    pos = skip_spaces(str, pos);
    if (pos >= str.size() || !is_digit(str[pos]))
        return false;
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    pos = skip_spaces(str, pos);
    if (pos + kTimeLen > str.size())
        return false;
    return match_time_at(str, pos);
}

[[nodiscard]] constexpr bool is_rfc3339_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kRfc3339TAt{10U};
    static constexpr std::size_t kRfc3339TimeAt{11U};
    return match_iso_date_at(str, 0) && str.size() > kRfc3339TAt && str[kRfc3339TAt] == 'T' &&
           match_time_at(str, kRfc3339TimeAt);
}

[[nodiscard]] constexpr bool is_iso_datetime_space_prefix(std::string_view str,
                                                          bool require_fraction) noexcept
{
    static constexpr std::size_t kIsoDateLen{10U};
    static constexpr std::size_t kTimeLen{8U};
    if (!match_iso_date_at(str, 0))
        return false;
    if (str.size() <= kIsoDateLen || !is_space(str[kIsoDateLen]))
        return false;
    std::size_t pos{skip_spaces(str, kIsoDateLen)};
    if (!match_time_at(str, pos))
        return false;
    if (!require_fraction)
        return true;
    pos += kTimeLen;
    return pos < str.size() && (str[pos] == '.' || str[pos] == ',');
}

[[nodiscard]] constexpr bool is_nginx_error_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kNginxMinLen{22U};
    static constexpr std::size_t kNginxMon1{5U};
    static constexpr std::size_t kNginxMon2{6U};
    static constexpr std::size_t kNginxSlash2{7U};
    static constexpr std::size_t kNginxDd1{8U};
    static constexpr std::size_t kNginxDd2{9U};
    static constexpr std::size_t kNginxSpaceAt{10U};
    static constexpr std::size_t kNginxTimeAt{11U};
    static constexpr std::size_t kNginxScanFrom{19U};
    static constexpr std::size_t kNginxScanTo{32U};
    if (str.size() < kNginxMinLen)
        return false;
    if (!(is_digit(str[0]) && is_digit(str[1]) && is_digit(str[2]) && is_digit(str[3]) &&
          str[4] == '/' && is_digit(str[kNginxMon1]) && is_digit(str[kNginxMon2]) &&
          str[kNginxSlash2] == '/' && is_digit(str[kNginxDd1]) && is_digit(str[kNginxDd2])))
        return false;
    if (!is_space(str[kNginxSpaceAt]))
        return false;
    if (!match_time_at(str, kNginxTimeAt))
        return false;
    for (std::size_t i{kNginxScanFrom}; i < str.size() && i < kNginxScanTo; ++i)
        if (str[i] == '[')
            return true;
    return false;
}

[[nodiscard]] constexpr bool is_spark_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kSparkMinLen{17U};
    static constexpr std::size_t kSparkSlash2{5U};
    static constexpr std::size_t kSparkMon1{6U};
    static constexpr std::size_t kSparkMon2{7U};
    static constexpr std::size_t kSparkSpaceAt{8U};
    static constexpr std::size_t kSparkTimeAt{9U};
    if (str.size() < kSparkMinLen)
        return false;
    if (!(is_digit(str[0]) && is_digit(str[1]) && str[2] == '/' && is_digit(str[3]) &&
          is_digit(str[4]) && str[kSparkSlash2] == '/' && is_digit(str[kSparkMon1]) &&
          is_digit(str[kSparkMon2])))
        return false;
    if (!is_space(str[kSparkSpaceAt]))
        return false;
    return match_time_at(str, kSparkTimeAt);
}

[[nodiscard]] constexpr bool is_hdfs_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kHdfsDateLen{6U};
    static constexpr std::size_t kHdfsSpaceAt{6U};
    static constexpr std::size_t kHdfsTimeStart{7U};
    static constexpr std::size_t kHdfsTimeEnd{13U};
    static constexpr std::size_t kHdfsLastSpace{13U};
    static constexpr std::size_t kHdfsLastDigit{14U};
    if (str.size() < kHdfsMinLen)
        return false;
    for (std::size_t i{0}; i < kHdfsDateLen; ++i)
        if (!is_digit(str[i]))
            return false;
    if (!is_space(str[kHdfsSpaceAt]))
        return false;
    for (std::size_t i{kHdfsTimeStart}; i < kHdfsTimeEnd; ++i)
        if (!is_digit(str[i]))
            return false;
    return is_space(str[kHdfsLastSpace]) && is_digit(str[kHdfsLastDigit]);
}

[[nodiscard]] constexpr bool is_proxifier_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kProxifierMinLen{17U};
    static constexpr std::size_t kProxifierMm2{5U};
    static constexpr std::size_t kProxifierSpaceAt{6U};
    static constexpr std::size_t kProxifierTimeAt{7U};
    static constexpr std::size_t kProxifierBracket{15U};
    if (str.size() < kProxifierMinLen || str[0] != '[')
        return false;
    if (!(is_digit(str[1]) && is_digit(str[2]) && str[3] == '.' && is_digit(str[4]) &&
          is_digit(str[kProxifierMm2])))
        return false;
    if (!is_space(str[kProxifierSpaceAt]))
        return false;
    if (!match_time_at(str, kProxifierTimeAt))
        return false;
    return str.size() > kProxifierBracket && str[kProxifierBracket] == ']';
}

[[nodiscard]] constexpr bool is_rfc5424_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kRfc5424MinLen{6U};
    if (str.size() < kRfc5424MinLen || str[0] != '<')
        return false;
    std::size_t pos{1U};
    if (!consume_digits(str, pos, 1U, 3U))
        return false;
    if (pos >= str.size() || str[pos] != '>')
        return false;
    ++pos;
    if (pos >= str.size() || !is_digit(str[pos]))
        return false;
    ++pos;
    if (pos >= str.size() || !is_space(str[pos]))
        return false;
    return match_iso_date_at(str, skip_spaces(str, pos));
}

[[nodiscard]] constexpr bool is_apache_error_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kApacheMinLen{22U};
    static constexpr std::size_t kApacheMon1{5U};
    static constexpr std::size_t kApacheMon2{6U};
    static constexpr std::size_t kApacheMon3{7U};
    static constexpr std::size_t kApacheDayAt{8U};
    if (str.size() < kApacheMinLen || str[0] != '[')
        return false;
    if (!(is_upper(str[1]) && is_lower(str[2]) && is_lower(str[3]) && is_space(str[4]) &&
          is_upper(str[kApacheMon1]) && is_lower(str[kApacheMon2]) && is_lower(str[kApacheMon3])))
        return false;
    std::size_t pos{kApacheDayAt};
    pos = skip_spaces(str, pos);
    if (pos + 2U > str.size() || !is_digit(str[pos]) || !is_digit(str[pos + 1]))
        return false;
    pos += 2U;
    pos = skip_spaces(str, pos);
    return match_time_at(str, pos);
}

// invariant: two RAS columns share this byte class — the alert LABEL and SUBSYS — and differ
// only in a length bound, which each caller carries.
[[nodiscard]] constexpr bool is_bgl_identifier_byte(char chr) noexcept
{
    return is_upper(chr) || is_digit(chr) || chr == '_';
}

// refs: DN-43.D14
// invariant: the alert-class column is part of the GRAMMAR and of NO projection field — an
// instrument must never ingest its own oracle as a feature.
// note: the column is the corpus curators' answer key, written by no producer.
[[nodiscard]] constexpr bool is_bgl_labelled_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kBglMinLen{14U};
    static constexpr std::size_t kBglMaxDigits{20U};
    static constexpr std::size_t kBglLabelMaxLen{16U};
    static constexpr std::size_t kBglDateLen{10U};
    static constexpr std::size_t kBglMon1{5U};
    static constexpr std::size_t kBglMon2{6U};
    static constexpr std::size_t kBglSep2{7U};
    static constexpr std::size_t kBglDay1{8U};
    static constexpr std::size_t kBglDay2{9U};
    if (str.size() < kBglMinLen)
        return false;
    std::size_t pos{1U};
    if (str[0] != '-')
    {
        if (!is_upper(str[0]))
            return false;
        while (pos < str.size() && pos < kBglLabelMaxLen && is_bgl_identifier_byte(str[pos]))
            ++pos;
    }
    // refs: DN-43.D2
    // assert: the label is a TOKEN and must end at whitespace — otherwise the length bound
    // silently truncates an over-long run and hands `<epoch>` a suffix of it.
    if (pos >= str.size() || !is_space(str[pos]))
        return false;
    pos = skip_spaces(str, pos);
    if (!consume_digits(str, pos, 1U, kBglMaxDigits))
        return false;
    pos = skip_spaces(str, pos);
    if (pos + kBglDateLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && is_digit(str[pos + 2]) &&
           is_digit(str[pos + 3]) && str[pos + 4] == '.' && is_digit(str[pos + kBglMon1]) &&
           is_digit(str[pos + kBglMon2]) && str[pos + kBglSep2] == '.' &&
           is_digit(str[pos + kBglDay1]) && is_digit(str[pos + kBglDay2]);
}

[[nodiscard]] constexpr bool is_health_app_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kHealthMinLen{18U};
    static constexpr std::size_t kHealthDateLen{8U};
    static constexpr std::size_t kHealthSepAt{8U};
    static constexpr std::size_t kHealthTimeAt{9U};
    if (str.size() < kHealthMinLen)
        return false;
    for (std::size_t i{0}; i < kHealthDateLen; ++i)
        if (!is_digit(str[i]))
            return false;
    if (str[kHealthSepAt] != '-')
        return false;
    // refs: DN-43.O5
    // assert: every time field is variable width — LogHub's own HealthApp sample is not
    // zero-padded anywhere.
    // note: the generator zero-pads, so nine green generations could not see this gap.
    std::size_t pos{kHealthTimeAt};
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    if (pos >= str.size() || str[pos] != ':')
        return false;
    ++pos;
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    if (pos >= str.size() || str[pos] != ':')
        return false;
    ++pos;
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    if (pos >= str.size() || str[pos] != ':')
        return false;
    ++pos;
    if (!consume_digits(str, pos, 1U, 3U))
        return false;
    if (pos >= str.size() || str[pos] != '|')
        return false;
    // refs: DN-43.D16
    // assert: proving the two remaining separators HERE is what makes parse()'s three unconditional
    // takes total.
    // note: a parse()-side decline deletes the line; a decline here demotes it to raw text.
    const std::string_view::size_type second_sep{str.find('|', pos + 1U)};
    if (second_sep == std::string_view::npos)
        return false;
    return str.find('|', second_sep + 1U) != std::string_view::npos;
}

[[nodiscard]] constexpr bool is_hpc_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kHpcMaxDigits{20U};
    static constexpr std::size_t kHpcMinTsLen{10U};
    std::size_t pos{0};
    if (!consume_digits(str, pos, 1U, kHpcMaxDigits))
        return false;
    for (int i{0}; i < 3; ++i)
    {
        pos = skip_spaces(str, pos);
        if (pos >= str.size() || is_space(str[pos]))
            return false;
        while (pos < str.size() && !is_space(str[pos]))
            ++pos;
    }
    pos = skip_spaces(str, pos);
    const std::size_t ts_start{pos};
    while (pos < str.size() && is_digit(str[pos]))
        ++pos;
    if (pos - ts_start < kHpcMinTsLen)
        return false;
    pos = skip_spaces(str, pos);
    if (pos >= str.size() || !is_digit(str[pos]))
        return false;
    while (pos < str.size() && is_digit(str[pos]))
        ++pos;
    return pos < str.size() && is_space(str[pos]);
}

[[nodiscard]] constexpr bool has_clf_timestamp(std::string_view str) noexcept
{
    static constexpr std::size_t kClfMinLen{22U};
    static constexpr std::size_t kClfMon3{5U};
    static constexpr std::size_t kClfSlash2{6U};
    static constexpr std::size_t kClfYear1{7U};
    static constexpr std::size_t kClfYear2{8U};
    static constexpr std::size_t kClfYear3{9U};
    static constexpr std::size_t kClfYear4{10U};
    static constexpr std::size_t kClfColon{11U};
    static constexpr std::size_t kClfTimeAt{12U};
    if (str.size() < kClfMinLen)
        return false;
    const std::size_t limit{str.size() - 21U};
    for (std::size_t i{0}; i <= limit; ++i)
    {
        if (str[i] != '[')
            continue;
        const std::size_t pos{i + 1U};
        if (!is_digit(str[pos]) || !is_digit(str[pos + 1]) || str[pos + 2] != '/' ||
            !is_upper(str[pos + 3]) || !is_lower(str[pos + 4]) || !is_lower(str[pos + kClfMon3]) ||
            str[pos + kClfSlash2] != '/')
            continue;
        if (!is_digit(str[pos + kClfYear1]) || !is_digit(str[pos + kClfYear2]) ||
            !is_digit(str[pos + kClfYear3]) || !is_digit(str[pos + kClfYear4]) ||
            str[pos + kClfColon] != ':')
            continue;
        if (match_time_at(str, pos + kClfTimeAt))
            return true;
    }
    return false;
}

[[nodiscard]] constexpr std::size_t count_kv_pair_signatures(std::string_view str,
                                                             std::size_t cap) noexcept
{
    std::size_t found{0};
    for (std::size_t i{1U}; i + 1U < str.size() && found < cap; ++i)
    {
        if (str[i] != '=')
            continue;
        const char prev{str[i - 1]};
        const char next{str[i + 1]};
        const bool key_ok{is_lower(prev) || is_upper(prev) || is_digit(prev) || prev == '_' ||
                          prev == '.' || prev == '-'};
        const bool val_ok{!is_space(next) && next != '='};
        if (key_ok && val_ok)
            ++found;
    }
    return found;
}

// refs: SRC-D-TID-9
// invariant: the SIMD and scalar paths return identical pointers, so the canonical digest is
// invariant to whether SSE2 was compiled in.
// note: the scalar loop IS the SIMD remainder handler, reused as the whole range.
namespace simd_detail
{

    constexpr int kSseWidth{16};
    constexpr unsigned kSseMaskAll{0xFFFFU};

    // note: the intrinsics need a reinterpret_cast; the bounded views are not null-terminated.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast, bugprone-not-null-terminated-result)
    [[nodiscard]] inline const char* find_non_ws_ptr(std::string_view str) noexcept
    {
        const char* ptr = str.data();
        const char* const end = str.data() + str.size();
#ifdef INSIGHT_CANON_SSE2
        const __m128i vsp = _mm_set1_epi8(' ');
        const __m128i vtab = _mm_set1_epi8('\t');
        while (ptr + kSseWidth <= end)
        {
            const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            const auto mask = static_cast<unsigned>(_mm_movemask_epi8(
                _mm_or_si128(_mm_cmpeq_epi8(chunk, vsp), _mm_cmpeq_epi8(chunk, vtab))));
            if ((mask & kSseMaskAll) != kSseMaskAll)
            {
                // assert: `~mask & kSseMaskAll` is non-zero here, the block holding a non-ws byte,
                // so countr_zero is bit-exact with __builtin_ctz.
                // note: std::countr_zero is portable to MSVC; __builtin_ctz was gcc/clang only.
                ptr += std::countr_zero(~mask & kSseMaskAll);
                return ptr;
            }
            ptr += kSseWidth;
        }
#endif
        while (ptr < end && is_space(*ptr))
            ++ptr;
        return ptr;
    }

    [[nodiscard]] inline const char* find_ws_ptr(std::string_view str) noexcept
    {
        const char* ptr = str.data();
        const char* const end = str.data() + str.size();
#ifdef INSIGHT_CANON_SSE2
        const __m128i vsp = _mm_set1_epi8(' ');
        const __m128i vtab = _mm_set1_epi8('\t');
        while (ptr + kSseWidth <= end)
        {
            const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            const auto mask = static_cast<unsigned>(_mm_movemask_epi8(
                _mm_or_si128(_mm_cmpeq_epi8(chunk, vsp), _mm_cmpeq_epi8(chunk, vtab))));
            if ((mask & kSseMaskAll) != 0U)
            {
                // assert: `mask & kSseMaskAll` has a set bit here, so countr_zero is bit-exact with
                // the __builtin_ctz it replaced.
                ptr += std::countr_zero(mask & kSseMaskAll);
                return ptr;
            }
            ptr += kSseWidth;
        }
#endif
        while (ptr < end && !is_space(*ptr))
            ++ptr;
        return ptr;
    }

    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast, bugprone-not-null-terminated-result)
} // namespace simd_detail

inline void sv_skip_ws(std::string_view& str) noexcept
{
    const char* const ptr = simd_detail::find_non_ws_ptr(str);
    str = str.substr(static_cast<std::size_t>(ptr - str.data()));
}

// post: the next non-whitespace token, with `str` advanced past it and past the whitespace that
// follows; empty when `str` is all whitespace.
// note: every substr pos is <= size, so this noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] inline std::string_view sv_take_token(std::string_view& str) noexcept
{
    const char* const non_ws = simd_detail::find_non_ws_ptr(str);
    const auto non_ws_off = static_cast<std::size_t>(non_ws - str.data());
    if (non_ws_off == str.size())
    {
        str = {};
        return {};
    }
    const std::string_view remaining{str.substr(non_ws_off)};
    const char* const ws_ptr = simd_detail::find_ws_ptr(remaining);
    const auto tok_size = static_cast<std::size_t>(ws_ptr - remaining.data());
    const std::string_view tok{remaining.substr(0, tok_size)};

    const std::string_view after_tok{remaining.substr(tok_size)};
    const char* const next = simd_detail::find_non_ws_ptr(after_tok);
    str = after_tok.substr(static_cast<std::size_t>(next - after_tok.data()));
    return tok;
}

// post: the bytes before `delim` with `str` advanced past it; all of `str`, and `str` emptied, when
// `delim` is absent.
// note: substr(pos + 1U) runs only after find() returned pos < size — cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] constexpr std::string_view sv_take_until(std::string_view& str, char delim) noexcept
{
    const auto pos = str.find(delim);
    if (pos == std::string_view::npos)
    {
        const auto result = str;
        str = {};
        return result;
    }
    const auto result = str.substr(0, pos);
    str = str.substr(pos + 1U);
    return result;
}

// refs: ADR-16.D9, ADR-16.D5, DN-43.D11
// post: the field is simply NOT NAMED when its terminator is absent — `str` is left untouched, so
// every byte survives into the caller's content.
// invariant: sv_take_until's no-delimiter branch hands back the WHOLE remainder, which moves a
// message body onto a cube dimension and templates the line to the hash of the empty string.
// note: an empty component positively states that the line declares no source.
[[nodiscard]] constexpr std::string_view sv_take_until_or_none(std::string_view& str,
                                                               char delim) noexcept
{
    if (str.find(delim) == std::string_view::npos)
        return {};
    return sv_take_until(str, delim);
}

// note: actual is min(n, size), so both substr calls are in range and cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] constexpr std::string_view sv_take_n(std::string_view& str, std::size_t n) noexcept
{
    const auto actual = n < str.size() ? n : str.size();
    const auto result = str.substr(0, actual);
    str = str.substr(actual);
    return result;
}

// post: the bracketed content with `str` advanced past `]`; on a view not opening `[`, an empty
// result and `str` untouched.
[[nodiscard]] constexpr std::string_view sv_take_bracketed(std::string_view& str) noexcept
{
    if (str.empty() || str[0] != '[')
        return {};
    str.remove_prefix(1U);
    return sv_take_until(str, ']');
}

// post: the quoted content without its quotes; on a view not opening `"`, an empty result and `str`
// untouched.
[[nodiscard]] constexpr std::string_view sv_take_quoted(std::string_view& str) noexcept
{
    if (str.empty() || str[0] != '"')
        return {};
    str.remove_prefix(1U);
    return sv_take_until(str, '"');
}

// refs: DN-43.D3, DN-43.D11, ADR-16.D9
// post: a tag bounded to ONE token ending in `:`, or in `[pid]:` closing inside that token,
// returned with its pid stripped and `str` left at the message body.
// invariant: when no tag is delimited NOTHING is removed — the result is empty and `str` is
// untouched, so no caller loses a body to a colon that was not a delimiter.
// note: syslog declines an empty result; BGL's Thunderbird branch keeps the remainder.
[[nodiscard]] inline std::string_view take_bounded_syslog_tag(std::string_view& str) noexcept
{
    static constexpr std::string_view kTagStop{"[: \t"};
    static constexpr std::string_view kPidStop{"] \t"};

    const auto stop{str.find_first_of(kTagStop)};
    if (stop == std::string_view::npos || is_space(str[stop]) || stop == 0U)
        return {};

    std::string_view tag{str};
    // note: remove_suffix keeps bytes [0, stop) and is noexcept, unlike substr.
    tag.remove_suffix(str.size() - stop);

    std::size_t body_at{stop + 1U};
    if (str[stop] == '[')
    {
        const auto close{str.find_first_of(kPidStop, stop + 1U)};
        if (close == std::string_view::npos || str[close] != ']' || close + 1U >= str.size() ||
            str[close + 1U] != ':')
            return {};
        body_at = close + 2U;
    }

    // assert: `body_at` is in range — the colon branch proved stop < size and the bracket branch
    // proved close + 1 < size.
    str.remove_prefix(body_at);
    sv_skip_ws(str);
    return tag;
}

} // namespace insight::tokenization
