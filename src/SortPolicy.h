// SortPolicy.h -- the order reminders appear in on screen.
//
// Pure logic, no Arduino, so the rule can be unit-tested. It was previously
// inline in MemoStore.cpp and fenced behind a panel-mode #if in MemoUI, which
// meant the compact panels never sorted at all and the order only ever looked
// correct by accident.
//
// The rule, in one sentence: pending reminders in ascending date/time order,
// completed ones always last.

#ifndef VOICE_MEMO_SORT_POLICY_H
#define VOICE_MEMO_SORT_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Sort key for an entry with no due time. Large enough to sink it below every
// real date, so undated reminders trail the dated ones instead of leading
// them (a reminder with no time is the least urgent, not the most).
static const int64_t kVmNoDueSortKey = 0x7FFFFFFFLL;

// The fields the order depends on. A struct keeps the comparator testable
// without dragging in Arduino's String.
struct VmSortItem {
  bool   done;
  time_t doneAt;
  bool   hasDue;
  time_t dueEpoch;
};

static inline int vmSortBucket(const struct VmSortItem* e)
{
  return e->done ? 1 : 0;   // 0 = pending, 1 = done (always last)
}

static inline int64_t vmSortKey(const struct VmSortItem* e)
{
  return e->hasDue ? (int64_t)e->dueEpoch : kVmNoDueSortKey;
}

// True when `a` must be drawn above `b`.
//
// Pending entries are strictly chronological, earliest first -- so an overdue
// reminder rises to the TOP, which is the point: the thing already missed is
// the most urgent, not the least. Completed entries sit below every pending
// one, most recently completed first, which leaves the one closest to being
// purged at the very bottom.
static inline bool vmSortBefore(const struct VmSortItem* a,
                                const struct VmSortItem* b)
{
  const int ba = vmSortBucket(a);
  const int bb = vmSortBucket(b);
  if (ba != bb) return ba < bb;
  if (ba == 1) return a->doneAt > b->doneAt;
  return vmSortKey(a) < vmSortKey(b);
}

#endif  // VOICE_MEMO_SORT_POLICY_H
