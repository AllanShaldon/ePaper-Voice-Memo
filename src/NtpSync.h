// NtpSync.h -- network time source.
//
// The PCF8563 on the reTerminal is the only clock the UI reads, but nothing
// ever corrected it: the firmware seeded it from the build time, and that seed
// only fires when the chip is BEHIND the build (see ClockSync.h). A clock that
// runs ahead -- or a board flashed from a machine in another zone -- stays
// wrong forever.
//
// This module asks SNTP for UTC whenever WiFi is up and hands the epoch back.
// Converting to local time and writing the chip belongs to RtcClock.
//
// The device has no timezone database and no DST rules; a fixed offset is
// enough for America/Fortaleza (UTC-3, no DST). Override at build time with
// -D VM_TZ_OFFSET_SECONDS=<seconds> for another zone.

#ifndef VOICE_MEMO_NTP_SYNC_H
#define VOICE_MEMO_NTP_SYNC_H

#include <Arduino.h>
#include <time.h>

#ifndef VM_TZ_OFFSET_SECONDS
#define VM_TZ_OFFSET_SECONDS (-3 * 3600)
#endif

// Blocks up to timeoutMs waiting for SNTP. Returns a UTC epoch, or 0 when the
// network is down or no server answered in time. Never returns a partially
// valid time: a non-zero result is trustworthy.
time_t vmFetchNtpUtcEpoch(uint32_t timeoutMs);

#endif  // VOICE_MEMO_NTP_SYNC_H
