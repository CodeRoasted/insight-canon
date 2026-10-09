// invariant: a canon warning carries counts and the routed format, never a byte of the input line,
// because a host journal keeps it and the hosted diff promises its input is not stored.
// invariant: each arm first requires the warning to be HEARD, so a silent logger cannot pass it.
// refs: ADR-16.D9, ADR-29.D7
#include <gtest/gtest.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>

import insight.canon.test;

using insight::tokenization::ArenaAllocator;
using insight::tokenization::CanonicalEvent;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{

// note: the program name of the syslog probe, the one Argos's audit saw quoted by the release sift.
constexpr std::string_view kSyslogSecret{"SECRETPROG"};
constexpr std::string_view kSyslogProbe{"Oct  9 12:00:00 host SECRETPROG[12]:"};
constexpr std::string_view kJsonSecretKey{"secretkey_alpha"};
constexpr std::string_view kJsonSecretOtherKey{"secretkey_beta"};
constexpr std::string_view kJsonProbe{R"({"secretkey_alpha": {"depth": 1}, "secretkey_beta": 2})"};
constexpr std::string_view kEmptyProjectionHeading{"empty projection"};
constexpr std::string_view kRolelessHeading{"NO recognized role"};

class CapturingSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
  public:
    [[nodiscard]] const std::vector<std::string>& payloads() const noexcept
    {
        return payloads_;
    }

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        payloads_.emplace_back(msg.payload.data(), msg.payload.size());
    }
    void flush_() override {}

  private:
    std::vector<std::string> payloads_;
};

// post: a capture sink attached to `logger` for the probe's lifetime, its sink list restored on
// exit.
class AttachedCapture
{
  public:
    explicit AttachedCapture(std::shared_ptr<spdlog::logger> logger)
        : logger_{std::move(logger)}, sink_{std::make_shared<CapturingSink>()},
          saved_sinks_{logger_->sinks()}
    {
        logger_->sinks().push_back(sink_);
    }
    ~AttachedCapture()
    {
        logger_->sinks() = saved_sinks_;
    }
    AttachedCapture(const AttachedCapture&) = delete;
    AttachedCapture& operator=(const AttachedCapture&) = delete;
    AttachedCapture(AttachedCapture&&) = delete;
    AttachedCapture& operator=(AttachedCapture&&) = delete;

    [[nodiscard]] const std::vector<std::string>& payloads() const noexcept
    {
        return sink_->payloads();
    }

  private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<CapturingSink> sink_;
    std::vector<spdlog::sink_ptr> saved_sinks_;
};

// invariant: one arena, one zero-package composition and one tokenizer per probe, so no routed
// format or counter crosses from one probe to the next.
struct Door
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};
};

// post: the payloads that contain `heading`, in capture order.
[[nodiscard]] std::vector<std::string> matching(const std::vector<std::string>& payloads,
                                                std::string_view heading)
{
    std::vector<std::string> out;
    for (const std::string& payload : payloads)
        if (payload.find(heading) != std::string::npos)
            out.push_back(payload);
    return out;
}

[[nodiscard]] std::string dump(const std::vector<std::string>& payloads)
{
    std::string out{std::to_string(payloads.size()) + " record(s):"};
    for (const std::string& payload : payloads)
        out += "\n  " + payload;
    return out;
}

[[nodiscard]] std::string describe(const CanonicalEvent& event)
{
    return "event format=" + std::string{insight::to_string(event.format)} + " template=\"" +
           std::string{event.template_str} + "\" no_role_witness_key=\"" +
           std::string{event.no_role_witness_key} + "\"";
}

} // namespace

// invariant: the syslog probe projects to empty content, and its warning names no field of it.
TEST(DiagnosticsQuoteNoInput, EmptyProjectionWarningQuotesNoComponent)
{
    const AttachedCapture capture{insight::logging::tokenizer_logger()};
    Door door;
    const std::expected<CanonicalEvent, std::string> event{
        door.tokenizer.process_line(kSyslogProbe)};
    ASSERT_TRUE(event.has_value()) << "the probe yields no event: " << event.error();

    const std::vector<std::string> warnings{matching(capture.payloads(), kEmptyProjectionHeading)};
    ASSERT_EQ(warnings.size(), 1U)
        << "the probe '" << kSyslogProbe
        << "' must raise exactly one empty-projection warning, or this arm measures nothing; "
        << describe(*event) << "; " << dump(capture.payloads());
    EXPECT_EQ(warnings.front().find(kSyslogSecret), std::string::npos)
        << "the warning quotes the input's program name '" << kSyslogSecret
        << "': " << warnings.front();
}

// invariant: the role-less JSON probe is marked, and its warning gives the key count, never a key.
// invariant: it runs on a fresh thread because the warning's rate limit is thread-local, so the
// probe is that thread's first role-less record and is always reported.
TEST(DiagnosticsQuoteNoInput, RolelessJsonWarningQuotesNoKey)
{
    const AttachedCapture capture{insight::logging::strategy_logger()};
    std::expected<CanonicalEvent, std::string> event{std::unexpected{std::string{"never run"}}};
    {
        std::jthread probe{[&event]
                           {
                               Door door;
                               event = door.tokenizer.process_line(kJsonProbe);
                           }};
    }
    ASSERT_TRUE(event.has_value()) << "the probe yields no event: " << event.error();
    ASSERT_EQ(event->format, insight::LogFormat::JSON)
        << "the probe did not route to JSON: " << insight::to_string(event->format);

    const std::vector<std::string> warnings{matching(capture.payloads(), kRolelessHeading)};
    ASSERT_EQ(warnings.size(), 1U)
        << "the probe '" << kJsonProbe
        << "' must raise exactly one role-less warning on its fresh thread, or this arm measures "
           "nothing; "
        << describe(*event) << "; " << dump(capture.payloads());
    EXPECT_EQ(warnings.front().find(kJsonSecretKey), std::string::npos)
        << "the warning quotes the input key '" << kJsonSecretKey << "': " << warnings.front();
    EXPECT_EQ(warnings.front().find(kJsonSecretOtherKey), std::string::npos)
        << "the warning quotes the input key '" << kJsonSecretOtherKey << "': " << warnings.front();
    EXPECT_NE(warnings.front().find("Top-level keys: 2 "), std::string::npos)
        << "the warning does not give the probe's key count of 2: " << warnings.front();
}
