// invariant: a golden that pins canon's whole OUTPUT for a named line population, read through the
// composed tokenizer: every projection member of every event, plus its template id.
// invariant: THE DEFECT THIS EXISTS FOR — the masking golden pins the masker alone, so a change to
// a strategy's content projection, to detection or to a role rule stayed green on every gate.
// invariant: a red here says canon's output moved; the masking golden says whether the masker did,
// and a red here with that golden green names everything around the masker.
// invariant: the population is the public determinism proof's corpus, one stream per file, plus
// the JSON reader-agreement fixtures and their escaped twins, plus the ISO 8601 zone witnesses.
// invariant: each stream runs through ONE zero-package tokenizer in file order, as the proof runs
// a file, so sticky detection state is part of what is pinned.
// invariant: REGENERATION IS AN ACTION, NOT A PROPERTY: it refuses to rewrite a row whose pinned
// value differs unless kCanonicalizationVersion already differs from the golden's header.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

namespace
{

constexpr std::string_view kVersionKey{"canonicalization_version = "};
constexpr std::string_view kStreamKey{"== stream: "};
constexpr std::string_view kInputKey{"> "};
constexpr char kExpectedMark{'='};
constexpr std::string_view kNoEvent{"<no event>"};

constexpr std::string_view kRegenCommand{
    "<build dir>/core/insight_canon_tests --gtest_also_run_disabled_tests "
    "--gtest_filter='ProjectionGolden.DISABLED_RegenerateGolden'"};

struct Row
{
    std::string stream;
    std::string input;
    // invariant: EMPTY means fill-me, honoured only by regeneration.
    std::string expected;
    std::size_t input_line_no{0};
    std::size_t expected_at{0};
};

struct Golden
{
    std::string version;
    std::vector<Row> rows;
    std::vector<std::string> lines;
};

// post: the input bytes of an escaped golden field: `\\`, `\t`, `\n` and `\r` are the four escapes,
// so a golden line can carry any byte a log line can.
[[nodiscard]] std::string unescape(std::string_view text)
{
    std::string out;
    for (std::size_t pos{0}; pos < text.size(); ++pos)
    {
        if (text[pos] != '\\' || pos + 1 == text.size())
        {
            out.push_back(text[pos]);
            continue;
        }
        ++pos;
        switch (text[pos])
        {
        case 't':
            out.push_back('\t');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        default:
            out.push_back(text[pos]);
        }
    }
    return out;
}

[[nodiscard]] Golden load_golden(std::string& why)
{
    Golden golden;
    std::ifstream file{INSIGHT_CANON_PROJECTION_GOLDEN_PATH, std::ios::binary};
    if (!file)
    {
        why = std::string{"cannot open the golden at "} + INSIGHT_CANON_PROJECTION_GOLDEN_PATH;
        return golden;
    }
    std::string text;
    std::string stream;
    bool awaiting_expected{false};
    while (std::getline(file, text))
    {
        golden.lines.push_back(text);
        const std::size_t line_no{golden.lines.size()};
        if (awaiting_expected)
        {
            if (text.empty() || text.front() != kExpectedMark)
            {
                why = "golden line " + std::to_string(line_no) +
                      " follows an input and is not an `=` line\n  line: " + text;
                return golden;
            }
            golden.rows.back().expected = text.substr(1);
            golden.rows.back().expected_at = line_no - 1;
            awaiting_expected = false;
            continue;
        }
        if (text.starts_with(kVersionKey))
            golden.version = text.substr(kVersionKey.size());
        else if (text.starts_with(kStreamKey))
            stream = text.substr(kStreamKey.size());
        else if (text.starts_with(kInputKey))
        {
            if (stream.empty())
            {
                why = "golden line " + std::to_string(line_no) + " is an input before any stream";
                return golden;
            }
            golden.rows.push_back(Row{.stream = stream,
                                      .input = unescape(text.substr(kInputKey.size())),
                                      .input_line_no = line_no});
            awaiting_expected = true;
        }
        else if (!text.empty() && !text.starts_with('#'))
        {
            why = "golden line " + std::to_string(line_no) +
                  " is no header, stream, input, `=` line or comment\n  line: " + text;
            return golden;
        }
    }
    if (awaiting_expected)
        why = "the golden ends on an input with no `=` line";
    else if (golden.version.empty())
        why = std::string{"the golden carries no `"} + std::string{kVersionKey} + "` header line";
    return golden;
}

struct Loaded
{
    Golden golden;
    std::string why;
};

[[nodiscard]] const Loaded& loaded()
{
    static const Loaded instance{[]
                                 {
                                     Loaded out;
                                     out.golden = load_golden(out.why);
                                     return out;
                                 }()};
    return instance;
}

// invariant: one arena, one zero-package composition and one tokenizer per STREAM.
struct Door
{
    static constexpr std::size_t kArenaSize{1U << 22U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, StreamContext{}};
};

// post: one rendered row per golden row, in order: the template id, then every projection member,
// tab-separated; a line that yields no event renders `<no event>`.
[[nodiscard]] std::vector<std::string> render_rows(const Golden& golden)
{
    std::vector<std::string> out;
    out.reserve(golden.rows.size());
    std::unique_ptr<Door> door;
    std::string_view stream;
    for (const Row& row : golden.rows)
    {
        if (door == nullptr || row.stream != stream)
        {
            door = std::make_unique<Door>();
            stream = row.stream;
        }
        const auto event{door->tokenizer.process_line(row.input)};
        if (!event.has_value())
        {
            out.emplace_back(kNoEvent);
            continue;
        }
        ProjectionColumns columns;
        render_projection(*event, columns);
        std::string rendered{render(template_id_of(event->template_str))};
        for (const std::string& column : columns)
            rendered += '\t' + column;
        out.push_back(std::move(rendered));
    }
    return out;
}

// post: the name of the first tab-separated cell where the two rows differ.
[[nodiscard]] std::string first_differing_member(std::string_view expected, std::string_view actual)
{
    std::size_t cell{0};
    std::size_t exp_pos{0};
    std::size_t act_pos{0};
    while (true)
    {
        const std::size_t exp_end{expected.find('\t', exp_pos)};
        const std::size_t act_end{actual.find('\t', act_pos)};
        if (expected.substr(exp_pos, exp_end - exp_pos) !=
            actual.substr(act_pos, act_end - act_pos))
        {
            if (cell == 0)
                return "template_id";
            return cell <= kProjectionMembers.size() ? std::string{kProjectionMembers[cell - 1]}
                                                     : std::string{"<extra cell>"};
        }
        if (exp_end == std::string_view::npos || act_end == std::string_view::npos)
            return "<cell count>";
        exp_pos = exp_end + 1;
        act_pos = act_end + 1;
        ++cell;
    }
}

} // namespace

TEST(ProjectionGolden, GoldenIsReadableAndNotEmpty)
{
    ASSERT_TRUE(loaded().why.empty()) << loaded().why;
    EXPECT_FALSE(loaded().golden.rows.empty())
        << "the golden holds no rows — an empty population pins nothing and passes vacuously";
}

TEST(ProjectionGolden, GoldenHeaderRecordsTheLiveCanonicalizationVersion)
{
    ASSERT_TRUE(loaded().why.empty()) << loaded().why;
    EXPECT_EQ(loaded().golden.version, insight::kCanonicalizationVersion)
        << "the golden's header names the generation its pinned rows belong to; a bump without "
           "a regeneration leaves the golden addressed to a contract that no longer exists.\n"
        << "  source: " << insight::kCanonicalizationVersion << "\n"
        << "  golden: " << loaded().golden.version << "\n  regenerate with:\n    " << kRegenCommand;
}

TEST(ProjectionGolden, EveryRowRendersByteIdentically)
{
    ASSERT_TRUE(loaded().why.empty()) << loaded().why;
    const Golden& golden{loaded().golden};
    const std::vector<std::string> actual{render_rows(golden)};
    std::size_t moved{0};
    for (std::size_t index{0}; index < golden.rows.size(); ++index)
    {
        const Row& row{golden.rows[index]};
        if (row.expected.empty())
        {
            ADD_FAILURE() << "golden line " << row.input_line_no << " (stream " << row.stream
                          << ") has NO pinned projection; fill it by running:\n  " << kRegenCommand;
            continue;
        }
        if (row.expected == actual[index])
            continue;
        ++moved;
        ADD_FAILURE() << "CANON OUTPUT MOVED — golden line " << row.input_line_no << " (stream "
                      << row.stream << ")\n  input   : " << row.input
                      << "\n  first differing member: "
                      << first_differing_member(row.expected, actual[index])
                      << "\n  expected: " << row.expected << "\n  actual  : " << actual[index];
    }
    if (moved != 0)
        ADD_FAILURE() << moved << " of " << golden.rows.size() << " rows moved.\n"
                      << "  kCanonicalizationVersion (source): "
                      << insight::kCanonicalizationVersion << "\n"
                      << "  canonicalization_version (golden): " << golden.version << "\n"
                      << (golden.version == insight::kCanonicalizationVersion
                              ? "  THE OUTPUT MOVED AND THE VERSION DID NOT. Restore the change, "
                                "or bump kCanonicalizationVersion with its ledger entry, then "
                                "regenerate:\n    "
                              : "  The version was bumped; regenerate and review the diff:\n    ")
                      << kRegenCommand;
}

TEST(ProjectionGolden, DISABLED_RegenerateGolden)
{
    ASSERT_TRUE(loaded().why.empty()) << loaded().why;
    const Golden& golden{loaded().golden};
    const bool version_moved{golden.version != insight::kCanonicalizationVersion};
    const std::vector<std::string> actual{render_rows(golden)};

    std::vector<std::string> lines{golden.lines};
    std::string refused;
    for (std::size_t index{0}; index < golden.rows.size(); ++index)
    {
        const Row& row{golden.rows[index]};
        if (!row.expected.empty() && row.expected != actual[index] && !version_moved)
            refused += "\n    line " + std::to_string(row.input_line_no) + ": " +
                       first_differing_member(row.expected, actual[index]);
        lines[row.expected_at] = std::string{kExpectedMark} + actual[index];
    }
    // invariant: THE GUARD — a moved row is rewritten only after the version token was bumped, so
    // regeneration cannot be the silent repair of an output change that forgot its bump.
    ASSERT_TRUE(refused.empty())
        << "REFUSING TO REGENERATE: rows moved while kCanonicalizationVersion did not ("
        << insight::kCanonicalizationVersion << "):" << refused
        << "\n  Bump the token with its ledger entry first, or restore the change.";

    for (std::string& line : lines)
        if (line.starts_with(kVersionKey))
            line = std::string{kVersionKey} + std::string{insight::kCanonicalizationVersion};
    std::ofstream out{INSIGHT_CANON_PROJECTION_GOLDEN_PATH, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(out.is_open()) << "cannot write " << INSIGHT_CANON_PROJECTION_GOLDEN_PATH;
    for (const std::string& line : lines)
        out << line << '\n';
}
