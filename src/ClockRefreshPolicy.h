// ClockRefreshPolicy.h -- when the e-paper repaints just to keep the clock right.
//
// Measured 2026-10-10: the list repainted every 5 minutes, so the clock on the
// screen was the time of the LAST repaint -- up to ~5 minutes behind and
// jumping in 5-minute steps, while the RTC itself was exact (NTP). Now it
// repaints on every minute boundary: a partial pass (fast, no flash), and once
// an hour a full pass, because partial refreshes leave residue on this panel.
// Pure C, unit-tested on the host (test/test_clock_refresh).

#ifndef VOICE_MEMO_CLOCK_REFRESH_POLICY_H
#define VOICE_MEMO_CLOCK_REFRESH_POLICY_H

#include <stdbool.h>
#include <time.h>

// Minute index of a (local) epoch; what the screen's HH:MM shows.
static inline long long vmMinuteOf(time_t epoch)
{
  return epoch > 0 ? (long long)(epoch / 60) : -1;
}

// Repaint for the clock? Only when the minute on screen is no longer "now".
// A repaint for any other reason in the same minute already updated it.
static inline bool vmClockNeedsRepaint(long long lastDrawnMinute, time_t now)
{
  if (now <= 0) return false;              // no clock yet: nothing to show
  return vmMinuteOf(now) != lastDrawnMinute;
}

// Full (clean) pass on the hour, partial on every other minute.
static inline bool vmClockRepaintIsFull(time_t now)
{
  return now > 0 && ((now / 60) % 60) == 0;
}

#endif  // VOICE_MEMO_CLOCK_REFRESH_POLICY_H
