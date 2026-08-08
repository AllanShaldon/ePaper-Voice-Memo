#include <unity.h>
#include "DonePolicy.h"

static const time_t kTtl = 5 * 60;         // 5 min
static const time_t kNow = 1786200000;     // arbitrary sane epoch

void test_undone_never_expires()
{
    // The whole point: a reminder you have not completed must never vanish.
    TEST_ASSERT_FALSE(vmDoneExpired(false, kNow - kTtl * 10, kNow, kTtl));
}

void test_done_just_now_survives()
{
    TEST_ASSERT_FALSE(vmDoneExpired(true, kNow, kNow, kTtl));
}

void test_done_one_second_before_ttl_survives()
{
    TEST_ASSERT_FALSE(vmDoneExpired(true, kNow - kTtl + 1, kNow, kTtl));
}

void test_done_exactly_at_ttl_expires()
{
    TEST_ASSERT_TRUE(vmDoneExpired(true, kNow - kTtl, kNow, kTtl));
}

void test_done_long_past_ttl_expires()
{
    TEST_ASSERT_TRUE(vmDoneExpired(true, kNow - kTtl * 3, kNow, kTtl));
}

void test_missing_stamp_never_expires()
{
    // Entry from a firmware that did not record doneAt: age unknown, so keep
    // it rather than delete on a guess.
    TEST_ASSERT_FALSE(vmDoneExpired(true, 0, kNow, kTtl));
}

void test_invalid_clock_never_expires()
{
    // At boot the RTC can read near zero. Without this guard (now - doneAt)
    // underflows into a huge number and wipes reminders completed seconds ago.
    TEST_ASSERT_FALSE(vmDoneExpired(true, kNow - kTtl * 3, 0, kTtl));
}

void test_stamp_in_the_future_never_expires()
{
    // Clock moved backwards (RTC reseed). Treat the entry as fresh.
    TEST_ASSERT_FALSE(vmDoneExpired(true, kNow + 3600, kNow, kTtl));
}

void test_ttl_is_five_minutes()
{
    TEST_ASSERT_EQUAL_INT(300, (int)kVmDoneTtlSeconds);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_undone_never_expires);
    RUN_TEST(test_done_just_now_survives);
    RUN_TEST(test_done_one_second_before_ttl_survives);
    RUN_TEST(test_done_exactly_at_ttl_expires);
    RUN_TEST(test_done_long_past_ttl_expires);
    RUN_TEST(test_missing_stamp_never_expires);
    RUN_TEST(test_invalid_clock_never_expires);
    RUN_TEST(test_stamp_in_the_future_never_expires);
    RUN_TEST(test_ttl_is_five_minutes);
    return UNITY_END();
}
