#include "util/log.hpp"

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

}  // namespace
}  // namespace manifold
