// DonePolicy.h -- when a completed reminder stops being shown.
//
// Pure logic, no Arduino: a completed reminder is not deleted on the spot. It
// stays visible (struck through, sorted to the bottom) so the list still reads
// as "I did that", and only disappears after kVmDoneTtlSeconds. Extracted into
// a header so the rule is unit-testable -- the alternative is waiting twelve
// hours in front of the device to find out whether it works.

#ifndef VOICE_MEMO_DONE_POLICY_H
#define VOICE_MEMO_DONE_POLICY_H

#include <stdbool.h>
#include <time.h>

// How long a completed reminder lingers before it is purged. 5 min is long
// enough to read as "I just did that" and see the strikethrough land, and
// short enough that the list stays about what is still pending.
//
// Note this is short enough that the REDRAW cadence matters: the purge runs
// during a repaint, so without a housekeeping tick the entry would survive
// until the next scheduled refresh. See VoiceMemoApp::pollDueAlarm().
static const time_t kVmDoneTtlSeconds = 5 * 60;

// True when a completed reminder has outlived its grace period.
//
// Guards, each protecting against a way this silently eats data:
//   - not done            -> never purge; only completed work ages out.
//   - doneAt <= 0         -> no stamp (entry written by an older firmware),
//                            so its age is unknown; keep it rather than guess.
//   - now <= 0            -> clock not yet valid. At boot the RTC can read
//                            near zero, which would make (now - doneAt) look
//                            enormous and wipe reminders completed seconds ago.
//   - doneAt > now        -> stamp in the future (clock moved backwards);
//                            treat as fresh instead of purging immediately.
static inline bool vmDoneExpired(bool done, time_t doneAt, time_t now,
                                 time_t ttlSeconds)
{
  if (!done) return false;
  if (doneAt <= 0) return false;
  if (now <= 0) return false;
  if (doneAt > now) return false;
  return (now - doneAt) >= ttlSeconds;
}

#endif  // VOICE_MEMO_DONE_POLICY_H
