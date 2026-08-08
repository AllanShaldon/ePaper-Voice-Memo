// VoiceMemoApp.h -- application orchestrator.
//
// The application is just glue: it owns one instance of each module and
// drives the recording lifecycle from the KEY0 button, plus a touch-driven
// checkbox interaction for marking reminders done.
//
// Adding a brand new behavior usually means editing ONE module, not this
// file:
//   - new STT provider  -> SpeechClient
//   - new memo model    -> MemoClient
//   - new screen        -> MemoUI
//   - new RTC           -> RtcClock
//   - new persistence   -> MemoStore
//   - new audio codec   -> AudioCapture
//   - new touch chip    -> TouchInput
//
// If you find yourself adding state here for a single module, that state
// probably belongs inside the module instead.

#ifndef VOICE_MEMO_APP_H
#define VOICE_MEMO_APP_H

#include <Arduino.h>

#include "AudioCapture.h"
#include "DailyQuoteClient.h"
#include "MemoClient.h"
#include "MemoStore.h"
#include "MemoUI.h"
#include "RtcClock.h"
#include "SpeechClient.h"
#include "TouchInput.h"

struct AudioConfig {
  uint32_t sampleRate;
  uint32_t maxRecordSeconds;
};

// Top-level user configuration filled in VoiceMemoReminder.ino. Sub-structs
// live in the module that owns them so a contributor can read SpeechClient.h
// to find every speech-related field in one place.
struct VoiceMemoConfig {
  const char*  wifiSsid;
  const char*  wifiPassword;
  SpeechConfig speech;
  MemoConfig   memo;
  AudioConfig  audio;
  uint32_t     httpTimeoutMs;
};

class VoiceMemoApp {
 public:
  explicit VoiceMemoApp(const VoiceMemoConfig& config);

  void begin();
  void loop();

 private:
  // Fixed reTerminal E-series pins. Model-specific LED pin lives in driver.h
  // because it changes between E1001/E1002/E1003.
  static constexpr int kSerialRxPin    = 44;
  static constexpr int kSerialTxPin    = 43;
  static constexpr int kKey0Pin        = 3;   // right  -- hold records, click completes
  static constexpr int kKey1Pin        = 4;   // middle -- selection down
  static constexpr int kKey2Pin        = 5;   // left   -- selection up
  static constexpr int kMicClkPin      = 42;
  static constexpr int kMicDataPin     = 41;
  static constexpr int kMicPwrEnPin    = 38;
  static constexpr int kI2cSdaPin      = 19;
  static constexpr int kI2cSclPin      = 20;
  static constexpr int kTouchIntPin    = 2;
  static constexpr int kTouchResetPin  = 48;
  static constexpr int kBuzzerPin      = 45;

  // Battery sense pins are device-specific; see driver.h.
  // 电池检测引脚按设备型号区分，见 driver.h。
  static constexpr int kBatteryEnablePin = VM_BATTERY_ENABLE_PIN;
  static constexpr int kBatteryAdcPin    = 1;

  static constexpr unsigned long kDebounceDelayMs = 35;

  // A KEY0 press shorter than this is a CLICK (complete the selected card),
  // not a recording. Measured on the hardware: deliberate taps land at
  // 0.4-0.7 s, and nothing useful is ever said in under 0.8 s. Note this is a
  // BUTTON-HOLD threshold, deliberately not AudioCapture::tooShort(), which
  // only triggers below 1/3 s and would let a tap through as a recording.
  static constexpr unsigned long kClickMaxMs = 800;
  static constexpr unsigned long kListRefreshMs = 5UL * 60UL * 1000UL;

  const VoiceMemoConfig config_;

  AudioCapture audio_;
  RtcClock     rtc_;
  MemoStore    store_;
  SpeechClient stt_;
  MemoClient   memo_;
  DailyQuoteClient quote_;
  MemoUI       ui_;
  TouchInput   touch_;

  // Button + recording state machine.
  bool          recording_;
  bool          busy_;
  bool          lastRawButton_;
  bool          stableButton_;
  unsigned long pressStartMs_ = 0;

  // Selection cursor for the non-touch panels. -1 = nothing selected, which is
  // the state the device boots into so a stray KEY0 click cannot complete a
  // reminder the user never pointed at.
  int           selectedIndex_ = -1;
  // First list index drawn on screen. The panel shows VM_VISIBLE_MEMO_MAX
  // cards at a time while the store holds MemoStore::kMax.
  int           scrollOffset_ = 0;
  // Debounce state for the two navigation keys, same shape as KEY0's.
  bool          lastRawKey1_ = HIGH;
  bool          stableKey1_  = HIGH;
  unsigned long debounceKey1Ms_ = 0;
  bool          lastRawKey2_ = HIGH;
  bool          stableKey2_  = HIGH;
  unsigned long debounceKey2Ms_ = 0;
  bool          ledState_;
  unsigned long debounceMs_;
  unsigned long lastBlinkMs_;
  unsigned long lastListRefreshMs_;

  void setupPins();
  void ledOn();
  void ledOff();
  void beepStart();
  bool ensureWiFi(uint32_t timeoutMs);
  int      readBatteryPercent();
  UiStatus currentStatus(bool processing);

  void pollButton();
  // Ends a recording without uploading it -- used when a press turns out to
  // have been a click. The next startRecord() resets the buffer, so the
  // captured samples need no explicit discard.
  void abortRecording();
  void pollNavButtons();
  void pollTouch();
  // Moves the selection cursor by delta over the visible cards, clamping at
  // both ends, and redraws. No-op while recording or busy.
  void moveSelection(int delta);
  // Keeps scrollOffset_ inside the list and the selection inside the window.
  void clampScroll();
  // Completes / un-completes the selected card. No-op when nothing is selected.
  void toggleSelected();
  void pollScheduledRefresh();
  void startRecording();
  void stopRecording(bool forced);
  void captureChunk();
  void drawTodoList(const String& hint, bool processing, bool allowQuoteNetwork);
};

#endif  // VOICE_MEMO_APP_H
