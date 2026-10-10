#include <unity.h>
#include "ClockRefreshPolicy.h"

// 2026-10-10 11:07:29 local wall clock as epoch (the RTC's representation).
static const time_t k1107 = 1791630449;

void test_same_minute_does_not_repaint()
{
  const long long drawn = vmMinuteOf(k1107);
  TEST_ASSERT_FALSE(vmClockNeedsRepaint(drawn, k1107 + 10));   // 11:07:39
}

void test_next_minute_repaints()
{
  const long long drawn = vmMinuteOf(k1107);
  TEST_ASSERT_TRUE(vmClockNeedsRepaint(drawn, k1107 + 31));    // 11:08:00
}

void test_never_drawn_repaints_once_clock_exists()
{
  TEST_ASSERT_TRUE(vmClockNeedsRepaint(-1, k1107));
  TEST_ASSERT_FALSE(vmClockNeedsRepaint(-1, 0));               // RTC not set: nothing to show
}

void test_full_pass_only_on_the_hour()
{
  const time_t onze = k1107 - 7 * 60 - 29;                     // 11:00:00
  TEST_ASSERT_TRUE(vmClockRepaintIsFull(onze));
  TEST_ASSERT_TRUE(vmClockRepaintIsFull(onze + 59));           // still 11:00
  TEST_ASSERT_FALSE(vmClockRepaintIsFull(onze + 60));          // 11:01 partial
  TEST_ASSERT_FALSE(vmClockRepaintIsFull(k1107));
}

int main(int, char**)
{
  UNITY_BEGIN();
  RUN_TEST(test_same_minute_does_not_repaint);
  RUN_TEST(test_next_minute_repaints);
  RUN_TEST(test_never_drawn_repaints_once_clock_exists);
  RUN_TEST(test_full_pass_only_on_the_hour);
  return UNITY_END();
}
