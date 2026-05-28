#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace insight
{

// ── Time ──
using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;

// ── Identifiers ──
using EventID = uint64_t;
using TemplateID = uint64_t;
using SessionID = uint64_t;
using WindowID = uint64_t;

// ── Sequences ──
using NGram = std::vector<EventID>;

// ── Enums ──
enum class LogLevel : uint8_t
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(LogLevel level) noexcept
{
    using namespace std::literals;

    switch (level)
    {
    case LogLevel::Trace:
        return "Trace"sv;
    case LogLevel::Debug:
        return "Debug"sv;
    case LogLevel::Info:
        return "Info"sv;
    case LogLevel::Warn:
        return "Warn"sv;
    case LogLevel::Error:
        return "Error"sv;
    case LogLevel::Fatal:
        return "Fatal"sv;
    default:
        return "Unknown"sv;
    }
}

enum class LogFormat : uint8_t
{
    Syslog,
    JSON,
    KeyValue,
    CLF,
    Log4j,
    SparkHDFS,
    BGL,
    AndroidLogcat,
    ApacheError,
    WindowsCBS,
    HealthApp,
    Proxifier,
    HPC,
    CloudWatch,
    SystemdJournal,
    IISW3C,
    RFC5424,
    NginxError,
    // GitHub Actions / Azure Pipelines logs: every line prefixed with an
    // RFC3339 UTC timestamp at 100-ns (7-fraction-digit) precision + 'Z', then
    // the raw message (which may carry ##[group]/##[error]/##[warning] workflow
    // commands). A first-class CI input — without it these lines are mis-claimed
    // by Syslog (RFC3339 prefix) and shredded into empty templates.
    GitHubActions,
    // Catch-all for unstructured text (CI / pytest / build logs). Selected only
    // when no structured strategy matches a non-empty line, so the tokenizer
    // never silently drops a line. Keep immediately before Unknown.
    RawText,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(LogFormat fmt) noexcept
{
    using namespace std::literals;

    switch (fmt)
    {
    case LogFormat::Syslog:
        return "Syslog"sv;
    case LogFormat::JSON:
        return "JSON"sv;
    case LogFormat::KeyValue:
        return "KeyValue"sv;
    case LogFormat::CLF:
        return "CLF"sv;
    case LogFormat::Log4j:
        return "Log4j"sv;
    case LogFormat::SparkHDFS:
        return "SparkHDFS"sv;
    case LogFormat::BGL:
        return "BGL"sv;
    case LogFormat::AndroidLogcat:
        return "AndroidLogcat"sv;
    case LogFormat::ApacheError:
        return "ApacheError"sv;
    case LogFormat::WindowsCBS:
        return "WindowsCBS"sv;
    case LogFormat::HealthApp:
        return "HealthApp"sv;
    case LogFormat::Proxifier:
        return "Proxifier"sv;
    case LogFormat::HPC:
        return "HPC"sv;
    case LogFormat::CloudWatch:
        return "CloudWatch"sv;
    case LogFormat::SystemdJournal:
        return "SystemdJournal"sv;
    case LogFormat::IISW3C:
        return "IISW3C"sv;
    case LogFormat::RFC5424:
        return "RFC5424"sv;
    case LogFormat::NginxError:
        return "NginxError"sv;
    case LogFormat::GitHubActions:
        return "GitHubActions"sv;
    case LogFormat::RawText:
        return "RawText"sv;
    default:
        return "Unknown"sv;
    }
}

// What a LINE does in the sequence (its structural role), as opposed to what a
// token inside it MEANS (its SemanticClass). Two orthogonal ontologies, two
// registries — keeping them separate is what avoids the value-vs-line-role
// conflation (Salience epic flaw F12, §4.2). These roles are ANNOUNCED — the line
// declares itself via a marker (`##[group]`, `##[error]`, a non-zero exit) — never
// derived from graph position (that is a structural-layer output, not a role).
// A seed catalog; designed to grow during calibration.
enum class StructuralRole : uint8_t
{
    None = 0,    ///< no announced role (the common case)
    GroupBegin,  ///< a section/group opens (`##[group]`)
    GroupEnd,    ///< a section/group closes (`##[endgroup]`)
    Terminator   ///< an outcome/failure marker (`##[error]`, error/fatal level, non-zero exit)
};

[[nodiscard]] constexpr std::string_view to_string(StructuralRole role) noexcept
{
    using namespace std::literals;

    switch (role)
    {
    case StructuralRole::GroupBegin:
        return "GroupBegin"sv;
    case StructuralRole::GroupEnd:
        return "GroupEnd"sv;
    case StructuralRole::Terminator:
        return "Terminator"sv;
    default:
        return "None"sv;
    }
}

enum class ParseError : uint8_t
{
    EmptyLine,
    InvalidFormat,
    TimestampFailed,
    Corrupted
};

[[nodiscard]] constexpr std::string_view to_string(ParseError err) noexcept
{
    using namespace std::literals;

    switch (err)
    {
    case ParseError::EmptyLine:
        return "EmptyLine"sv;
    case ParseError::InvalidFormat:
        return "InvalidFormat"sv;
    case ParseError::TimestampFailed:
        return "TimestampFailed"sv;
    case ParseError::Corrupted:
        return "Corrupted"sv;
    default:
        return "Unknown"sv;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AnomalyType — coarse classification
// ─────────────────────────────────────────────────────────────────────────────
enum class AnomalyType : uint8_t
{
    None = 0,
    DistributionDrift,
    SequenceAnomaly,
    FrequencyAnomaly,
    StabilityAnomaly,
    Composite,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(AnomalyType atype) noexcept
{
    using namespace std::literals;

    switch (atype)
    {
    case AnomalyType::None:
        return "None"sv;
    case AnomalyType::DistributionDrift:
        return "DistributionDrift"sv;
    case AnomalyType::SequenceAnomaly:
        return "SequenceAnomaly"sv;
    case AnomalyType::FrequencyAnomaly:
        return "FrequencyAnomaly"sv;
    case AnomalyType::StabilityAnomaly:
        return "StabilityAnomaly"sv;
    case AnomalyType::Composite:
        return "Composite"sv;
    default:
        return "Unknown"sv;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sub-score produced by an individual detector
// ─────────────────────────────────────────────────────────────────────────────
struct SubScore
{
    std::string detector_name;
    double raw_score{0.0};
    double normalized{0.0};
    double confidence{0.0};
    std::string evidence;
};

// ─────────────────────────────────────────────────────────────────────────────
// Severity classification
// ─────────────────────────────────────────────────────────────────────────────
enum class Severity : uint8_t
{
    None = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Critical = 4
};

[[nodiscard]] constexpr std::string_view severity_to_string(Severity sev) noexcept
{
    switch (sev)
    {
        using namespace std::literals;
    case Severity::None:
        return "None"sv;
    case Severity::Low:
        return "Low"sv;
    case Severity::Medium:
        return "Medium"sv;
    case Severity::High:
        return "High"sv;
    case Severity::Critical:
        return "Critical"sv;
    default:
        return "Unknown"sv;
    }
}

} // namespace insight
