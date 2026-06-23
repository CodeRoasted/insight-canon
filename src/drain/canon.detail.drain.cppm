// insight.canon.detail.drain — SEALED template-mining domain (1.5.2 domain decomposition,
// §11.9.11). The Drain online log-template miner: content → (TemplateID, arena-stable template +
// params). A leaf over the contract: imports api only (DrainConfig, TemplateID, ArenaAllocator) —
// independent of scan/strategy/parse. Never re-exported by the facade and never installed
// (PRIVATE file set).
export module insight.canon.detail.drain;
import insight.canon.internal; // std + global C types
import insight.canon.api;      // DrainConfig, TemplateID, ArenaAllocator

// ──────── from src/insight/tokenization/drain.hpp ────────
export namespace insight::tokenization
{

// (ArenaAllocator forward-decl dropped — imported from insight.canon.api; redeclaring it in this
//  module's purview conflicts with the import under C++20 modules.)
class Drain
{
  public:
    explicit Drain(DrainConfig config = {});
    ~Drain();

    Drain(const Drain&) = delete;
    Drain& operator=(const Drain&) = delete;
    Drain(Drain&&) noexcept;
    Drain& operator=(Drain&&) noexcept;

    // Hot-path variant: returns arena-stable string_views, zero heap
    // allocations on the line path. Both `template_str` and `params` are
    // valid until `out_arena.reset()` (or destruction).
    struct ArenaMatchResult
    {
        TemplateID template_id{};
        std::string_view template_str; // empty when render == TemplateRender::Skip
        std::span<const std::string_view> params;
        bool new_cluster{false};
    };

    // Controls whether template_str is built in match_into_arena.
    // Most callers routing on template_id alone can pass Skip to save ~5-10 ns.
    enum class TemplateRender : std::uint8_t
    {
        Eager,
        Skip
    };

    [[nodiscard]] ArenaMatchResult match_into_arena(std::string_view content,
                                                    ArenaAllocator& out_arena,
                                                    TemplateRender render = TemplateRender::Eager);

    // Lookup
    [[nodiscard]] std::optional<std::string> get_template(TemplateID tmpl_id) const;
    [[nodiscard]] std::size_t cluster_count() const noexcept;
    [[nodiscard]] std::size_t total_matched() const noexcept;

    // Maintenance
    void prune(std::size_t max_clusters);
    void reset();

    // Dump all templates (for debugging/serialization)
    [[nodiscard]] std::map<TemplateID, std::string> all_templates() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ── Stateless per-line template masker (stateless_template_id.md D-TID-1/2) ──────
// A deterministic, run-independent function of the line's OWN masked/kept tokens —
// NO cluster state, NO cross-line learning. The same logical line yields the same
// template_str (hence the same SHA-256 template_id) in any run, any order, inside any
// surrounding stream: the phantom pair (a shared line two drains template differently)
// cannot form. The per-token KEEP / MASK / composite-normalize classification is the
// SAME one Drain applies in intern_token (status-value KEEP, bare-number / IPv4 / hex
// MASK, source-location / versioned-ref / bracket-index normalization); what is
// dropped is absorb_into's cross-line wildcard *discovery* (D-TID-2: discover→decide).
// This is the identity source that retires the clustering tree (D-TID-3).
//
// Result is arena-stable until out_arena.reset() (or destruction); `params` are the
// raw tokens at fully-masked (<*>) positions, as views into `content` (which the
// caller must keep stable for the params' lifetime).
struct StatelessTemplate
{
    std::string_view template_str;
    std::span<const std::string_view> params;
};

[[nodiscard]] StatelessTemplate stateless_template(std::string_view content,
                                                   ArenaAllocator& out_arena,
                                                   const DrainConfig& config);

} // namespace insight::tokenization
