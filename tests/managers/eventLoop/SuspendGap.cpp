#include <managers/eventLoop/EventLoopManager.hpp>

#include <gtest/gtest.h>

// CEventLoopManager::suspendGapMs derives the suspended time from the divergence
// between CLOCK_BOOTTIME (counts during suspend) and CLOCK_MONOTONIC (frozen).

TEST(EventLoop, suspendGapNoneWhenClocksTrackTogether) {
    EXPECT_EQ(CEventLoopManager::suspendGapMs(30000, 30000, 5000), 0u);
}

TEST(EventLoop, suspendGapIgnoresJitterUnderThreshold) {
    EXPECT_EQ(CEventLoopManager::suspendGapMs(30120, 30000, 5000), 0u);
}

TEST(EventLoop, suspendGapDetectsLongSuspend) {
    EXPECT_EQ(CEventLoopManager::suspendGapMs(3630000, 30000, 5000), 3600000u);
}

TEST(EventLoop, suspendGapThresholdIsInclusive) {
    EXPECT_EQ(CEventLoopManager::suspendGapMs(34999, 30000, 5000), 0u);
    EXPECT_EQ(CEventLoopManager::suspendGapMs(35000, 30000, 5000), 5000u);
}

TEST(EventLoop, suspendGapMonotonicNeverExceedsBoot) {
    EXPECT_EQ(CEventLoopManager::suspendGapMs(30000, 30001, 5000), 0u);
}
