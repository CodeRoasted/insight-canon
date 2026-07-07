module insight.canon.api;
import insight.canon.internal;

// location_recognizer.cpp — the intent registry's SECOND rule class (intent_identity_model.md §5.3/
// §5.4, II-8): extract the test-file WHERE from a line. The sub-quantum coverage coordinate the
// where_set_shift verdict compares — which test files a step reported this run (P1 coverage-loss /
// P6 reshape live in this SET's membership, not in template frequency). The G2-spike stand-in
// (a throwaway Sift-side regex) graduates HERE, into the canon recognizer layer (no surface-local
// locator survives — II-8). Universal test-file families; framework file-naming is CI-dialect-
// independent. Deterministic, ASCII-safe, no regex, no cross-line state.

namespace insight
{
namespace
{
[[nodiscard]] constexpr bool is_space(char chr) noexcept { return chr == ' ' || chr == '\t'; }
[[nodiscard]] constexpr bool is_lower(char chr) noexcept { return chr >= 'a' && chr <= 'z'; }
[[nodiscard]] constexpr bool is_word(char chr) noexcept
{
    return is_lower(chr) || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') || chr == '_';
}

// A noexcept slice (std::string_view::substr may throw; our positions are in-bounds by
// construction, so a manual view carries the same bytes without the throw path).
[[nodiscard]] constexpr std::string_view slice(std::string_view str, std::size_t pos,
                                               std::size_t count) noexcept
{
    return std::string_view{str.data() + pos, count};
}

// The JS/TS/py extensions a `.test.`/`.spec.` infix may carry (jest/vitest/playwright/pytest).
[[nodiscard]] bool is_script_ext(std::string_view ext) noexcept
{
    constexpr std::array<std::string_view, 7> known{"ts", "tsx", "js", "jsx", "mjs", "cjs", "py"};
    return std::ranges::any_of(known, [ext](std::string_view kind) noexcept { return ext == kind; });
}

// If `tok` contains a recognized test-file path, return its end offset (one past the extension);
// npos otherwise. A path token has no spaces (whitespace-split upstream); trailing `:line`,
// `::node`, `)` etc. sit AFTER the extension and are excluded by construction.
[[nodiscard]] std::size_t test_file_end(std::string_view tok) noexcept
{
    // 1. jest/vitest/playwright/pytest: `.test.<ext>` or `.spec.<ext>`.
    for (const std::string_view infix : {std::string_view{".test."}, std::string_view{".spec."}})
    {
        const std::size_t pos{tok.find(infix)};
        if (pos == std::string_view::npos)
            continue;
        const std::size_t ext_start{pos + infix.size()};
        std::size_t ext_end{ext_start};
        while (ext_end < tok.size() && is_lower(tok[ext_end]))
            ++ext_end;
        if (is_script_ext(slice(tok, ext_start, ext_end - ext_start)))
            return ext_end;
    }
    // 2. pytest bare module: `test_*.py` / `*_test.py` (basename convention, no `.test.` infix).
    if (const std::size_t py_pos{tok.find(".py")};
        py_pos != std::string_view::npos && (py_pos + 3 == tok.size() || !is_word(tok[py_pos + 3])))
    {
        const std::size_t slash{tok.rfind('/', py_pos)};
        const std::size_t base_start{slash == std::string_view::npos ? 0 : slash + 1};
        const std::string_view base{slice(tok, base_start, py_pos - base_start)};
        if (base.starts_with("test_") || base.ends_with("_test"))
            return py_pos + 3;
    }
    // 3. go / ruby: `*_test.go`, `*_spec.rb`, `*_test.rb`.
    for (const std::string_view suffix : {std::string_view{"_test.go"}, std::string_view{"_spec.rb"},
                                          std::string_view{"_test.rb"}})
    {
        const std::size_t pos{tok.find(suffix)};
        if (pos != std::string_view::npos)
        {
            const std::size_t end{pos + suffix.size()};
            if (end == tok.size() || !is_word(tok[end]))
                return end;
        }
    }
    return std::string_view::npos;
}
} // namespace

std::string_view recognize_location(std::string_view content) noexcept
{
    // Whitespace-split scan (a file path carries no spaces); the FIRST token bearing a recognized
    // test-file suffix is the WHERE. A leading glyph/verdict (`✓`, `PASS`, ANSI residue) sits in a
    // SEPARATE whitespace token, so it never contaminates the path.
    std::size_t cursor{0};
    const std::size_t len{content.size()};
    while (cursor < len)
    {
        while (cursor < len && is_space(content[cursor]))
            ++cursor;
        if (cursor >= len)
            break;
        const std::size_t start{cursor};
        while (cursor < len && !is_space(content[cursor]))
            ++cursor;
        const std::string_view tok{slice(content, start, cursor - start)};
        if (const std::size_t end{test_file_end(tok)}; end != std::string_view::npos)
            return slice(tok, 0, end);
    }
    return {};
}

} // namespace insight
