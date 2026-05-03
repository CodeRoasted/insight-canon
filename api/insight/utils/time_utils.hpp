#pragma once

#include <optional>
#include <string_view>

#include "insight/core/types.hpp"

namespace insight::utils
{

// Parse ISO 8601 / RFC 3339 timestamps (UTC).
// Accepted forms: "2024-01-15T10:30:00Z", "2024-01-15T10:30:00.123Z",
//                 "2024-01-15T10:30:00+05:30", "2024-01-15 10:30:00"
[[nodiscard]] std::optional<Timestamp> parse_iso8601(std::string_view timestamp_str) noexcept;

// Parse BSD syslog timestamp (no year — uses current year, local time).
// Accepted form: "Jan  1 12:00:00" or "Jan 15 08:03:22"
[[nodiscard]] std::optional<Timestamp> parse_bsd_syslog_ts(std::string_view timestamp_str) noexcept;

// Parse CLF/Combined-Log-Format timestamp.
// Accepted form: "10/Oct/2000:13:55:36 -0700"
[[nodiscard]] std::optional<Timestamp> parse_clf_timestamp(std::string_view timestamp_str) noexcept;

// Parse Unix epoch seconds (e.g. "1117838570") to Timestamp.
[[nodiscard]] std::optional<Timestamp>
parse_epoch_timestamp(std::string_view timestamp_str) noexcept;

// Parse HDFS compact date+time: date="YYMMDD", time="HHMMSS".
[[nodiscard]] std::optional<Timestamp> parse_compact_date_time(std::string_view date,
                                                               std::string_view time) noexcept;

// Parse Spark-style short-year date+time: "YY/MM/DD HH:MM:SS" (19 chars).
[[nodiscard]] std::optional<Timestamp>
parse_short_year_slash(std::string_view timestamp_str) noexcept;

// Parse BGL dotted date: "YYYY.MM.DD" (10 chars).  No time component.
[[nodiscard]] std::optional<Timestamp> parse_dotted_date(std::string_view timestamp_str) noexcept;

// Parse Apache error-log timestamp: "Sun Dec 04 04:47:44 2005" (24 chars).
[[nodiscard]] std::optional<Timestamp>
parse_apache_error_ts(std::string_view timestamp_str) noexcept;

// Parse HealthApp compact timestamp: "YYYYMMDD-HH:MM:SS:mmm" (22 chars).
[[nodiscard]] std::optional<Timestamp> parse_health_app_ts(std::string_view timestamp_str) noexcept;

// Parse ISO-like timestamp with comma or dot milliseconds (Log4j / Windows
// CBS). Accepted: "2024-01-15 10:30:00,123" or "2024-01-15 10:30:00.123" Unlike
// parse_iso8601, this REQUIRES space separator (not T) and milliseconds.
[[nodiscard]] std::optional<Timestamp>
parse_log4j_timestamp(std::string_view timestamp_str) noexcept;

// Parse a log-level string case-insensitively.
// Recognises: trace, debug, info, warn/warning, error/err, fatal/critical/crit.
[[nodiscard]] LogLevel parse_log_level(std::string_view level_str) noexcept;

// Parse Nginx error-log timestamp (same format as Apache error logs).
[[nodiscard]] std::optional<Timestamp>
parse_nginx_error_ts(std::string_view timestamp_str) noexcept;
} // namespace insight::utils
