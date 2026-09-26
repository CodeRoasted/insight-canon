// invariant: the projection renders EVERY CanonicalEvent member into its own column, and a column's
// text is a function of that member alone — the property the cut's generation gate digests.
// invariant: escaping keeps every column free of the separators the proof and the digest use.
// refs: DN-108.D24, F-SRC-insight-canon:canon.api.cppm:kProjectionMembers
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogFormat;
using insight::LogLevel;
using insight::OrdinalObservation;
using insight::OrdinalSchedule;
using insight::SpanId;
using insight::StructuralRole;
using insight::TraceId;
using insight::tokenization::CanonicalEvent;
using insight::tokenization::kProjectionMembers;
using insight::tokenization::ProjectionColumns;
using insight::tokenization::render_projection;

namespace
{

std::size_t column(std::string_view name)
{
    for (std::size_t idx{0}; idx < kProjectionMembers.size(); ++idx)
        if (kProjectionMembers[idx] == name)
            return idx;
    ADD_FAILURE() << "no projection member named " << name;
    return 0;
}

constexpr std::int64_t kMicros{1'700'000'000'123'456};
constexpr std::int64_t kSubMicroNanos{789};

} // namespace

TEST(Projection, RendersEveryMemberInItsOwnColumn)
{
    const std::array<std::string_view, 2> params{"a|b", "c\td"};
    const std::array<OrdinalObservation, 1> ordinals{
        {{.field_name = "latency_ms", .schedule = OrdinalSchedule::DurationLog2Ns, .value = -42}}};
    const std::array<SpanId, 2> linked{{{.value = 7}, {.value = 9}}};
    CanonicalEvent event{};
    event.id = 12;
    event.timestamp = insight::Timestamp{std::chrono::duration_cast<insight::Duration>(
        std::chrono::microseconds{kMicros} + std::chrono::nanoseconds{kSubMicroNanos})};
    event.declared_timestamp = true;
    event.level = LogLevel::Warn;
    event.declared_level = true;
    event.format = LogFormat::JSON;
    event.component = "svc\\core";
    event.host = "node\r1";
    event.template_str = "line\nbreak <*>";
    event.params = params;
    event.structural_role = StructuralRole::None;
    event.trace = {.present = true,
                   .has_parent = true,
                   .is_span = false,
                   .trace_id = TraceId{.value = 3},
                   .span_id = SpanId{.value = 4},
                   .parent_span_id = SpanId{.value = 5}};
    event.ordinals = ordinals;
    event.linked_span_ids = linked;
    event.echoed_source = true;
    event.no_role_witness_key = "k";

    ProjectionColumns out;
    out.fill("stale");
    render_projection(event, out);

    const std::array<std::pair<std::string_view, std::string_view>, kProjectionMembers.size()> want{
        {
            {"id", "12"},
            {"timestamp", "1700000000123456"},
            {"declared_timestamp", "1"},
            {"level", "Warn"},
            {"declared_level", "1"},
            {"format", "JSON"},
            {"component", "svc\\\\core"},
            {"host", "node\\r1"},
            {"template_str", "line\\nbreak <*>"},
            {"params", "2|a\\|b|c\\td"},
            {"structural_role", std::string_view{insight::to_string(StructuralRole::None)}},
            {"trace", "1,1,0,3,4,5"},
            {"ordinals", "1|latency_ms,dur-log2-ns-v1,-42"},
            {"linked_span_ids", "2|7|9"},
            {"echoed_source", "1"},
            {"no_role_witness_key", "k"},
        }};
    for (const auto& [name, text] : want)
        EXPECT_EQ(out[column(name)], text) << "member " << name;
}

TEST(Projection, EmptyListsAndDefaultsRenderTheirCountAndZero)
{
    ProjectionColumns out;
    render_projection(CanonicalEvent{}, out);
    EXPECT_EQ(out[column("params")], "0");
    EXPECT_EQ(out[column("ordinals")], "0");
    EXPECT_EQ(out[column("linked_span_ids")], "0");
    EXPECT_EQ(out[column("trace")], "0,0,0,0,0,0");
    EXPECT_EQ(out[column("timestamp")], "0");
    EXPECT_EQ(out[column("level")], "Unknown");
}
