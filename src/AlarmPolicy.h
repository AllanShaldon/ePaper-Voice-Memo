// AlarmPolicy.h -- when a reminder should sound the buzzer.
//
// Pure logic, no Arduino, so the rule is unit-testable: an alarm that fires
// twice, or never, is the kind of bug you only notice by missing something.
//
// The device keeps a single "already announced up to" watermark instead of a
// per-reminder flag. That avoids another field in the persisted blob (and the
// version bump that wipes everyone's list), and it answers the question that
// actually matters: has this due time passed since the last time we looked?

#ifndef VOICE_MEMO_ALARM_POLICY_H
#define VOICE_MEMO_ALARM_POLICY_H

#include <stdbool.h>
#include <time.h>

// True when this reminder just came due and has not been announced yet.
//
// Guards, each earning its place:
//   - no due time / already done -> nothing to announce.
//   - dueEpoch > now             -> still in the future.
//   - dueEpoch <= watermark      -> already announced in an earlier pass.
//                                   Without this the buzzer would fire on
//                                   every poll for as long as the reminder
//                                   stays overdue, which is every 5 seconds.
//   - now <= 0                   -> clock not valid yet (fresh boot). Alarms
//                                   are suppressed rather than fired against
//                                   a meaningless timestamp.
static inline bool vmShouldAlert(bool hasDue, bool done, time_t dueEpoch,
                                 time_t watermark, time_t now)
{
  if (!hasDue || done) return false;
  if (now <= 0) return false;
  if (dueEpoch > now) return false;
  return dueEpoch > watermark;
}

// The watermark to persist after a scan. Always `now`: everything due up to
// this instant has been considered, whether or not anything actually rang.
//
// Seeding it with `now` on first boot (rather than 0) is what stops a device
// powering up next to a list of old reminders from playing every one of them
// back at once.
static inline time_t vmNextWatermark(time_t now, time_t previous)
{
  return (now > previous) ? now : previous;
}

#endif  // VOICE_MEMO_ALARM_POLICY_H
