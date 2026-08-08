#include <unity.h>
#include "AlarmPolicy.h"

static const time_t NOW  = 1786200000;
static const time_t MARK = NOW - 600;   // last scan, ten minutes ago

void test_due_since_last_scan_rings()
{
    TEST_ASSERT_TRUE(vmShouldAlert(true, false, NOW - 60, MARK, NOW));
}

void test_due_exactly_now_rings()
{
    TEST_ASSERT_TRUE(vmShouldAlert(true, false, NOW, MARK, NOW));
}

void test_future_does_not_ring()
{
    TEST_ASSERT_FALSE(vmShouldAlert(true, false, NOW + 60, MARK, NOW));
}

void test_already_announced_does_not_ring_again()
{
    // The one that matters: without this the buzzer fires on every poll for
    // as long as the reminder stays overdue.
    TEST_ASSERT_FALSE(vmShouldAlert(true, false, MARK - 60, MARK, NOW));
}

void test_done_never_rings()
{
    TEST_ASSERT_FALSE(vmShouldAlert(true, true, NOW - 60, MARK, NOW));
}

void test_no_due_time_never_rings()
{
    TEST_ASSERT_FALSE(vmShouldAlert(false, false, NOW - 60, MARK, NOW));
}

void test_invalid_clock_never_rings()
{
    TEST_ASSERT_FALSE(vmShouldAlert(true, false, NOW - 60, MARK, 0));
}

void test_watermark_advances_to_now()
{
    TEST_ASSERT_EQUAL_INT64(NOW, vmNextWatermark(NOW, MARK));
}

void test_watermark_never_goes_backwards()
{
    // Clock reseeded backwards: keep the old mark, or reminders already
    // announced would ring a second time.
    TEST_ASSERT_EQUAL_INT64(MARK, vmNextWatermark(MARK - 5000, MARK));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_due_since_last_scan_rings);
    RUN_TEST(test_due_exactly_now_rings);
    RUN_TEST(test_future_does_not_ring);
    RUN_TEST(test_already_announced_does_not_ring_again);
    RUN_TEST(test_done_never_rings);
    RUN_TEST(test_no_due_time_never_rings);
    RUN_TEST(test_invalid_clock_never_rings);
    RUN_TEST(test_watermark_advances_to_now);
    RUN_TEST(test_watermark_never_goes_backwards);
    return UNITY_END();
}
