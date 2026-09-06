// invariant: the SEALED strategy domain — ParsedLine, the strategy interface and the concrete
// representation strategies; it imports the api and never the facade.
// invariant: never re-exported by the facade and never installed, so nothing outside canon can
// import it.
// refs: ADR-3.D4
export module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
// invariant: the provider CODE-TIER contract lives in the INSTALLED spi module so an external
// package can implement a dialect strategy without importing this sealed shard.
// invariant: it is re-exported here so the core representation strategies below, which are members
// of this shard, are byte-unchanged.
// refs: ADR-17
export import insight.canon.spi;

export namespace insight::tokenization
{

// post: an Android logcat record — a month-day clock, two ids, a one-letter level and a
// colon-terminated tag.
class AndroidLogcatStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: an Apache error-log record — a bracketed full date, a bracketed level, then the message.
class ApacheErrorLogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: a BlueGene/L or Thunderbird supercomputer record.
// invariant: the LEADING column is LogHub's alert LABEL — a dash on a normal record, a bounded
// uppercase class name on an anomalous one — and the grammar VALIDATES it.
// invariant: it is carried in NO projection field, because the column is the corpus curators'
// answer key rather than producer content.
// refs: DN-43.D14
class BGLStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: a Common or Combined Log Format access record.
class CLFStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    [[nodiscard]] static std::optional<Timestamp>
    parse_clf_timestamp(std::string_view timestamp_str);
    [[nodiscard]] static LogLevel status_code_to_level(int status);
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: an AWS CloudWatch JSON record — a millisecond epoch, a message and a log group.
class CloudWatchStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// invariant: the GitHub-Actions dialect strategy is NOT here — a dialect strategy is code-tier
// PACKAGE knowledge, not a core representation format.
// invariant: it reaches the detector as a COMPOSED strategy factory, so canon core registers no
// dialect and every strategy below is a universal representation format.
// refs: ADR-17
export namespace insight::tokenization
{

// post: a pipe-delimited HealthApp record — FOUR fields and THREE separators, the head being a
// compact date and a variable-width clock.
// invariant: every clock field is variable-width because the corpus is NOT zero-padded, so a
// one-digit minute is the same grammar as a two-digit one.
// invariant: the ARITY is grammar: the claim predicate proves the head AND all three separators,
// because parse consumes three unconditionally and a parse-side decline DELETES the line.
// refs: DN-43.D16
class HealthAppStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: an HPC record — a numeric id, a node, a facility, a state change and an epoch.
class HPCStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: an IIS W3C extended access record — a space-separated fixed field order.
class IISW3CStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// invariant: this strategy declares NO role-name vocabularies — they are its PRIVATE business and
// live in its implementation scope, where the compound-key router needs them.
// invariant: a module interface carrying a private lookup table publishes an implementation detail,
// and forces every consumer of that interface to recompile when a name is added.
// refs: SRC-D-ECS-1
class JsonStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

class KVStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    // invariant: views into the original arena-stable line; values are UN-QUOTED slices, so nothing
    // is copied and nothing is owned.
    struct KVPair
    {
        std::string_view key;
        std::string_view value;
    };
    [[nodiscard]] static std::vector<KVPair> extract_pairs(std::string_view line);
    [[nodiscard]] static std::optional<Timestamp> try_parse_timestamp(std::string_view value);
    [[nodiscard]] static LogLevel try_parse_level(std::string_view value);
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: a Java Log4j or Python logging record — Hadoop, Zookeeper and OpenStack layouts share one
// grammar.
class Log4jStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: an Nginx error-log record — a slash-separated date, a bracketed level, a pid and a
// connection id.
class NginxErrorStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: a Proxifier record — a bracketed day-and-clock, a process name and a connection event.
class ProxifierStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// invariant: the last-resort catch-all, selected ONLY when no structured strategy matches a
// NON-EMPTY line, so the tokenizer never silently drops a line.
// invariant: parse is zero-copy — content is a subview of the already arena-stable input, trimmed
// by pointer arithmetic, with no allocation and no full-line scan.
// invariant: confidence is a CONSTANT ZERO, which keeps it out of the majority vote AND stops the
// sticky fast path latching onto it; any positive score would capture every later line.
class RawTextStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// invariant: the leading-RFC-3339 LAYOUT — a stamp token then free text, no vocabulary — so it
// is a core REPRESENTATION format and not a dialect: it names no ecosystem.
// invariant: it exists because rejecting the shape from Syslog is not enough — Syslog got the
// EVENT TIME right, and MetaLog windows on event time, so a bare rejection would lose the clock.
// invariant: it claims the RFC-3339 prefix AND the negation of the syslog header, and that negation
// lives in the PREDICATE rather than in a confidence ordering.
// invariant: that makes the two predicates DISJOINT, which is what makes the sticky latch
// self-correcting on a mixed stream — each shape scores zero on the other and re-detects.
// invariant: a lower confidence over the bare prefix would instead let the first matching line
// capture the strategy for the rest of the file.
// invariant: it READS the stamp and KEEPS its bytes — content is the WHOLE line, because the
// stamp is exactly what the declared transport row peels, so its layer is undecidable content-side.
// invariant: the remedy for a validated field of undecidable LAYER is read, never remove, which
// makes projection totality trivial here rather than argued.
// invariant: it costs nothing at the template grain BECAUSE a bare stamp is one
// whitespace-delimited digit-leading token, which the masker already collapses to one wildcard.
// invariant: component is left EMPTY because the layout names no functional source, and saying so
// is a fact where inventing one would be a fabrication on the cube's WHERE axis.
// invariant: the BSD prefix gets no sibling — it carries no year and collides with prose, so
// consuming it would fabricate an event time from an inference.
// refs: ADR-17.D1, DN-43.D4, DN-43.D12
class Rfc3339TextStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: an RFC 5424 syslog record — a bracketed priority, a version, then the declared field
// order.
class RFC5424Strategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: a Spark or HDFS record — a short-year slash date, or a compact six-digit date and clock.
class SparkHDFSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// invariant: module-internal — defined in the strategy whose grammar it is, and consumed by
// nothing else.
// invariant: ONE scan with two readers, so the format gate and the grammar cannot drift apart.
// refs: DN-43.D2, DN-43.D14
namespace insight::tokenization
{

// invariant: what the strategy CLAIMS is what it validates before removing a byte from content; the
// alert label is carried NOWHERE.
// invariant: THREE fields are consumed and not published, and they do NOT share a verdict — a
// consumed-but-unpublished field is validated iff its value proves the record's FIELD ALIGNMENT.
// invariant: the FACILITY is validated, because it is what selects between the two header shapes.
// invariant: the DATE is validated, and NOT at the site where it is consumed — the format gate
// already keyed the grammar on its exact dotted shape, so it is a discriminator.
// invariant: reading its verdict off the consumption site instead gets it backwards, which is the
// trap this enumeration exists to close.
// invariant: the SECOND NODE field is the one that is not validated anywhere, and it must STAY that
// way — the alignment proof sits on the token behind it and succeeds without reading it.
// invariant: validating it would be HARMFUL — the pinned records whose second node holds a binary
// blob have flawless headers, and declining them republishes the blob as a template.
// invariant: it would also throw away a level the producer DECLARED.
// invariant: everything the parse publishes is declared below, so a non-zero confidence means
// exactly this parse will succeed — gate and grammar then cannot drift apart.
// refs: DN-43.D11, DN-43.D15
// refs: F-SRC-insight-canon:canon.detail.scan.cppm, F-SRC-insight-canon:test_strategies.cpp
struct BglRecord
{
    // invariant: the node is the node IDENTITY, so it is the host and hors-cube.
    // invariant: the component is the subsystem on the RAS branch and the daemon tag on the
    // Thunderbird one, empty when there is none.
    // invariant: content is EVERY byte after the header — the total projection.
    // invariant: the declared level is engaged on the RAS branch ONLY, because that grammar
    // DECLARES its severity in a fixed column, so the level is read rather than guessed.
    // invariant: the Thunderbird branch has no level column at all and infers from the message body
    // in the inferred species.
    // refs: ADR-16.D9, DN-43.D5, ADR-19.D4
    std::string_view epoch;
    std::string_view node;
    std::string_view component;
    std::string_view content;
    std::optional<LogLevel> declared_level;
};

// post: nullopt means the line is not a record of either grammar.
// invariant: bounded by the HEADER — every scan stops at a whitespace boundary and the message
// body is handed over as a tail view, never scanned.
[[nodiscard]] std::optional<BglRecord> scan_bgl_record(std::string_view line) noexcept;

} // namespace insight::tokenization

// invariant: deliberately NOT exported — module-internal, defined in the strategy whose grammar
// it is, and consumed by the one other strategy that claims exactly its negation.
// refs: DN-43.D2
namespace insight::tokenization
{

// invariant: the grammar's structural commitment is its HEADER, never its timestamp — a leading
// RFC-3339 stamp is evidence of a TIMESTAMP and not of syslog.
// invariant: reading it as evidence of syslog is what let the strategy claim application lines it
// could not parse and shred them into one empty template.
// invariant: this function IS the claim: confidence scores non-zero iff it yields a value, and
// parse consumes the split it already computed instead of re-deriving it by hand.
// invariant: one predicate serves the parse gate, the detector's routing and the sticky stream
// latch, and making it mean this parse will succeed makes it correct for all three.
// refs: DN-43.D1, DN-43.D2
struct SyslogHeader
{
    // invariant: the stamp is the raw timestamp field, read by the branch's own parser; the tag is
    // the daemon name with any pid stripped, and it is the functional source.
    // invariant: the body is EVERY byte after the tag's colon — the total projection — and the
    // flag says which stamp grammar matched, which selects the timestamp parser.
    // refs: ADR-16.D9, ADR-19.D4
    std::string_view stamp;
    std::string_view tag;
    std::string_view body;
    bool bsd{false};
};

// post: nullopt means the line is not syslog.
// invariant: bounded by the HEADER — both token scans stop at the first whitespace and the
// message body is handed over as a tail view that is never scanned.
[[nodiscard]] std::optional<SyslogHeader> scan_syslog_header(std::string_view line) noexcept;

} // namespace insight::tokenization

// invariant: the record-source layer routes an export DOCUMENT to the unpack before tokenization; a
// flat span and every non-OTEL line are not documents.
// refs: ADR-29, SRC-D-OTEL-18
export namespace insight::tokenization
{

class SyslogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

class SystemdJournalStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// post: true iff the line is an OTLP trace-export DOCUMENT; a cheap raw-byte check with no simdjson
// cursor spent.
// invariant: the probe is GATED and O(1) as a SHIPPING condition, not a tuning preference — it
// runs on every JSON line of every stream and most of those streams are not OTEL at all.
// invariant: the gate is the JSON-family layout check, and behind it the root object's FIRST KEY is
// COMPARED, never searched for.
// invariant: a whole-line substring scan is forbidden, and a bounded windowed search is not the
// cheap answer either — the measurement is at the definition site.
// refs: ADR-29.D7
[[nodiscard]] bool is_otel_span_document(std::string_view line) noexcept;

// post: the number of spans emitted, and 0 when the input is not an export document.
// invariant: one document becomes N CANONICAL flat-span records, byte-form-identical to what the
// lab emits for the same spans, so the flat-span parser is authored ONCE.
// invariant: that is what makes shape-1 equals shape-2 a golden-tested property rather than a hope;
// the resource service name is injected into each span's attributes.
// refs: SRC-D-OTEL-18a
std::size_t unpack_otel_spans(std::string_view document, std::vector<std::string>& out);

// invariant: the ACQUISITION-side recogniser — broad and deliberately OVER-triggering, which is
// the opposite tuning from the record path's O(1) first-key compare.
// invariant: the acquisition entry holds the whole input by definition and is not the hot path, so
// RECALL matters here and PRECISION matters on the record path.
// note: the definition site states why one predicate could not serve both.
// refs: DN-29.D15
[[nodiscard]] bool is_otel_span_document_broad(std::string_view document) noexcept;

} // namespace insight::tokenization

export namespace insight::tokenization
{

class WindowsCBSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
