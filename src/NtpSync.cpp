#include "NtpSync.h"

#include <WiFi.h>

namespace {

// Anything before 2024-01-01 means SNTP has not answered yet: the ESP32 boots
// its system clock at epoch 0 and configTime() does not block.
constexpr time_t kSaneEpochFloor = 1704067200;

bool sntpStarted = false;

}  // namespace

time_t vmFetchNtpUtcEpoch(uint32_t timeoutMs)
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial1.println("[ntp] skipped: no wifi");
    return 0;
  }

  if (!sntpStarted) {
    // Offset 0: plain UTC here. The local offset is applied once, by the
    // caller, when writing the RTC -- so no other module's notion of time
    // shifts underneath it.
    configTime(0, 0, "a.st1.ntp.br", "pool.ntp.org", "time.google.com");
    sntpStarted = true;
  }

  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    const time_t utc = time(nullptr);
    if (utc >= kSaneEpochFloor) {
      Serial1.printf("[ntp] utc epoch %lld after %lu ms\n",
                     static_cast<long long>(utc),
                     static_cast<unsigned long>(millis() - start));
      return utc;
    }
    delay(200);
  }

  Serial1.printf("[ntp] no answer in %lu ms\n",
                 static_cast<unsigned long>(timeoutMs));
  return 0;
}
