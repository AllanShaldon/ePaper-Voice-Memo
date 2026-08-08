#include <unity.h>
#include <string.h>
#include "SortPolicy.h"

static const time_t T = 1786200000;

static VmSortItem pending(time_t due)
{
    VmSortItem e; memset(&e, 0, sizeof(e));
    e.done = false; e.hasDue = true; e.dueEpoch = due;
    return e;
}

static VmSortItem pendingNoDue()
{
    VmSortItem e; memset(&e, 0, sizeof(e));
    e.done = false; e.hasDue = false;
    return e;
}

static VmSortItem done(time_t due, time_t doneAt)
{
    VmSortItem e; memset(&e, 0, sizeof(e));
    e.done = true; e.hasDue = true; e.dueEpoch = due; e.doneAt = doneAt;
    return e;
}

// Insertion sort mirroring MemoStore::sortByDue, so the test exercises the
// ordering as a whole and not just one comparison.
static void sortItems(VmSortItem* v, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        VmSortItem key = v[i];
        size_t j = i;
        while (j > 0 && vmSortBefore(&key, &v[j - 1])) { v[j] = v[j - 1]; j--; }
        v[j] = key;
    }
}

void test_pending_sorted_ascending_by_due()
{
    VmSortItem v[] = { pending(T + 300), pending(T + 100), pending(T + 200) };
    sortItems(v, 3);
    TEST_ASSERT_EQUAL_INT64(T + 100, v[0].dueEpoch);
    TEST_ASSERT_EQUAL_INT64(T + 200, v[1].dueEpoch);
    TEST_ASSERT_EQUAL_INT64(T + 300, v[2].dueEpoch);
}

void test_overdue_rises_to_the_top()
{
    // The reminder already missed is the most urgent, so it leads the list.
    VmSortItem v[] = { pending(T + 100), pending(T - 5000), pending(T + 50) };
    sortItems(v, 3);
    TEST_ASSERT_EQUAL_INT64(T - 5000, v[0].dueEpoch);
}

void test_done_always_below_pending()
{
    // The done entry is due EARLIEST, so only the bucket can push it down.
    VmSortItem v[] = { done(T - 9000, T), pending(T + 100), pending(T + 200) };
    sortItems(v, 3);
    TEST_ASSERT_FALSE(v[0].done);
    TEST_ASSERT_FALSE(v[1].done);
    TEST_ASSERT_TRUE(v[2].done);
}

void test_done_ordered_newest_completion_first()
{
    // Oldest completion sits at the very bottom: it is the next to be purged.
    VmSortItem v[] = { done(T, T - 100), done(T, T - 900), done(T, T - 500) };
    sortItems(v, 3);
    TEST_ASSERT_EQUAL_INT64(T - 100, v[0].doneAt);
    TEST_ASSERT_EQUAL_INT64(T - 500, v[1].doneAt);
    TEST_ASSERT_EQUAL_INT64(T - 900, v[2].doneAt);
}

void test_undated_trails_dated_but_leads_done()
{
    VmSortItem v[] = { pendingNoDue(), done(T, T), pending(T + 9999999) };
    sortItems(v, 3);
    TEST_ASSERT_TRUE(v[0].hasDue);      // dated pending first
    TEST_ASSERT_FALSE(v[1].hasDue);     // undated pending next
    TEST_ASSERT_TRUE(v[2].done);        // done last
}

void test_reported_case_from_the_device()
{
    // The exact list from the photo that exposed the bug: two completed
    // reminders sat between two pending ones.
    VmSortItem v[] = {
        pending(T + 6000),          // 15:00
        done(T + 300, T + 10),      // 13:30, completed
        done(T + 20000, T + 20),    // 19:00, completed
        pending(T + 17000),         // 18:00
    };
    sortItems(v, 4);
    TEST_ASSERT_FALSE(v[0].done);
    TEST_ASSERT_EQUAL_INT64(T + 6000, v[0].dueEpoch);   // 15:00
    TEST_ASSERT_FALSE(v[1].done);
    TEST_ASSERT_EQUAL_INT64(T + 17000, v[1].dueEpoch);  // 18:00
    TEST_ASSERT_TRUE(v[2].done);
    TEST_ASSERT_TRUE(v[3].done);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_pending_sorted_ascending_by_due);
    RUN_TEST(test_overdue_rises_to_the_top);
    RUN_TEST(test_done_always_below_pending);
    RUN_TEST(test_done_ordered_newest_completion_first);
    RUN_TEST(test_undated_trails_dated_but_leads_done);
    RUN_TEST(test_reported_case_from_the_device);
    return UNITY_END();
}
