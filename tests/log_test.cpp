#include "util/log.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(LogTest, MessagesArePrefixedWithLevel) {
    testing::internal::CaptureStderr();
    log_info("generated in {} attempts", 2);
    log_warning("slow");
    log_error("failed: {}", "reason");
    EXPECT_EQ(testing::internal::GetCapturedStderr(),
              "[info] generated in 2 attempts\n[warning] slow\n[error] failed: reason\n");
}

TEST(LogTest, ListenerReceivesMessagesUntilRemoved) {
    std::vector<std::pair<LogLevel, std::string>> received;
    set_log_listener([&](LogLevel level, std::string_view message) { received.emplace_back(level, message); });
    testing::internal::CaptureStderr();
    log_warning("slow {}", 1);
    set_log_listener({});
    log_info("not received");
    testing::internal::GetCapturedStderr();
    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received[0].first, LogLevel::kWarning);
    EXPECT_EQ(received[0].second, "slow 1");
}

}  // namespace
}  // namespace manifold
