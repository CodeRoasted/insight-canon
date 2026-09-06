// invariant: this INTERFACE imports the api only - the implementation adds the scan shard's
// char-class predicates - and neither is re-exported by the facade or installed.
// refs: ADR-3.D4, ADR-16.D5
export module insight.canon.detail.mask;
import insight.canon.internal;
import insight.canon.api;
export namespace insight::tokenization
{

// post: the masked template plus the raw tokens at fully-masked positions, both arena-stable until
// the arena is reset or destroyed.
// pre: the caller keeps `content` alive for the params' lifetime - they are views into it.
// invariant: a pure function of the line's OWN tokens - no cluster state and no cross-line
// learning, so the same logical line templates identically in any run and any order.
// refs: ADR-16.D5, SRC-D-TID-1, SRC-D-TID-2, SRC-D-TID-3
// note: the per-token KEEP, MASK and normalize classification is DECIDED, never discovered.
/**************************************************************************************************
D-LSRC-11 — the DIAGNOSTIC_COMPOSITE class, which subsumes the source-location rule
A token is split on `:` and `/` and every segment is classified on its own: a digit-leading segment
masks to the wildcard because it is the instance, a letter-leading segment is KEPT because it is the
stable class anchor - a filename, a level, a subsystem - and the original separators are rejoined in
place. Two gates keep the blast radius on genuine diagnostic composites. TRIGGER: the token must
carry a `:` immediately followed by a digit, so a plain numeric path is never claimed and stays with
the whole-token digit rule. ANCHOR: at least one letter-leading segment must exist, so a pure
numeric colon token such as a clock has no anchor and falls through to the whole-token digit mask
unchanged. One carve-out survives per segment: a digit segment that is a status value, short and
immediately preceded WITHIN the composite by a status keyword, is KEPT, so an exit code or an HTTP
status flipping from green to red never collapses into one template. This rule SUBSUMES the older
source-location rule exactly rather than sitting beside it - one general segment rule in place of a
family of shape-specific ones - and it is what collapses the Chromium and glog prefix, which the
source-location rule left whole because a PID and a date are not path-like. It absorbs SRC-D-MSK-1.
***************************************************************************************************/
/**************************************************************************************************
D-LSRC-12 — the BRACKET_TIMESTAMP class - the bracket is the entire difference
A token that is exactly `[` plus one COMPLETE RFC3339 full datetime plus `]` masks to `[<*>]`. The
bracket is the whole of what this rule adds: an unbracketed stamp is digit-leading and was already
masked, so only this one token class moves. Before it, such a token fell through every rule to a
literal KEEP - the diagnostic composite declines it because its colon-digit trigger fires but no
segment is letter-leading, the bracket-index rule declines at the first `-`, and the whole-token
digit mask never sees a `[`-leading byte - so on a timestamped stream reaching the masker without a
dialect declared, every stamped line was its own template. The trigger is deliberately NARROW,
because the class is claimed and nothing adjacent to it: a date-only, time-only, bare-integer, word
or version interior is declined, and so is any trailing punctuation. The byte grammar has ONE owner,
F-SRC-insight-canon:canon.api.cppm:rfc3339_datetime_length, shared with the transport peel, so the
shape is never spelled twice. The output-class collision with the bracket-index rule's own `[<*>]`
is NAMED AND ACCEPTED: both are a masked instance inside brackets, and inventing a second
placeholder vocabulary for one rule is worse than sharing the normal form. It absorbs SRC-D-MSK-5.
***************************************************************************************************/
/**************************************************************************************************
D-LSRC-13 — a class PREFIX inside a bracket survives the mask
The bracketed index rule keeps a short alphabetic prefix that sits inside the bracket and masks only
the digits after it, so a make recursion depth reads `make[<*>]:` and a pytest-xdist worker reads
`[gw<*>]`. The prefix is VOCABULARY: it identifies the producer and is low-cardinality, while only
the index varies, so masking the whole bracket would destroy exactly the distinction the template
exists to carry. This generalizes the pure bracketed-digits form to a prefix plus digits - keep the
stable class marker, mask the varying instance - which is the same shape the counter and currency
rules use. It absorbs SRC-D-TID-13b.
***************************************************************************************************/
/**************************************************************************************************
D-LSRC-14 — a key=value pair masks the VALUE and keeps the KEY
A token of the form key, `=`, digit-leading value normalizes to the key, `=`, wildcard. The key is
the field's NAME and is low-cardinality; the value is the instance. Without the rule a key whose
value is an identifier over-splits once per value, and on an error line that reintroduces the
singleton false-diff the stateless masker exists to kill - a run's distinct transaction ids each
become their own template and each produces a phantom new-and-vanished pair. Two exclusions are
part of the rule. A status value is NOT masked, on the same keyword-and-size gate as the
space-separated carve-out, so a green-to-red flip stays distinct. A value that is a WORD is not
masked either: it is not digit-leading, and equating two spellings of a varying word would require
cross-line learning, which is the unbuilt registry's job and never this masker's. It absorbs
SRC-D-TID-17.
***************************************************************************************************/
// refs: ADR-16.D2, SRC-D-MSK-2, SRC-D-MSK-4
// refs: SRC-D-TID-5, SRC-D-TID-12, SRC-D-TID-13, SRC-D-TID-14, SRC-D-TID-17
struct StatelessTemplate
{
    std::string_view template_str;
    std::span<const std::string_view> params;
};

[[nodiscard]] StatelessTemplate
stateless_template(std::string_view content, ArenaAllocator& out_arena, const MaskConfig& config);

// invariant: every accessor returns a view DERIVED from the table the masker itself reads, never a
// list restated beside it.
// invariant: this limb sees a rule ADDED, REMOVED or REORDERED and nothing else - widening an
// existing rule in place leaves every table byte-identical.
// note: the other limb is the masked-output golden, and neither substitutes for the other.
// refs: SRC-D-TID-16
namespace rule_catalog
{

    // post: the composite rule ids, in the precedence order the dispatcher tries them.
    [[nodiscard]] std::span<const std::string_view> composite_rule_ids() noexcept;

    // post: the id of the first composite rule that CLAIMS `token`, empty when the layer declines
    // it - including when the pre-gate skips the catalog outright.
    // note: not noexcept: it allocates, and that would turn an allocation failure into a terminate.
    [[nodiscard]] std::string_view composite_rule_claiming(std::string_view token);

    // post: the status lexicon that gates the KEEP carve-out and its key-value form, lowercase.
    [[nodiscard]] std::span<const std::string_view> status_keywords() noexcept;

    // post: the declared currency markers, as their literal byte sequences.
    [[nodiscard]] std::span<const std::string_view> currency_markers() noexcept;

    // post: each declared ephemeral root as its ordered path COMPONENTS - a root is components,
    // never a string containing a separator.
    [[nodiscard]] std::span<const std::span<const std::string_view>>
    ephemeral_root_segments() noexcept;

    // post: the hex-run length at or above which a run is an instance hash rather than a word.
    // note: exposed so a witness asserts a run sits below the floor instead of restating it.
    [[nodiscard]] std::size_t min_hash_length() noexcept;

} // namespace rule_catalog

} // namespace insight::tokenization
