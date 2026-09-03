// insight.canon.detail.strategy — SEALED format-strategy domain (
// ADR-3.D4). ParsedLine, the IFormatStrategy contract, and the 21 concrete format strategies.
// Imports api (Timestamp/LogLevel/LogFormat/ArenaAllocator) — never the facade. The strategy impl
// units additionally import detail.scan (fast_gates predicates) and keep simdjson textual in their
// GMFs (simdjson_scratch.hpp). Never re-exported by the facade and never installed (PRIVATE file
// set).
export module insight.canon.detail.strategy;
import insight.canon.internal; // std + global C types
import insight.canon.api;      // Timestamp, LogLevel, LogFormat, ArenaAllocator
// ParsedLine + IFormatStrategy — the provider CODE-TIER contract (ADR-17) — moved to
// the INSTALLED insight.canon.spi module so an external semantic package can implement a dialect
// strategy without importing this SEALED shard. Re-exported here so the 20 core representation
// strategies below (module members of this shard) are byte-unchanged.
export import insight.canon.spi;

// ──────── from src/insight/tokenization/strategies/android_logcat.hpp ────────
export namespace insight::tokenization
{

/// Parses Android logcat format:
///   "03-17 16:13:38.811 1702 2395 D WindowManager: msg"
class AndroidLogcatStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/apache_error.hpp ────────
export namespace insight::tokenization
{

/// Parses Apache error-log format:
///   "[Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok"
class ApacheErrorLogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/bgl.hpp ────────
export namespace insight::tokenization
{

/// Parses BlueGene/L (BGL) and Thunderbird supercomputer log formats. The leading column is
/// LogHub's alert LABEL — `-` on a normal record, otherwise a bounded uppercase class name — and
/// it is validated by the grammar and carried in no projection field (DN-43.D14):
///   BGL:         "- 1117838570 2005.06.03 R02-M1 ... RAS KERNEL INFO msg"
///   BGL alert:   "KERNDTLB 1117838570 2005.06.03 R02-M1 ... RAS KERNEL FATAL msg"
///   Thunderbird: "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228 crond:
///   msg"
class BGLStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/clf.hpp ────────
export namespace insight::tokenization
{

// Common Log Format / Combined Log Format (Apache/Nginx access logs)
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

// ──────── from src/insight/tokenization/strategies/cloudwatch.hpp ────────
export namespace insight::tokenization
{

/// Parses AWS CloudWatch JSON log format:
///   {"timestamp":1705312200000,"message":"User login","logGroup":"/aws/lambda/myFunc"}
class CloudWatchStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// GitHubActionsStrategy — the GHA/Azure DIALECT strategy — relocated to insight_semantic_github
// (ADR-17: a dialect strategy is code-tier package knowledge, not a core
// representation format). It reaches the FormatDetector as a composed StrategyFactory
// (register_strategy seam), so canon core registers no dialect. The 20 strategies below are
// universal representation formats.

// ──────── from src/insight/tokenization/strategies/health_app.hpp ────────
export namespace insight::tokenization
{

/// Parses the pipe-delimited HealthApp record — FOUR fields, THREE separators:
///   "YYYYMMDD-HH:MM:SS:mmm|component|process_id|message"
///   "20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579"
/// Every clock field is variable-width and the corpus is NOT zero-padded, so
/// "20171224-0:0:0:232|Step_SPUtils|30002312|setDiffTotalSteps=8874" is the same grammar.
/// The claim predicate (is_health_app_prefix) proves the head AND all three separators: the
/// arity is grammar, because parse() consumes three separators unconditionally and a
/// parse()-side decline deletes the line rather than demoting it (DN-43.D16).
class HealthAppStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/hpc.hpp ────────
export namespace insight::tokenization
{

/// Parses HPC (High Performance Computing) log format:
///   "134681 node-246 unix.hw state_change.unavailable 1077804742 1 Component
///   State Change: ..."
class HPCStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/iis_w3c.hpp ────────
export namespace insight::tokenization
{

/// Parses IIS W3C Extended Log Format:
///   "2024-01-15 10:30:00 GET /index.html - 80 - 10.0.0.1 Mozilla/5.0 200 0 0 15"
class IISW3CStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/json.hpp ────────
export namespace insight::tokenization
{

class JsonStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

    // The four role-name vocabularies moved to json.cpp's implementation scope (SRC-D-ECS-1): they
    // are the strategy's private business, nothing outside its own TU ever named them, and the
    // compound-key router needs them at namespace scope. A module interface that carries a
    // private lookup table publishes an implementation detail and forces every consumer to
    // recompile when a name is added.
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/kv.hpp ────────
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
    // Views into the original (arena-stable) line; values are un-quoted slices.
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

// ──────── from src/insight/tokenization/strategies/log4j.hpp ────────
export namespace insight::tokenization
{

/// Parses Java Log4j / Python logging formats:
///   Hadoop:    "2015-10-18 18:01:47,978 INFO [main] org.apache.hadoop: msg"
///   Zookeeper: "2015-07-29 17:41:44,747 - INFO  [QuorumPeer] - msg"
///   OpenStack: "nova-api.log 2017-05-16 00:00:00.008 25746 INFO nova.osapi
///   [req-id] msg"
class Log4jStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/nginx_error.hpp ────────
export namespace insight::tokenization
{

/// Parses Nginx error-log format:
///   "2024/03/27 10:15:23 [error] 12345#0: *99 connect() failed"
class NginxErrorStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/proxifier.hpp ────────
export namespace insight::tokenization
{

/// Parses Proxifier log format:
///   "[10.30 16:49:06] chrome.exe - proxy.cse.cuhk.edu.hk:5070 open through
///   proxy ..."
class ProxifierStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/raw_text.hpp ────────
export namespace insight::tokenization
{

// Last-resort catch-all for unstructured text (CI / pytest / build logs).
//
// The FormatDetector selects this strategy ONLY when no structured strategy
// matches a non-empty line, so the tokenizer never silently drops a line.
//
// Performance: parse() is zero-copy — `content` is a subview of the (already
// arena-stable) input, produced by trimming leading ASCII whitespace with
// pointer arithmetic. No allocation, no full-line scan. confidence() is a
// constant 0.0, which both keeps it out of the normal majority vote and stops
// LogParser's sticky fast-path from ever latching onto it (a >0 confidence
// would greedily capture every following line).
class RawTextStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/strategy/rfc3339_text.cpp ────────
export namespace insight::tokenization
{

// The leading-RFC-3339 LAYOUT: a stamp token followed by free text.
//   "2026-05-31T08:00:01Z INFO request method=GET path=/api/users/1000 status=200"
//
// A representation format with a timestamp field and no vocabulary (ADR-17.D1: core owns the
// LANGUAGE, packages own the VOCABULARY). It exists because rejecting the shape from Syslog is not
// enough: the one thing Syslog's RFC-3339 branch got RIGHT was the event time, and MetaLog windows
// on event time, so a bare rejection to RawTextStrategy — which sets no timestamp — would fix the
// level, the component and the template and lose the clock (DN-43.D4).
//
// It claims `is_rfc3339_prefix(line) && !scan_syslog_header(line)`, and the negation lives in the
// PREDICATE rather than in a confidence ordering. That makes the two predicates DISJOINT, which is
// what makes LogParser's sticky latch self-correcting on a mixed stream: a genuine syslog line
// scores 0 here and re-detects to Syslog, a timestamped application line scores 0 on Syslog and
// re-detects here. A lower confidence over the bare prefix would instead let the first matching
// line capture the strategy for the rest of the file — the starvation RawTextStrategy's constant
// 0.0 exists to prevent.
//
// It READS the stamp and KEEPS its bytes (DN-43.D12, the SPI's naming-totality rule). `content` is
// the WHOLE line: the stamp is exactly what the declared transport row `api-rfc3339-line-prefix`
// peels, so its layer is undecidable content-side and only a declaration may remove it — the remedy
// for a validated field of undecidable LAYER is *read, never remove*. That makes projection
// totality trivial here rather than argued (`content.empty()` iff the line is empty), and it costs
// nothing at the template grain: a bare digit-leading stamp is one whitespace-delimited token and
// masks to a single leading `<*>`, which is why `normalize_bracket_timestamp` had to exist for the
// BRACKETED sibling and nothing had to exist for this one. The strategy is not deleted, because the
// event time is recovered by the READING, and that is the half we keep.
//
// `component` is left EMPTY: the layout names no functional source, and saying so is a fact where
// inventing one would be a fabrication on the cube's WHERE axis. The BSD prefix gets no sibling —
// `Mon DD HH:MM:SS` carries no year and collides with prose, so consuming it would fabricate an
// event time from an inference.
class Rfc3339TextStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/rfc5424.hpp ────────
export namespace insight::tokenization
{

/// Parses RFC 5424 syslog format:
///   "<PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID [SD] MSG"
///   e.g. "<134>1 2024-01-15T10:30:00Z myhost myapp 1234 ID47 - User logged in"
class RFC5424Strategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/spark_hdfs.hpp ────────
export namespace insight::tokenization
{

/// Parses Spark and HDFS log formats:
///   Spark: "17/06/09 20:10:40 INFO executor.Backend: msg"
///   HDFS:  "081109 203615 148 INFO dfs.DataNode: msg"
class SparkHDFSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── the BGL RECORD predicate — ONE scan, two readers (DN-43.D2, DN-43.D14) ────────
// Module-internal: defined in bgl.cpp, whose grammar it is, and consumed by nothing else.
namespace insight::tokenization
{

// What BGLStrategy CLAIMS, and therefore what it validates before removing a byte from `content`
// (DN-43.D11 condition 1). The grammar is
//
//   <label> <epoch> <date> <node> <ts2> <node2> <FACILITY> <SUBSYS> <LEVEL> <msg>   (RAS records)
//   <label> <epoch> <date> <node> <ts2>         <FACILITY> <SUBSYS> <LEVEL> <msg>   (306 corpus
//   lines) <label> <epoch> <date> <node> <Mon> <DD> <HH:MM:SS> <host> [<tag>[pid]:] <msg>
//   (Thunderbird)
//
// `<label>` is the alert class and is carried NOWHERE — see is_bgl_labelled_prefix.
//
// THREE FIELDS ARE CONSUMED AND NOT PUBLISHED, AND THEY DO NOT SHARE A VERDICT — this sentence
// used to say all three were "validated grammar", and exactly ONE of them is not validated.
// `DN-43.D15`'s criterion decides each: a consumed-but-unpublished field is validated iff its
// value proves the record's FIELD ALIGNMENT, never as byte hygiene.
//   * `<FACILITY>` IS validated — `starts_with_facility` is what selects between the two RAS
//     header shapes.
//   * `<date>` IS validated, and NOT at the site where it is consumed, which is the trap this
//     enumeration exists to close. `scan_bgl_record`'s `(void)sv_take_token(rest)` applies nothing
//     to it; the predicate already ran in the FORMAT GATE `is_bgl_labelled_prefix`
//     (`F-SRC-insight-canon:canon.detail.scan.cppm`), which keys the BGL/Thunderbird grammar on
//     three opening fields — the label, the digit `<epoch>`, and `<date>`'s exact dotted
//     `YYYY.MM.DD` shape. A discriminator, so it sits on the alignment side of the criterion.
//     Reading its verdict off the consumption site instead gets it backwards.
//   * `<node2>` is the ONE that is not validated, anywhere. No predicate touches its bytes, and
//     `DN-43.D15` rules it must STAY that way: the alignment proof sits on the token BEHIND it
//     (the `RAS`/`NULL` probe), which succeeds truthfully without reading it. Validating it would
//     be actively harmful — the 8 pinned-corpus records whose `<node2>` holds a binary blob have
//     flawless headers, and declining them moves the blob out of a DROPPED field into
//     `RawTextStrategy`'s whole-line `content`, a published template NAME, while throwing away a
//     level the producer DECLARED.
// Both verdicts are pinned by `BGLStrategyTest` (`F-SRC-insight-canon:test_strategies.cpp`), with
// no corpus mounted: a malformed `<date>` declines, a binary `<node2>` parses.
//
// Everything the parse then publishes is below, so `confidence()` scoring non-zero means exactly
// "this parse will succeed", which is the only construction under which the gate and the grammar
// cannot drift apart.
struct BglRecord
{
    std::string_view epoch;     // <epoch> — seconds since the Unix epoch; the record's event time
    std::string_view node;      // <node> — the node identity (F3b: host, hors-cube)
    std::string_view component; // RAS: <SUBSYS>. Thunderbird: the daemon tag, EMPTY when none
    std::string_view content;   // every byte after the header — the total projection (ADR-16.D9)
    // Engaged on the RAS branch only: BGL DECLARES its severity in a fixed column, so the level is
    // read, not guessed. Disengaged on the Thunderbird branch, which has no level column at all
    // and infers from the message body in the `inferred` species (DN-43.D5).
    std::optional<LogLevel> declared_level;
};

// nullopt = the line is not a BGL or Thunderbird record. Bounded by the HEADER: every scan stops
// at a whitespace boundary and the message body is handed over as a tail view, never scanned.
[[nodiscard]] std::optional<BglRecord> scan_bgl_record(std::string_view line) noexcept;

} // namespace insight::tokenization

// ──────── the SYSLOG HEADER predicate — ONE scan, three readers (DN-43.D2) ────────
// Deliberately NOT exported: module-internal, defined in syslog.cpp (whose grammar it is) and
// consumed by rfc3339_text.cpp, which claims exactly its negation.
namespace insight::tokenization
{

// The syslog grammar's structural commitment is its HEADER — `TIMESTAMP HOST TAG:` — never its
// timestamp. A leading RFC-3339 stamp is evidence of a TIMESTAMP; reading it as evidence of syslog
// is what let the strategy claim application lines it could not parse, at 0.80, and shred them into
// one empty template (DN-43.D1).
//
// This function IS SyslogStrategy's claim. `confidence()` scores non-zero iff it yields a value and
// `parse()` consumes the split it already computed instead of re-deriving it by hand — the only
// construction under which the gate and the grammar cannot drift apart (DN-43.D2). The same value
// serves the parse gate, the detector's routing, and LogParser's sticky stream latch: one
// predicate, three readers, and making it mean "this parse will succeed" makes it correct for all
// three.
struct SyslogHeader
{
    std::string_view stamp; // the raw timestamp field — the branch's own timestamp parser reads it
    std::string_view tag;   // the daemon/program name, `[pid]` stripped — the F3b functional source
    std::string_view body;  // EVERY byte after the tag's ':' — the total projection (ADR-16.D9)
    bool bsd{false};        // which stamp grammar matched; selects the timestamp parser
};

// nullopt = the line is not syslog. Bounded by the HEADER: both token scans stop at the first
// whitespace, and the message body is handed over as a tail view that is never scanned.
[[nodiscard]] std::optional<SyslogHeader> scan_syslog_header(std::string_view line) noexcept;

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/syslog.hpp ────────
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

// ──────── from src/insight/tokenization/strategies/systemd_journal.hpp ────────
export namespace insight::tokenization
{

/// Parses systemd journal export format (JSON):
///   {"__REALTIME_TIMESTAMP":"1705312200000000","PRIORITY":"6",
///    "_COMM":"nginx","MESSAGE":"Worker started"}
class SystemdJournalStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── OTEL span-export document unpack (ADR-29, SRC-D-OTEL-18) ──────────────────────────
export namespace insight::tokenization
{

// True iff `line` is an OTLP/JSON resourceSpans trace-export DOCUMENT (shape 1) — the record-
// source layer routes these to unpack_otel_spans before tokenization; a flat span (shape 2)
// and every non-OTEL line return false. A cheap raw-byte check, no simdjson cursor spent.
//
// ADR-29.D7 — the probe is GATED and O(1), and that is a shipping condition, not a tuning
// preference: it runs on every JSON line of every stream, and the overwhelming majority of those
// streams are not OTEL at all. The gate is the JSON-family layout check (first non-whitespace byte
// is '{', the same test JsonStrategy::confidence applies); behind it the root object's FIRST KEY is
// COMPARED, never searched for. A whole-line substring scan is what D7 forbids — and a bounded
// windowed search is not the cheap answer either (the measurement is at the definition site).
[[nodiscard]] bool is_otel_span_document(std::string_view line) noexcept;

// Unpack one OTLP/JSON resourceSpans trace-export DOCUMENT into N CANONICAL flat-span record
// strings (D-OTEL-10 shape 1 → shape 2, SRC-D-OTEL-18a) — byte-form-identical to what the LogCraft
// lab emits for the same spans, so the flat-span parser is authored once and shape-1 ≡ shape-2 is
// a golden-tested property. resource `service.name` is injected into each span's attributes (the
// declared allowlist); span attributes are copied verbatim. Appends to `out`; returns the count
// of spans emitted (0 if `document` is not a resourceSpans export).
std::size_t unpack_otel_spans(std::string_view document, std::vector<std::string>& out);

// L3 (DN-29.D15) — the ACQUISITION-side recogniser: broad and deliberately over-triggering. NOT
// is_otel_span_document (L1), which is the record path's O(1) first-key compare and is defeated by
// a non-canonical key order. The acquisition entry holds the whole input by definition and is not
// the hot path, so recall is what matters there and precision is what matters on the record path.
// See the definition site for why one predicate could not serve both.
[[nodiscard]] bool is_otel_span_document_broad(std::string_view document) noexcept;

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/windows_cbs.hpp ────────
export namespace insight::tokenization
{

/// Parses Windows CBS/CSI log format:
///   "2016-09-28 04:30:30, Info    CBS    Loaded Servicing Stack ..."
class WindowsCBSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
