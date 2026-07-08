// insight.canon.detail.strategy — SEALED format-strategy domain (1.5.2 domain decomposition,
// §11.9.11). ParsedLine, the IFormatStrategy contract, and the 20 concrete format strategies.
// Imports api (Timestamp/LogLevel/LogFormat/ArenaAllocator) — never the facade. The strategy impl
// units additionally import detail.scan (fast_gates predicates) and keep simdjson textual in their
// GMFs (simdjson_scratch.hpp). Never re-exported by the facade and never installed (PRIVATE file
// set).
export module insight.canon.detail.strategy;
import insight.canon.internal; // std + global C types
import insight.canon.api;      // Timestamp, LogLevel, LogFormat, ArenaAllocator
// ParsedLine + IFormatStrategy — the provider CODE-TIER contract (ADR 0024 §2.3.1/§2.4) — moved to
// the INSTALLED insight.canon.spi module so an external semantic package can implement a dialect
// strategy without importing this SEALED shard. Re-exported here so the 19 core representation
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

/// Parses BlueGene/L (BGL) and Thunderbird supercomputer log formats:
///   BGL:         "- 1117838570 2005.06.03 R02-M1 ... RAS KERNEL INFO msg"
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
// (ADR 0024 §1.3/§2.3.1: a dialect strategy is code-tier package knowledge, not a core representation
// format). It reaches the FormatDetector as a composed StrategyFactory (register_strategy seam), so
// canon core registers no dialect. The 19 strategies below are universal representation formats.

// ──────── from src/insight/tokenization/strategies/health_app.hpp ────────
export namespace insight::tokenization
{

/// Parses pipe-delimited HealthApp log format:
///   "20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579"
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

  private:
    static constexpr std::array<std::string_view, 5> kTimestampKeys{"timestamp", "ts", "@timestamp",
                                                                    "time", "datetime"};
    static constexpr std::array<std::string_view, 4> kLevelKeys{"level", "severity", "loglevel",
                                                                "log_level"};
    static constexpr std::array<std::string_view, 5> kMessageKeys{"message", "msg", "log", "text",
                                                                  "body"};
    static constexpr std::array<std::string_view, 5> kComponentKeys{"component", "source", "logger",
                                                                    "service", "module"};
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

  private:
    [[nodiscard]] static std::optional<Timestamp>
    parse_bsd_timestamp(std::string_view timestamp_str);
    [[nodiscard]] static std::optional<Timestamp>
    parse_iso_timestamp(std::string_view timestamp_str);
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
