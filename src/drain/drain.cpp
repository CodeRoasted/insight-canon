module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)
#include <cstring>

module insight.canon.detail.drain;
import insight.canon.internal;
import insight.canon.api;

// drain.cpp — Drain v2
//
// Online log-template miner (He et al., ICWS 2017) reimplemented for
// throughput. The public Drain class API is preserved 1:1; the internal
// representation switches to interned token-ids in a flat SoA store.
//
// ── Design summary ─────────────────────────────────────────────────────────
//
//  - **Token interning.** Whitespace-delimited tokens are hashed (FNV-1a 32-
//    bit) and looked up in an open-addressing intern table. Numeric tokens
//    and tokens matching the configured masks (IPv4, hex) collapse to a
//    single `kWildcardId`. Each token in a line is therefore a `uint32_t`.
//
//  - **SoA cluster storage.** `std::vector<Cluster>`; each Cluster is a 64-B
//    POD holding length, count, generation, template_id, an offset+length
//    into a flat `template_token_ids_` arena, and a 64-bit wildcard bitset.
//
//  - **Bucket index** replaces the recursive prefix tree.
//        key   = (token_count << 32) | first_significant_token_id
//        value = std::vector<cluster_index>
//    Average bucket size is small (1–4 in real workloads); we walk it
//    linearly with int-only compares.
//
//  - **Similarity** between line-ids and cluster-ids is a u32-equality reduce.
//    Only genuine token equality scores (a real token on a template `<*>` does
//    not), which bounds generalisation. Auto-vectorises into AVX2 for clusters
//    whose token count fits in registers.
//
//  - **No per-line heap allocation** on the fast path: tokenisation writes
//    into a reusable `std::vector<TokenID>`; cluster creation amortises into
//    the SoA vector's geometric growth; the intern table grows only on miss.
//
//  - **Zero RE2.** Token masking (IPv4, hex) uses constexpr hand-written
//    scanners; no regex library is needed anywhere in this file.

namespace insight::tokenization
{

namespace
{

    // ── Constants ────────────────────────────────────────────────────────────

    constexpr std::string_view kWildcard{"<*>"};

    using TokenID = std::uint32_t;
    constexpr TokenID kInvalidId{0xFFFFFFFFU};
    constexpr TokenID kWildcardId{0U};

    constexpr std::size_t kBitsetCapacity{64};

    // Sentinel value for the flat direct-index cluster map.
    constexpr std::uint32_t kInvalidClusterIdx{0xFFFF'FFFFU};

    constexpr unsigned kDecimalBase{10U};
    constexpr unsigned kAsciiCaseMask{32U}; // bit that toggles upper/lower case
    constexpr unsigned kHexLetterCount{6U}; // hex letters a-f / A-F
    constexpr std::uint32_t kFnv1aOffset{0x811C9DC5U};
    constexpr std::uint32_t kFnv1aPrime{0x01000193U};
    constexpr std::size_t kDefaultInitialCapacity{4096U};
    constexpr std::size_t kCacheLineSize{64U};
    constexpr unsigned kBucketKeyShift{32U}; // high 32 bits of BucketKey hold the token count
    constexpr std::size_t kArenaDefaultBytes{65536UL};

    // ── Hand-written token-mask predicates ───────────────────────────────────
    // Replaces RE2 patterns from DrainConfig.  Called only for novel tokens
    // (cache hit returns immediately without touching these).

    [[nodiscard]] constexpr bool is_hex_char(char chr) noexcept
    {
        return (static_cast<unsigned>(chr) - '0' < kDecimalBase) ||
               (static_cast<unsigned>(static_cast<unsigned char>(chr) | kAsciiCaseMask) - 'a' <
                kHexLetterCount);
    }

    // Consumes 1–3 decimal digits from str at pos; returns false if no digit found.
    [[nodiscard]] constexpr bool consume_ipv4_octet(std::string_view str, std::size_t& pos) noexcept
    {
        if (pos >= str.size() || static_cast<unsigned>(str[pos]) - '0' >= kDecimalBase)
            return false;
        ++pos;
        if (pos < str.size() && static_cast<unsigned>(str[pos]) - '0' < kDecimalBase)
            ++pos;
        if (pos < str.size() && static_cast<unsigned>(str[pos]) - '0' < kDecimalBase)
            ++pos;
        return true;
    }

    // IPv4: \[?\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}(:\d+)?\]?[,;:\.\]]?
    [[nodiscard]] constexpr bool is_ipv4_token(std::string_view str) noexcept
    {
        if (str.empty())
            return false;
        std::size_t pos{0};
        if (str[0] == '[')
            ++pos;
        for (int oct{0}; oct < 4; ++oct)
        {
            if (!consume_ipv4_octet(str, pos))
                return false;
            if (oct < 3)
            {
                if (pos >= str.size() || str[pos] != '.')
                    return false;
                ++pos;
            }
        }
        if (pos < str.size() && str[pos] == ':')
        {
            ++pos;
            while (pos < str.size() && static_cast<unsigned>(str[pos]) - '0' < kDecimalBase)
                ++pos;
        }
        if (pos < str.size() && str[pos] == ']')
            ++pos;
        if (pos < str.size() && (str[pos] == ',' || str[pos] == ';' || str[pos] == ':' ||
                                 str[pos] == '.' || str[pos] == ']'))
            ++pos;
        return pos == str.size();
    }

    // Hex: 0x[0-9a-fA-F]+[,;:\.\]]?
    [[nodiscard]] constexpr bool is_hex_token(std::string_view str) noexcept
    {
        if (str.size() < 3U || str[0] != '0' || str[1] != 'x')
            return false;
        std::size_t pos{2U};
        if (!is_hex_char(str[pos]))
            return false;
        while (pos < str.size() && is_hex_char(str[pos]))
            ++pos;
        if (pos < str.size() && (str[pos] == ',' || str[pos] == ';' || str[pos] == ':' ||
                                 str[pos] == '.' || str[pos] == ']'))
            ++pos;
        return pos == str.size();
    }

    // ── FNV-1a 32-bit hash ───────────────────────────────────────────────────
    [[nodiscard]] constexpr std::uint32_t fnv1a32(std::string_view str) noexcept
    {
        std::uint32_t hash{kFnv1aOffset};
        for (const char chr : str)
        {
            hash ^= static_cast<std::uint8_t>(chr);
            hash *= kFnv1aPrime;
        }
        return hash;
    }

    // ── Open-addressing intern table ─────────────────────────────────────────
    struct InternTable
    {
        struct Entry
        {
            std::uint32_t hash{0};
            TokenID id{kInvalidId};
            std::string_view text; // arena-stable
        };

        std::vector<Entry> table;
        std::size_t mask{0};
        std::size_t size{0};
        std::vector<std::string_view> id_to_text;

        void init(std::size_t initial_capacity = kDefaultInitialCapacity)
        {
            std::size_t cap{1};
            while (cap < initial_capacity)
                cap <<= 1U;
            table.assign(cap, Entry{});
            mask = cap - 1U;
            size = 0;
            id_to_text.clear();
            id_to_text.push_back(kWildcard); // id 0
        }

        [[nodiscard]] TokenID lookup(std::string_view str, std::uint32_t hash_val) const noexcept
        {
            std::size_t idx{hash_val & mask};
            while (true)
            {
                const Entry& entry{table[idx]};
                if (entry.id == kInvalidId)
                    return kInvalidId;
                if (entry.hash == hash_val && entry.text.size() == str.size() &&
                    std::memcmp(entry.text.data(), str.data(), str.size()) == 0)
                    return entry.id;
                idx = (idx + 1U) & mask;
            }
        }

        // Insert a novel user token (not wildcard). Auto-assigns the next id.
        TokenID insert(std::string_view arena_text, std::uint32_t hash_val)
        {
            if ((size + 1U) * 2U > table.size())
                rehash(table.size() * 2U);

            std::size_t idx{hash_val & mask};
            while (true)
            {
                Entry& entry{table[idx]};
                if (entry.id == kInvalidId)
                {
                    const TokenID tok_id{static_cast<TokenID>(id_to_text.size())};
                    entry = Entry{.hash = hash_val, .id = tok_id, .text = arena_text};
                    id_to_text.push_back(arena_text);
                    ++size;
                    return tok_id;
                }
                if (entry.hash == hash_val && entry.text.size() == arena_text.size() &&
                    std::memcmp(entry.text.data(), arena_text.data(), arena_text.size()) == 0)
                    return entry.id;
                idx = (idx + 1U) & mask;
            }
        }

        // Cache a masked token as kWildcardId so future lookups skip RE2.
        // Does NOT push to id_to_text (kWildcardId is already id_to_text[0]).
        void insert_masked(std::string_view arena_text, std::uint32_t hash_val)
        {
            if ((size + 1U) * 2U > table.size())
                rehash(table.size() * 2U);

            std::size_t idx{hash_val & mask};
            while (true)
            {
                Entry& entry{table[idx]};
                if (entry.id == kInvalidId)
                {
                    entry = Entry{.hash = hash_val, .id = kWildcardId, .text = arena_text};
                    ++size;
                    return;
                }
                idx = (idx + 1U) & mask;
            }
        }

        void rehash(std::size_t new_cap)
        {
            std::vector<Entry> old{std::move(table)};
            table.assign(new_cap, Entry{});
            mask = new_cap - 1U;
            for (const Entry& entry : old)
            {
                if (entry.id == kInvalidId)
                    continue;
                std::size_t idx{entry.hash & mask};
                while (table[idx].id != kInvalidId)
                    idx = (idx + 1U) & mask;
                table[idx] = entry;
            }
        }
    };

    // ── Cluster (cache-line POD) ─────────────────────────────────────────────
    struct alignas(kCacheLineSize) Cluster
    {
        std::uint64_t wildcards{0};
        std::uint64_t generation{0};
        std::uint64_t count{1};
        std::uint32_t token_count{0};
        std::uint32_t token_offset{0};
        TemplateID id{0};
        TokenID first_significant{0}; // bucket key partner with token_count
        std::uint32_t pad0{0};
        // Rendered template string stored in Drain's internal arena (16 bytes, align 8).
        // Offset 48, ends at 64 — replaces pad1 (4) + implicit padding (4) + pad2 (8).
        // Empty on new cluster or after absorb_into changes a wildcard.
        // On a steady-state hit, match_into_arena returns this directly
        // instead of re-rendering into the caller's arena every call.
        std::string_view cached_template; // offset 48..63
    };
    static_assert(sizeof(Cluster) == kCacheLineSize, "Cluster must fit in one cache line");

    // ── Bucket key ───────────────────────────────────────────────────────────
    using BucketKey = std::uint64_t;

    [[nodiscard]] constexpr BucketKey make_bucket_key(std::uint32_t length,
                                                      TokenID first_significant) noexcept
    {
        return (static_cast<BucketKey>(length) << kBucketKeyShift) |
               static_cast<BucketKey>(first_significant);
    }

    // ── Helpers ──────────────────────────────────────────────────────────────
    [[nodiscard]] inline bool is_all_digits(std::string_view str) noexcept
    {
        if (str.empty())
            return false;
        return std::ranges::all_of(str, [](const char chr)
                                   { return static_cast<unsigned>(chr) - '0' < kDecimalBase; });
    }

    [[nodiscard]] constexpr bool is_ascii_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < kDecimalBase;
    }

    // Value-aware KEEP of low-cardinality status integers.
    //
    // A bare integer is masked to `<*>` (all-digit rule), which collapses
    // `exit code 0` and `exit code 1` into one template — a green→red flip then
    // vanishes at the template level. To keep such values DISTINCT we KEEP an
    // integer literal when it immediately follows a status keyword AND is small
    // (≤ kMaxStatusDigits). Size-gating bounds cardinality (exit codes ≤ 255,
    // HTTP status ≤ 599 — both ≤ 3 digits); the keyword gate keeps bare counts
    // ("port 8080", "took 200 ms") masked. The lexicon is a seed and will grow
    // during calibration.
    constexpr std::size_t kMaxStatusDigits{3};

    [[nodiscard]] inline bool equals_ascii_lower(std::string_view tok,
                                                 std::string_view lower) noexcept
    {
        if (tok.size() != lower.size())
            return false;
        for (std::size_t pos{0}; pos < tok.size(); ++pos)
        {
            char chr{tok[pos]};
            if (chr >= 'A' && chr <= 'Z')
                chr = static_cast<char>(chr - 'A' + 'a');
            if (chr != lower[pos])
                return false;
        }
        return true;
    }

    [[nodiscard]] inline bool is_status_keyword(std::string_view tok) noexcept
    {
        return equals_ascii_lower(tok, "code") || equals_ascii_lower(tok, "status") ||
               equals_ascii_lower(tok, "exit") || equals_ascii_lower(tok, "signal");
    }

    // Composite-token masking, SOURCE_LOCATION class.
    //
    // Drain masks only WHOLE tokens that are all-digit / IPv4 / hex. A compiler
    // source location like `tokenizer.cpp:4500:30:` is one whitespace token that
    // is none of those, so it stays literal and every (file,line,col) becomes its
    // own template — the dominant cardinality blow-up (≈2160 singletons on the
    // multi-format mix; most of the 2373).
    //
    // Recognize `<path>:<digits>[:<digits>]…[:]` where the prefix before the first
    // `:<digits>` is path-like (contains '.' or '/'), and normalize by masking each
    // numeric `:<digits>` run to `:<*>` while KEEPING the path (the semantic part:
    // which file). So `tokenizer.cpp:4500:30:` and `tokenizer.cpp:12:5:` collapse
    // to one template `tokenizer.cpp:<*>:<*>:`, but a different file stays distinct.
    //
    // Pure and deterministic (a function of the token bytes only — I5). Returns
    // true and fills `out` with the normalized form; returns false (leaving `out`
    // untouched) when `tok` is not a source location. The path-like requirement
    // keeps clock times (`12:30:45`) and bare ratios out — only `:<digits>` runs
    // behind a path prefix are masked.
    [[nodiscard]] inline bool normalize_source_location(std::string_view tok, std::string& out)
    {
        // Locate the first ':' that is immediately followed by a digit.
        std::size_t first{std::string_view::npos};
        for (std::size_t pos{0}; pos + 1 < tok.size(); ++pos)
        {
            if (tok[pos] == ':' && is_ascii_digit(tok[pos + 1]))
            {
                first = pos;
                break;
            }
        }
        if (first == std::string_view::npos)
            return false;

        const std::string_view prefix{tok.substr(0, first)};
        if (prefix.empty())
            return false;
        const bool path_like{
            std::ranges::any_of(prefix, [](char chr) { return chr == '.' || chr == '/'; })};
        if (!path_like)
            return false;

        out.clear();
        out.append(prefix);
        std::size_t cursor{first};
        while (cursor < tok.size())
        {
            if (tok[cursor] == ':' && cursor + 1 < tok.size() && is_ascii_digit(tok[cursor + 1]))
            {
                out.append(":<*>");
                ++cursor; // consume ':'
                while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
                    ++cursor; // consume the digit run
            }
            else
            {
                out.push_back(tok[cursor]);
                ++cursor;
            }
        }
        return true;
    }

    // VERSIONED_REF composite: `<name>/<numeric-version>[trailing punct]`.
    // Conan/cmake/package output ("zlib/3", "boost/1.83.0:") keeps a literal token
    // per (name,version), so a bumped version is a phantom new template. Normalize
    // by KEEPing the name and masking the numeric version → `zlib/<*>`, `boost/<*>:`.
    // Requires: a '/' whose suffix is a numeric (digits/dots) version run, then only
    // punctuation to end (so paths like "src/foo.cpp" — alpha suffix — are NOT hit).
    [[nodiscard]] inline bool normalize_versioned_ref(std::string_view tok, std::string& out)
    {
        const std::size_t slash{tok.rfind('/')};
        if (slash == std::string_view::npos || slash + 1 >= tok.size())
            return false;
        if (!is_ascii_digit(tok[slash + 1]))
            return false; // version must start with a digit

        std::size_t cursor{slash + 1};
        bool saw_digit{false};
        while (cursor < tok.size() && (is_ascii_digit(tok[cursor]) || tok[cursor] == '.'))
        {
            saw_digit = saw_digit || is_ascii_digit(tok[cursor]);
            ++cursor;
        }
        if (!saw_digit)
            return false;
        // Anything after the version run must be non-alphanumeric (punctuation),
        // else this is a path segment ("v1/2x") not a terminal version.
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
        {
            const char chr{tok[pos]};
            if (is_ascii_digit(chr) || (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z'))
                return false;
        }

        out.clear();
        out.append(tok.substr(0, slash + 1)); // "zlib/"
        out.append("<*>");
        out.append(tok.substr(cursor)); // trailing punctuation, e.g. ":"
        return true;
    }

    // BRACKET_INDEX composite: `<word>[<digits>]<rest>`. Recursion depth and
    // worker indices ("make[2]:", "thread[15]") otherwise template per index.
    // Normalize the first bracketed digit run → `make[<*>]:`.
    [[nodiscard]] inline bool normalize_bracket_index(std::string_view tok, std::string& out)
    {
        const std::size_t open{tok.find('[')};
        if (open == std::string_view::npos || open + 1 >= tok.size())
            return false;
        std::size_t cursor{open + 1};
        bool saw_digit{false};
        while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
        {
            saw_digit = true;
            ++cursor;
        }
        if (!saw_digit || cursor >= tok.size() || tok[cursor] != ']')
            return false;

        out.clear();
        out.append(tok.substr(0, open + 1)); // "make["
        out.append("<*>");
        out.append(tok.substr(cursor)); // "]:" and anything after
        return true;
    }

    template <typename Cb>
        requires std::invocable<Cb, std::string_view>
    inline void for_each_token(std::string_view content, Cb callback)
    {
        const char* const base{content.data()};
        std::size_t cursor{0};
        const std::size_t len{content.size()};
        while (cursor < len)
        {
            while (cursor < len && base[cursor] == ' ')
                ++cursor;
            if (cursor >= len)
                break;
            const std::size_t start{cursor};
            // Scan to the next space with memchr (vectorised in libc) — token bodies
            // (paths, ids, JSON) are the long runs; bit-identical boundaries to the
            // byte loop. memchr is never asymptotically worse for the short tokens.
            const void* const space{std::memchr(base + cursor, ' ', len - cursor)};
            cursor = space != nullptr
                         ? static_cast<std::size_t>(static_cast<const char*>(space) - base)
                         : len;
            callback(content.substr(start, cursor - start)); // lvalue: invoked per token
        }
    }

    // Count positions where the line token EQUALS the template token. A template
    // wildcard (kWildcardId) credits a match ONLY when the line token is itself
    // variable there (also kWildcardId — a masked number/IP) — i.e. the equality
    // 0==0 holds. A real token landing on a `<*>` does NOT count.
    //
    // Crediting every wildcard unconditionally (the old `+= popcount`) let a
    // cluster that had accumulated a few wildcards match almost anything: with
    // K wildcards it scored K "free" hits, so a line sharing none of the FIXED
    // skeleton still cleared the threshold and got absorbed — runaway
    // generalisation that collapses unrelated families into one all-wildcard
    // mega-template ("INFO <*> <*> <*> <*> <*>"). Scoring only genuine token
    // equality forces a join to match the cluster's stable skeleton, keeping
    // distinct families (e.g. request vs retry lines) distinct.
    [[nodiscard]] inline std::size_t
    similarity_matches(std::span<const TokenID> line_ids,
                       std::span<const TokenID> cluster_ids) noexcept
    {
        const std::size_t len{line_ids.size()};
        std::size_t matches{0};
        // Auto-vectorisable: u32-equality reduce. Both-wildcard positions
        // (line and template each kWildcardId) match via 0==0; a real token on
        // a template wildcard does not, which is what bounds generalisation.
        for (std::size_t i{0}; i < len; ++i)
            matches += static_cast<std::size_t>(line_ids[i] == cluster_ids[i]);
        return matches;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Drain::Impl
// ─────────────────────────────────────────────────────────────────────────────

struct Drain::Impl
{
    DrainConfig config;

    std::unique_ptr<ArenaAllocator> arena{std::make_unique<ArenaAllocator>(kArenaDefaultBytes)};

    InternTable intern;
    std::vector<TokenID> template_token_ids;
    std::vector<Cluster> clusters;
    std::unordered_map<BucketKey, std::vector<std::uint32_t>> buckets;

    // Flat direct-index cluster lookup: id_by_tid[template_id] = cluster_index.
    // Template IDs are sequential from 0, so this is a plain vector indexed by
    // TemplateID — O(1) with zero hash overhead on the hot match path.
    // Entries are kInvalidClusterIdx when the slot is empty (pruned).
    std::vector<std::uint32_t> id_by_tid;
    std::size_t cluster_count_{0};

    TemplateID next_id{1};
    std::uint64_t total_matched{0};
    std::uint64_t generation{0};

    // Reusable per-line scratch.
    std::vector<TokenID> line_ids;
    std::vector<std::string_view> line_raw_tokens;
    // Reusable scratch for composite-token normalization. Capacity is
    // retained across calls, so a matched composite costs no per-line heap alloc
    // in steady state; non-composite tokens never touch it.
    std::string composite_scratch;

    // Identity (KEEP) token ids. identity_id[id] is true for a status value
    // kept distinct (exit code / status / signal). A difference at an identity
    // position disqualifies a cluster (forces a new template) so a green→red flip
    // does not get merged + re-wildcarded. `any_identity` gates the matcher check
    // so logs without status values pay zero cost.
    std::vector<std::uint8_t> identity_id;
    bool any_identity{false};

    void mark_identity(TokenID tok_id)
    {
        if (tok_id >= identity_id.size())
            identity_id.resize(static_cast<std::size_t>(tok_id) + 1U, 0U);
        identity_id[tok_id] = 1U;
        any_identity = true;
    }

    [[nodiscard]] bool is_identity(TokenID tok_id) const noexcept
    {
        return tok_id < identity_id.size() && identity_id[tok_id] != 0U;
    }

    // True if line and candidate disagree at any IDENTITY position — i.e. a KEEP
    // status value differs. Such a cluster is not the same template.
    [[nodiscard]] bool identity_mismatch(std::span<const TokenID> line,
                                         std::span<const TokenID> cand) const noexcept
    {
        const std::size_t len{line.size()};
        for (std::size_t pos{0}; pos < len; ++pos)
            if (line[pos] != cand[pos] && (is_identity(line[pos]) || is_identity(cand[pos])))
                return true;
        return false;
    }

    void reset_state()
    {
        arena = std::make_unique<ArenaAllocator>(kArenaDefaultBytes);
        intern.init();
        template_token_ids.clear();
        clusters.clear();
        buckets.clear();
        id_by_tid.clear();
        cluster_count_ = 0;
        next_id = 1;
        total_matched = 0;
        generation = 0;
        identity_id.clear();
        any_identity = false;
    }

    [[nodiscard]] bool is_masked(std::string_view tok) const noexcept
    {
        if (config.mask_ip_addresses && is_ipv4_token(tok))
            return true;
        if (config.mask_hex_addresses && is_hex_token(tok))
            return true;
        return false;
    }

    // Lookup-or-insert a token as a LITERAL (never masked). Used by the
    // status-value KEEP path, which must bypass the all-digit mask.
    [[nodiscard]] TokenID intern_literal(std::string_view tok)
    {
        const std::uint32_t hash_val{fnv1a32(tok)};
        const TokenID existing{intern.lookup(tok, hash_val)};
        if (existing != kInvalidId) [[likely]]
            return existing;
        return intern.insert(arena->store_string(tok), hash_val);
    }

    [[nodiscard]] TokenID intern_token(std::string_view tok, std::string_view prev)
    {
        // Classify the token's digit-ness ONCE (the predicate is used by both the
        // KEEP gate and the all-digit mask).
        const bool all_digits{is_all_digits(tok)};

        // KEEP a low-cardinality status value distinct (exit code / status /
        // signal). Context- and size-gated, so bare numbers elsewhere still mask
        // and cardinality stays bounded. This is what makes a green→red flip
        // (exit 0→1) two templates instead of one collapsed `exit code <*>`.
        if (all_digits && tok.size() <= kMaxStatusDigits && is_status_keyword(prev))
        {
            const TokenID kept{intern_literal(tok)};
            mark_identity(kept);
            return kept;
        }

        if (tok.empty() || all_digits)
            return kWildcardId;

        // Normalize a composite token (source location / versioned ref /
        // bracket index) before interning, so noisy variants share a template.
        // Single cheap pre-gate first: every composite needs a ':' '/' or '[', so
        // a token with none (the common case) skips all three recognizers — one
        // scan instead of three. Matching tokens build the scratch; the interned
        // identity is the NORMALIZED form, so raw variants are never cached
        // individually (a matched composite re-normalizes per occurrence).
        std::string_view key{tok};
        const bool maybe_composite{std::ranges::any_of(
            tok, [](char chr) { return chr == ':' || chr == '/' || chr == '['; })};
        if (maybe_composite && (normalize_source_location(tok, composite_scratch) ||
                                normalize_versioned_ref(tok, composite_scratch) ||
                                normalize_bracket_index(tok, composite_scratch)))
            key = composite_scratch;

        // Hash lookup FIRST: steady-state hot path has zero RE2 calls.
        // kWildcardId (0) is a valid cached result for previously-masked tokens.
        const std::uint32_t hash_val{fnv1a32(key)};
        const TokenID existing{intern.lookup(key, hash_val)};
        if (existing != kInvalidId) [[likely]]
            return existing; // includes cached kWildcardId for masked tokens

        // Novel token: run masks once and cache result.
        const std::string_view stable{arena->store_string(key)};
        if (is_masked(key))
        {
            intern.insert_masked(stable, hash_val);
            return kWildcardId;
        }
        return intern.insert(stable, hash_val);
    }

    void tokenise(std::string_view content)
    {
        line_ids.clear();
        line_raw_tokens.clear();
        std::string_view prev{}; // raw previous token — context for status KEEP
        for_each_token(content,
                       [&](std::string_view tok)
                       {
                           line_raw_tokens.push_back(tok);
                           line_ids.push_back(intern_token(tok, prev));
                           prev = tok;
                       });
    }

    [[nodiscard]] static TokenID first_significant(const std::vector<TokenID>& ids) noexcept
    {
        for (const TokenID tok_id : ids)
            if (tok_id != kWildcardId)
                return tok_id;
        return kWildcardId;
    }

    // Returns cluster index; sets out_new_cluster.
    std::uint32_t match_or_insert(bool& out_new_cluster)
    {
        const std::uint32_t length{static_cast<std::uint32_t>(line_ids.size())};
        const TokenID first_sig{first_significant(line_ids)};
        const BucketKey key{make_bucket_key(length, first_sig)};

        auto& bucket{buckets[key]};

        std::uint32_t best_idx{kInvalidId};
        std::size_t best_matches{0};
        const std::size_t threshold_matches{
            static_cast<std::size_t>(config.similarity_threshold * static_cast<double>(length))};

        for (const std::uint32_t cand_idx : bucket)
        {
            const Cluster& cand{clusters[cand_idx]};
            const auto cand_span{
                std::span<const TokenID>(template_token_ids).subspan(cand.token_offset, length)};
            // A cluster whose KEEP/identity value differs is a different
            // template — skip it so the line forms its own cluster. Gated by
            // any_identity, so streams without status values are unaffected.
            if (any_identity && identity_mismatch(std::span<const TokenID>(line_ids), cand_span))
                continue;
            const std::size_t match_count{
                similarity_matches(std::span<const TokenID>(line_ids), cand_span)};
            if (match_count > best_matches)
            {
                best_matches = match_count;
                best_idx = cand_idx;
            }
        }

        if (best_idx != kInvalidId && best_matches >= threshold_matches)
        {
            absorb_into(best_idx, length);
            out_new_cluster = false;
            return best_idx;
        }

        if (clusters.size() >= config.max_clusters)
        {
            INSIGHT_LOG_WARN(logging::drain_logger(),
                             "cluster cap reached ({}); absorbing into best match",
                             config.max_clusters);
            if (best_idx != kInvalidId)
            {
                absorb_into(best_idx, length);
                out_new_cluster = false;
                return best_idx;
            }
        }

        // Create new cluster.
        Cluster new_cluster_obj{};
        new_cluster_obj.id = next_id++;
        new_cluster_obj.token_count = length;
        new_cluster_obj.token_offset = static_cast<std::uint32_t>(template_token_ids.size());
        new_cluster_obj.count = 1;
        new_cluster_obj.generation = generation;
        new_cluster_obj.first_significant = first_sig;
        template_token_ids.insert(template_token_ids.end(), line_ids.begin(), line_ids.end());

        const std::size_t cap{std::min<std::size_t>(length, kBitsetCapacity)};
        for (std::size_t i{0}; i < cap; ++i)
            if (line_ids[i] == kWildcardId)
                new_cluster_obj.wildcards |= (std::uint64_t{1} << i);

        const std::uint32_t idx{static_cast<std::uint32_t>(clusters.size())};
        clusters.push_back(new_cluster_obj);
        bucket.push_back(idx);
        // Register in flat direct-index map.
        const auto tid{static_cast<std::size_t>(new_cluster_obj.id)};
        if (tid >= id_by_tid.size())
            id_by_tid.resize(tid + 1U, kInvalidClusterIdx);
        id_by_tid[tid] = idx;
        ++cluster_count_;
        out_new_cluster = true;
        return idx;
    }

    // Merge the current line into cluster[idx]: any non-wildcard position
    // that disagrees becomes a wildcard.
    void absorb_into(std::uint32_t idx, std::uint32_t length) noexcept
    {
        Cluster& cand{clusters[idx]};
        const auto cand_ids{
            std::span<TokenID>(template_token_ids).subspan(cand.token_offset, length)};
        const std::size_t cap{std::min<std::size_t>(length, kBitsetCapacity)};
        for (std::size_t i{0}; i < cap; ++i)
        {
            const std::uint64_t bit{std::uint64_t{1} << i};
            if ((cand.wildcards & bit) != 0U)
                continue;
            if (cand_ids[i] != line_ids[i])
            {
                cand.wildcards |= bit;
                cand_ids[i] = kWildcardId;
                cand.cached_template = {}; // template changed — invalidate render cache
            }
        }
        for (std::size_t i{cap}; i < length; ++i)
        {
            if (cand_ids[i] != kWildcardId && cand_ids[i] != line_ids[i])
            {
                cand_ids[i] = kWildcardId;
                cand.cached_template = {}; // template changed — invalidate render cache
            }
        }
        ++cand.count;
        cand.generation = generation;
    }

    [[nodiscard]] std::string render_template(std::uint32_t cluster_idx) const
    {
        const Cluster& cluster_ref{clusters[cluster_idx]};
        const auto ids{std::span<const TokenID>(template_token_ids)
                           .subspan(cluster_ref.token_offset, cluster_ref.token_count)};
        if (cluster_ref.token_count == 0)
            return {};

        std::size_t total{cluster_ref.token_count - 1U};
        for (std::uint32_t i{0}; i < cluster_ref.token_count; ++i)
        {
            const TokenID tok_id{ids[i]};
            const std::string_view text{(tok_id == kWildcardId) ? kWildcard
                                                                : intern.id_to_text[tok_id]};
            total += text.size();
        }
        std::string out;
        out.reserve(total);
        for (std::uint32_t i{0}; i < cluster_ref.token_count; ++i)
        {
            const TokenID tok_id{ids[i]};
            const std::string_view text{(tok_id == kWildcardId) ? kWildcard
                                                                : intern.id_to_text[tok_id]};
            if (i != 0)
                out.push_back(' ');
            out.append(text);
        }
        return out;
    }

    [[nodiscard]] std::string_view render_template_into(std::uint32_t cluster_idx,
                                                        ArenaAllocator& out_arena) const
    {
        const Cluster& cluster_ref{clusters[cluster_idx]};
        const auto ids{std::span<const TokenID>(template_token_ids)
                           .subspan(cluster_ref.token_offset, cluster_ref.token_count)};
        if (cluster_ref.token_count == 0)
            return {};

        std::size_t total{cluster_ref.token_count - 1U};
        for (std::uint32_t i{0}; i < cluster_ref.token_count; ++i)
        {
            const TokenID tok_id{ids[i]};
            const std::string_view text{(tok_id == kWildcardId) ? kWildcard
                                                                : intern.id_to_text[tok_id]};
            total += text.size();
        }

        char* const buf_raw{static_cast<char*>(out_arena.allocate(total, 1))};
        const auto buf{std::span<char>{buf_raw, total}};
        std::size_t off{0};
        for (std::uint32_t i{0}; i < cluster_ref.token_count; ++i)
        {
            if (i != 0)
            {
                buf[off] = ' ';
                ++off;
            }
            const TokenID tok_id{ids[i]};
            const std::string_view text{(tok_id == kWildcardId) ? kWildcard
                                                                : intern.id_to_text[tok_id]};
            std::memcpy(&buf[off], text.data(), text.size());
            off += text.size();
        }
        return {buf.data(), total};
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Drain public interface
// ─────────────────────────────────────────────────────────────────────────────

Drain::ArenaMatchResult Drain::match_into_arena(std::string_view content, ArenaAllocator& out_arena,
                                                Drain::TemplateRender render)
{
    Impl& impl{*impl_};
    ++impl.generation;
    ++impl.total_matched;

    if (content.empty())
    {
        const auto tid0{static_cast<std::size_t>(0U)};
        const std::uint32_t existing{tid0 < impl.id_by_tid.size() ? impl.id_by_tid[tid0]
                                                                  : kInvalidClusterIdx};
        if (existing == kInvalidClusterIdx)
        {
            Cluster empty_cluster{};
            empty_cluster.id = 0;
            empty_cluster.count = 1;
            empty_cluster.generation = impl.generation;
            const std::uint32_t idx{static_cast<std::uint32_t>(impl.clusters.size())};
            impl.clusters.push_back(empty_cluster);
            if (impl.id_by_tid.empty())
                impl.id_by_tid.resize(1U, kInvalidClusterIdx);
            impl.id_by_tid[0] = idx;
            ++impl.cluster_count_;
            return {.template_id = 0, .new_cluster = true};
        }
        ++impl.clusters[existing].count;
        return {.template_id = 0, .new_cluster = false};
    }

    impl.tokenise(content);
    bool new_cluster{false};
    const std::uint32_t idx{impl.match_or_insert(new_cluster)};
    const Cluster& cluster_ref{impl.clusters[idx]};

    std::string_view tmpl_view{};
    if (render == TemplateRender::Eager)
    {
        if (!cluster_ref.cached_template.empty())
        {
            // Steady-state hit: return the cached render from Drain's arena.
            // No allocation or memcpy into out_arena needed.
            tmpl_view = cluster_ref.cached_template;
        }
        else
        {
            // First render or after a wildcard was introduced: render once
            // into Drain's internal arena and cache for subsequent calls.
            tmpl_view = impl.render_template_into(idx, *impl.arena);
            impl.clusters[idx].cached_template = tmpl_view;
        }
    }

    const auto tmpl_ids{std::span<const TokenID>(impl.template_token_ids)
                            .subspan(cluster_ref.token_offset, cluster_ref.token_count)};
    std::uint32_t param_count{0};
    for (std::uint32_t i{0}; i < cluster_ref.token_count; ++i)
        if (tmpl_ids[i] == kWildcardId)
            ++param_count;

    std::span<const std::string_view> params_span{};
    if (param_count > 0)
    {
        auto* params_buf_raw{static_cast<std::string_view*>(
            out_arena.allocate(param_count * sizeof(std::string_view), alignof(std::string_view)))};
        const auto params_buf{std::span<std::string_view>{params_buf_raw, param_count}};
        std::uint32_t param_idx{0};
        for (std::uint32_t i{0}; i < cluster_ref.token_count; ++i)
            if (tmpl_ids[i] == kWildcardId)
                params_buf[param_idx++] = impl.line_raw_tokens[i];
        params_span = params_buf;
    }

    return {.template_id = cluster_ref.id,
            .template_str = tmpl_view,
            .params = params_span,
            .new_cluster = new_cluster};
}

Drain::Drain(DrainConfig config) : impl_(std::make_unique<Impl>())
{
    impl_->config = config;
    impl_->intern.init();
    INSIGHT_LOG_INFO(logging::drain_logger(),
                     "drain init: max_depth={} sim_threshold={:.2f} max_clusters={} "
                     "mask_ip={} mask_hex={}",
                     impl_->config.max_depth, impl_->config.similarity_threshold,
                     impl_->config.max_clusters, impl_->config.mask_ip_addresses,
                     impl_->config.mask_hex_addresses);
}

Drain::~Drain() = default;
Drain::Drain(Drain&&) noexcept = default;
Drain& Drain::operator=(Drain&&) noexcept = default;

std::optional<std::string> Drain::get_template(TemplateID tmpl_id) const
{
    const auto tid{static_cast<std::size_t>(tmpl_id)};
    if (tid >= impl_->id_by_tid.size() || impl_->id_by_tid[tid] == kInvalidClusterIdx)
        return std::nullopt;
    return impl_->render_template(impl_->id_by_tid[tid]);
}

std::size_t Drain::cluster_count() const noexcept
{
    return impl_->cluster_count_;
}

std::size_t Drain::total_matched() const noexcept
{
    return impl_->total_matched;
}

void Drain::prune(std::size_t max_clusters)
{
    if (impl_->cluster_count_ <= max_clusters)
        return;

    const auto before{impl_->cluster_count_};
    INSIGHT_LOG_DEBUG(logging::drain_logger(), "prune start: clusters={} target={}", before,
                      max_clusters);

    std::vector<std::uint32_t> order;
    order.reserve(impl_->clusters.size());
    for (std::uint32_t cluster_idx{0}; cluster_idx < impl_->clusters.size(); ++cluster_idx)
        order.push_back(cluster_idx);
    std::ranges::sort(order, {}, [&](std::uint32_t cluster_idx)
                      { return impl_->clusters[cluster_idx].generation; });

    const std::size_t to_remove{order.size() - max_clusters};
    std::vector<bool> keep(impl_->clusters.size(), true);
    for (std::size_t rm_idx{0}; rm_idx < to_remove; ++rm_idx)
        keep[order[rm_idx]] = false;

    std::vector<Cluster> new_clusters;
    std::vector<TokenID> new_token_ids;
    std::unordered_map<BucketKey, std::vector<std::uint32_t>> new_buckets;
    std::vector<std::uint32_t> new_id_by_tid;
    std::size_t new_cluster_count{0};

    new_clusters.reserve(max_clusters);
    new_token_ids.reserve(impl_->template_token_ids.size());

    for (std::uint32_t cluster_idx{0}; cluster_idx < impl_->clusters.size(); ++cluster_idx)
    {
        if (!keep[cluster_idx])
            continue;
        Cluster cluster_copy{impl_->clusters[cluster_idx]};
        const std::uint32_t old_off{cluster_copy.token_offset};
        cluster_copy.token_offset = static_cast<std::uint32_t>(new_token_ids.size());
        new_token_ids.insert(new_token_ids.end(), impl_->template_token_ids.begin() + old_off,
                             impl_->template_token_ids.begin() + old_off +
                                 cluster_copy.token_count);
        const std::uint32_t new_idx{static_cast<std::uint32_t>(new_clusters.size())};
        const BucketKey key{
            make_bucket_key(cluster_copy.token_count, cluster_copy.first_significant)};
        new_buckets[key].push_back(new_idx);
        const auto tid{static_cast<std::size_t>(cluster_copy.id)};
        if (tid >= new_id_by_tid.size())
            new_id_by_tid.resize(tid + 1U, kInvalidClusterIdx);
        new_id_by_tid[tid] = new_idx;
        ++new_cluster_count;
        new_clusters.push_back(cluster_copy);
    }

    impl_->clusters = std::move(new_clusters);
    impl_->template_token_ids = std::move(new_token_ids);
    impl_->buckets = std::move(new_buckets);
    impl_->id_by_tid = std::move(new_id_by_tid);
    impl_->cluster_count_ = new_cluster_count;

    INSIGHT_LOG_INFO(logging::drain_logger(), "prune done: removed={} remaining={}",
                     before - impl_->cluster_count_, impl_->cluster_count_);
}

void Drain::reset()
{
    INSIGHT_LOG_INFO(logging::drain_logger(), "drain reset: clusters={}", impl_->cluster_count_);
    DrainConfig saved{impl_->config};
    impl_->reset_state();
    impl_->config = saved;
}

std::map<TemplateID, std::string> Drain::all_templates() const
{
    std::map<TemplateID, std::string> out;
    for (std::size_t tid{0}; tid < impl_->id_by_tid.size(); ++tid)
    {
        const std::uint32_t cluster_idx{impl_->id_by_tid[tid]};
        if (cluster_idx == kInvalidClusterIdx)
            continue;
        out.emplace(static_cast<TemplateID>(tid), impl_->render_template(cluster_idx));
    }
    return out;
}

// ── Stateless per-line template masker (D-TID-1/2) ──────────────────────────────
// Per token, EMIT its canonical form by its OWN class — the same precedence Drain
// applies in intern_token, but DECIDED per token (no absorb_into discovery, no
// cluster lookup): status-value KEEP → the literal; bare-number / empty / IPv4 / hex
// → "<*>" (a param); source-location / versioned-ref / bracket-index → the
// normalized literal (KEPT, NOT a param — it carries its own embedded "<*>"); any
// other token → the literal. The joined sequence is the template; the SHA-256 of it
// (computed downstream, unchanged) is the run-independent template_id. Pure: a
// function of `content`'s bytes only — no float, no map iteration, no state — so it is
// cross-stdlib bit-identical and order-/stream-independent by construction (D-TID-9).
StatelessTemplate stateless_template(std::string_view content, ArenaAllocator& out_arena,
                                     const DrainConfig& config)
{
    std::string tmpl;
    tmpl.reserve(content.size() + kWildcard.size());
    std::vector<std::string_view> params; // raw tokens at fully-masked positions
    std::string composite;                // scratch for composite normalization
    std::string_view prev{};              // raw previous token — context for status KEEP
    bool first{true};

    for_each_token(content,
                   [&](std::string_view tok)
                   {
                       if (!first)
                           tmpl.push_back(' ');
                       first = false;
                       const bool all_digits{is_all_digits(tok)};

                       // 1. status-value KEEP (identity): "exit code 0" stays distinct
                       //    from "exit code 1" — a green→red flip must not collapse.
                       if (all_digits && tok.size() <= kMaxStatusDigits && is_status_keyword(prev))
                       {
                           tmpl.append(tok);
                           prev = tok;
                           return;
                       }
                       // 2. bare number / empty → mask (a param).
                       if (tok.empty() || all_digits)
                       {
                           tmpl.append(kWildcard);
                           params.push_back(tok);
                           prev = tok;
                           return;
                       }
                       // 3. composite normalization → the normalized literal (KEPT).
                       const bool maybe_composite{std::ranges::any_of(
                           tok, [](char chr) { return chr == ':' || chr == '/' || chr == '['; })};
                       if (maybe_composite && (normalize_source_location(tok, composite) ||
                                               normalize_versioned_ref(tok, composite) ||
                                               normalize_bracket_index(tok, composite)))
                       {
                           tmpl.append(composite);
                           prev = tok;
                           return;
                       }
                       // 4. IPv4 / hex → mask (a param).
                       if ((config.mask_ip_addresses && is_ipv4_token(tok)) ||
                           (config.mask_hex_addresses && is_hex_token(tok)))
                       {
                           tmpl.append(kWildcard);
                           params.push_back(tok);
                           prev = tok;
                           return;
                       }
                       // 5. literal KEEP.
                       tmpl.append(tok);
                       prev = tok;
                   });

    const std::string_view tmpl_view{out_arena.store_string(tmpl)};
    std::span<const std::string_view> params_span{};
    if (!params.empty())
    {
        auto* buf{static_cast<std::string_view*>(
            out_arena.allocate(params.size() * sizeof(std::string_view), alignof(std::string_view)))};
        const auto buf_span{std::span<std::string_view>{buf, params.size()}};
        std::ranges::copy(params, buf_span.begin());
        params_span = buf_span;
    }
    return {.template_str = tmpl_view, .params = params_span};
}

} // namespace insight::tokenization
