#include "VoiceMemoApp.h"

#include "AlarmPolicy.h"

#include <WiFi.h>

#include "BatteryMath.h"
#include "UiLang.h"

VoiceMemoApp::VoiceMemoApp(const VoiceMemoConfig& config)
  : config_(config),
    audio_(),
    rtc_(),
    store_(),
    stt_(),
    memo_(),
    quote_(),
    ui_(),
    touch_(),
    recording_(false),
    busy_(false),
    lastRawButton_(HIGH),
    stableButton_(HIGH),
    ledState_(false),
    debounceMs_(0),
    lastBlinkMs_(0),
    lastListRefreshMs_(0)
{
}

void VoiceMemoApp::ledOn()  { digitalWrite(VM_LED_PIN, LOW); }
void VoiceMemoApp::ledOff() { digitalWrite(VM_LED_PIN, HIGH); }

void VoiceMemoApp::beepStart()
{
  // Short, brighter beep at recording start.
  // 短促且更明显的录音开始提示音。
  tone(kBuzzerPin, 2500, 180);
}

void VoiceMemoApp::setupPins()
{
  pinMode(VM_LED_PIN, OUTPUT);
  ledOff();
  pinMode(kKey0Pin, INPUT);
  pinMode(kKey1Pin, INPUT_PULLUP);
  pinMode(kKey2Pin, INPUT_PULLUP);
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);

  pinMode(kBatteryEnablePin, OUTPUT);
  digitalWrite(kBatteryEnablePin, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
}

int VoiceMemoApp::readBatteryPercent()
{
  digitalWrite(kBatteryEnablePin, HIGH);
  delay(5);
  const int mv = analogReadMilliVolts(kBatteryAdcPin);
  digitalWrite(kBatteryEnablePin, LOW);
  return vmBatteryPercent(mv);
}

UiStatus VoiceMemoApp::currentStatus(bool processing)
{
  UiStatus s;
  s.wifiConnected  = (WiFi.status() == WL_CONNECTED);
  s.batteryPercent = readBatteryPercent();
  s.processing     = processing;
  return s;
}

void VoiceMemoApp::drawTodoList(const String& hint, bool processing,
                                bool allowQuoteNetwork, bool partial)
{
  // Partial refresh is a 1 bpp-only path, and it accumulates residue, so it is
  // used for cursor movement and given up periodically for a clean full pass.
  bool usePartial = partial && MemoUI::supportsPartial();
  if (usePartial && kMaxPartialsBeforeFull > 0 &&
      partialsSinceFull_ >= kMaxPartialsBeforeFull) {
    usePartial = false;
  }
  partialsSinceFull_ = usePartial ? (partialsSinceFull_ + 1) : 0;

  const time_t nowEpoch = rtc_.nowEpoch();
  // Completed reminders age out on their own after kDoneTtlSeconds. Doing it
  // here means every redraw is also a garbage-collection tick, so nothing
  // lingers just because the user never pressed anything.
  if (store_.purgeExpiredDone(nowEpoch)) {
    // Same reasoning as toggleSelected(): entries disappeared under the
    // cursor, so park it somewhere that certainly still exists rather than
    // leaving it pointing at a row that is gone.
    selectedIndex_ = (store_.count() > 0) ? 0 : -1;
    scrollOffset_ = 0;
  }
  clampScroll();
  bool quoteNetworkReady = false;
  if (allowQuoteNetwork && quote_.needsRefresh(nowEpoch)) {
    quoteNetworkReady = (WiFi.status() == WL_CONNECTED) || ensureWiFi(5000);
  }
  quote_.refreshIfNeeded(nowEpoch, quoteNetworkReady);
  ui_.drawTodoList(store_, rtc_, currentStatus(processing), hint, quote_.quote(),
                   selectedIndex_, scrollOffset_, usePartial);
  lastListRefreshMs_ = millis();
}

bool VoiceMemoApp::ensureWiFi(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(config_.wifiSsid, config_.wifiPassword);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial1.print(".");
  }
  Serial1.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial1.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial1.println("[wifi] connection failed");
  return false;
}

void VoiceMemoApp::begin()
{
  // Arduino startup order:
  //   1. Start the debug UART (Serial1 on GPIO43/44).
  //   2. Configure pins (LED + KEY0 + buzzer).
  //   3. Initialize the I2C real-time clock (also brings up the shared
  //      I2C bus that the touch controller uses).
  //   4. Configure the e-paper display and show the BOOT splash.
  //   5. Allocate the WAV buffer and start the PDM microphone.
  //   6. Probe the touch controller now that the bus exists and the panel
  //      size is known.
  //   7. Load persisted reminders; configure speech / memo clients.
  //   8. Try WiFi once, then draw the reminder list.
  Serial1.begin(115200, SERIAL_8N1, kSerialRxPin, kSerialTxPin);
  delay(500);

  setupPins();
  const bool rtcOk = rtc_.begin(kI2cSdaPin, kI2cSclPin);

  Serial1.println("=========================================");
  Serial1.println("  VoiceMemoReminder");
  Serial1.printf("  Device: %s\n", VM_DEVICE_NAME);
  Serial1.printf("  RTC:    %s\n", rtcOk ? "ok" : "unavailable");
  Serial1.println("=========================================");

  ui_.begin();
  ui_.drawBoot(rtc_, uiStr(UiStringId::kBootStarting), currentStatus(false));

  if (!audio_.begin(config_.audio.sampleRate, config_.audio.maxRecordSeconds,
                    kMicClkPin, kMicDataPin, kMicPwrEnPin)) {
    // TODO(i18n): hardware-fault diagnostics stay in English (contain code
    // identifiers like PSRAM / driver.h and surface only on a boot failure).
    ui_.drawStatus("ERR", "Mic failed",
                   "Audio buffer or PDM microphone init failed. Check OPI PSRAM and driver.h.",
                   "Board: XIAO ESP32S3, PSRAM: OPI PSRAM.", false, 0.0f);
    while (true) delay(1000);
  }

#if VM_HAS_TOUCH
  touch_.begin(kTouchIntPin, kTouchResetPin,
               ui_.displayWidth(), ui_.displayHeight());
#endif

  store_.begin();

  // Seed the alarm watermark. A device powering up next to a list of already
  // overdue reminders must not play every one of them back at once, so when
  // nothing was ever stored we start from "now" and only announce what comes
  // due from here on.
  alertWatermark_ = store_.alertWatermark();
  if (alertWatermark_ <= 0) {
    alertWatermark_ = rtc_.nowEpoch();
    store_.setAlertWatermark(alertWatermark_);
  }

  stt_.configure(config_.speech, config_.httpTimeoutMs);
  memo_.configure(config_.memo,  config_.httpTimeoutMs);
  quote_.configure(config_.memo, config_.httpTimeoutMs);
  quote_.begin(rtc_.nowEpoch());

  ui_.drawBoot(rtc_, uiStr(UiStringId::kBootWifi), currentStatus(false));
  ensureWiFi(15000);

  drawTodoList(uiStr(UiStringId::kHintAdd), false, true, /*partial=*/true);
}

void VoiceMemoApp::startRecording()
{
  if (busy_ || recording_) return;

  // CRITICAL: capture must begin IMMEDIATELY. An ePaper full refresh costs
  // ~1.5 s on E1003, but the I2S DMA ring can only buffer ~256 ms of audio.
  // If we drew a "REC" screen here, the first second of the user's speech
  // would be overwritten in DMA before captureChunk() ever ran. So we do
  // NOT touch the screen at the start of a recording -- the buzzer beep
  // and solid LED are the user feedback.
  audio_.startRecord();
  recording_ = true;
  beepStart();
  ledOn();

  Serial1.println("[rec] start");
}

void VoiceMemoApp::stopRecording(bool forced)
{
  if (!recording_) return;
  recording_ = false;
  ledOff();
  busy_ = true;

  const float seconds = audio_.recordedSeconds();
  Serial1.printf("[rec] stop: %.2fs, %u audio bytes\n", seconds,
                 static_cast<unsigned>(audio_.audioBytes()));

  if (audio_.tooShort()) {
    drawTodoList(uiStr(UiStringId::kHintTooShort), false, false);
    busy_ = false;
    return;
  }

  audio_.finishRecord();

  // Screen refresh is safe here because audio capture is already complete.
  // Unlike at recording START, where an ePaper refresh would starve the I2S
  // DMA ring and lose audio samples.
  // Inline processing state: keep the list visible, show "Processing" in the
  // header. Safe to refresh here -- audio capture is already complete.
  drawTodoList(uiStr(UiStringId::kHintAdd), true, false);
  ledOn();   // solid LED through the network call as a second cue

  if (!ensureWiFi(10000)) {
    ledOff();
    drawTodoList(uiStr(UiStringId::kHintNoWifi), false, false);
    busy_ = false;
    return;
  }

  String transcript;
  const bool sttOk = stt_.transcribe(audio_.wavData(), audio_.wavSize(),
                                     transcript);
  if (!sttOk && transcript.length() == 0) {
    transcript = uiStr(UiStringId::kNoSpeech);
  }
  Serial1.printf("[stt] \"%s\"\n", transcript.c_str());

  const time_t nowEpoch = rtc_.nowEpoch();
  MemoEntry entry = memo_.summarize(transcript, nowEpoch);
  Serial1.printf("[memo] \"%s\" due=%lld label=\"%s\"\n",
                 entry.text.c_str(), static_cast<long long>(entry.dueEpoch),
                 entry.fuzzyLabel.c_str());

  // Every panel now keeps the full MemoStore::kMax. The non-touch panels used
  // to cap at VM_VISIBLE_MEMO_MAX because nothing could scroll, so an entry
  // off-page was unreachable -- the fifth reminder silently overwrote one you
  // could still see. KEY1/KEY2 scroll the window now, so that cap only lost
  // reminders.
  store_.add(entry);
  ledOff();

  const String hint = forced
      ? uiStr(UiStringId::kHintMaxLen)
      : uiStr(UiStringId::kHintAdd);
  drawTodoList(hint, false, true);

  busy_ = false;
}

void VoiceMemoApp::captureChunk()
{
  if (!recording_) return;

  const bool full = audio_.readChunk();
  if (full) {
    stopRecording(true);
    return;
  }

  const unsigned long now = millis();
  if (now - lastBlinkMs_ >= 300) {
    lastBlinkMs_ = now;
    ledState_ = !ledState_;
    if (ledState_) ledOn(); else ledOff();
  }
}

void VoiceMemoApp::pollButton()
{
  // KEY0 is active low (hardware pull-up). Debounce converts the raw GPIO
  // into clean press / release events: press starts recording, release
  // stops the recording and triggers upload + summarize + render.
  const bool rawButton = digitalRead(kKey0Pin);
  if (rawButton != lastRawButton_) {
    debounceMs_ = millis();
    lastRawButton_ = rawButton;
  }
  if ((millis() - debounceMs_) > kDebounceDelayMs && rawButton != stableButton_) {
    stableButton_ = rawButton;
    if (stableButton_ == LOW) {
      pressStartMs_ = millis();
      startRecording();
    } else {
      // Classify by how long KEY0 was held, not by how much audio arrived.
      // Recording starts on the press edge either way, so a real memo never
      // loses its opening syllable; a click just throws those samples away.
      const unsigned long held = millis() - pressStartMs_;
      if (held < kClickMaxMs) {
        abortRecording();
        toggleSelected();
      } else {
        stopRecording(false);
      }
    }
  }
}

void VoiceMemoApp::moveSelection(int delta)
{
  if (recording_ || busy_) return;
  const int n = static_cast<int>(store_.count());
  if (n <= 0) { selectedIndex_ = -1; scrollOffset_ = 0; return; }

  if (selectedIndex_ < 0) {
    // First press enters at the edge of the page already on screen, not at the
    // top of the list -- jumping the view on the first keypress reads as a bug.
    selectedIndex_ = (delta > 0) ? scrollOffset_
                                 : scrollOffset_ + VM_VISIBLE_MEMO_MAX - 1;
    if (selectedIndex_ >= n) selectedIndex_ = n - 1;
  } else {
    selectedIndex_ += delta;
  }
  if (selectedIndex_ < 0) selectedIndex_ = 0;
  if (selectedIndex_ >= n) selectedIndex_ = n - 1;

  clampScroll();
  Serial1.printf("[nav] selecao=%d de %d (janela em %d)\n",
                 selectedIndex_, n, scrollOffset_);
  // Do NOT repaint here: a burst of presses would each block on a multi-second
  // refresh and the later ones would be dropped. flushPendingRedraw() paints
  // once the keys go quiet.
  listDirty_ = true;
  lastNavMs_ = millis();
}

void VoiceMemoApp::clampScroll()
{
  const int n = static_cast<int>(store_.count());
  const int page = VM_VISIBLE_MEMO_MAX;

  // Keep the window inside the list first, so a purge that shrank the list
  // cannot leave the view parked past the end showing nothing.
  const int maxOffset = (n > page) ? (n - page) : 0;
  if (scrollOffset_ > maxOffset) scrollOffset_ = maxOffset;
  if (scrollOffset_ < 0) scrollOffset_ = 0;

  // Then scroll the minimum needed to bring the selection into view.
  if (selectedIndex_ >= 0) {
    if (selectedIndex_ < scrollOffset_) scrollOffset_ = selectedIndex_;
    if (selectedIndex_ >= scrollOffset_ + page) {
      scrollOffset_ = selectedIndex_ - page + 1;
    }
  }
}

void VoiceMemoApp::toggleSelected()
{
  if (recording_ || busy_) return;
  if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(store_.count())) {
    // A click with nothing selected would look broken. Say what to do instead.
    pendingHintPickFirst_ = true;
    listDirty_ = true;
    lastNavMs_ = millis();
    return;
  }

  const time_t now = rtc_.nowEpoch();
  store_.toggleDone(static_cast<size_t>(selectedIndex_), now);
  Serial1.printf("[nav] concluido toggle linha %d\n", selectedIndex_);

  // The card jumps to the bottom (or back up) on the next sort, so keeping the
  // index would silently point the cursor at whatever slid into place. Parking
  // it on the first item keeps it visible and gives a predictable place to
  // continue from -- clearing it made the cursor vanish, which reads as the UI
  // losing your place.
  selectedIndex_ = (store_.count() > 0) ? 0 : -1;
  scrollOffset_ = 0;
  clampScroll();

  // Repaint through the pending path so a toggle followed by more key presses
  // still costs one refresh. Full pass, not partial: the card's own content
  // changed, which is the worst case for e-paper residue -- and the user
  // reports the pause here reads as deliberate, not as lag.
  listDirty_ = true;
  pendingFullRefresh_ = true;
  lastNavMs_ = millis();
}

void VoiceMemoApp::abortRecording()
{
  if (!recording_) return;
  recording_ = false;
  ledOff();
  Serial1.println("[rec] abortado: clique, nao gravacao");
}

void VoiceMemoApp::chimeDue()
{
  // Five rising notes. Three was distinct from the recording beep but passed
  // too quickly to register from across the room -- an alarm has to survive
  // not being listened for. Still under a second, so the loop stalls briefly
  // and the buttons stay responsive.
  static const int kNotes[] = {2000, 2400, 2800, 3200, 3600};
  const int noteCount = static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0]));
  for (int i = 0; i < noteCount; i++) {
    tone(kBuzzerPin, kNotes[i], 140);
    delay(190);
  }
  noTone(kBuzzerPin);
}

void VoiceMemoApp::pollDueAlarm()
{
  // Never interrupt a recording: the buzzer sits next to the microphone and
  // would land straight in the audio being uploaded.
  if (recording_ || busy_) return;

  const unsigned long nowMs = millis();
  if (nowMs - lastAlarmScanMs_ < kAlarmScanMs) return;
  lastAlarmScanMs_ = nowMs;

  const time_t now = rtc_.nowEpoch();
  if (now <= 0) return;

  int rang = 0;
  for (size_t i = 0; i < store_.count(); i++) {
    const MemoEntry& e = store_.at(i);
    if (vmShouldAlert(e.hasDue, e.done, e.dueEpoch, alertWatermark_, now)) {
      Serial1.printf("[alarme] venceu: \"%s\"\n", e.text.c_str());
      rang++;
    }
  }

  // One chime per scan, however many came due together: five reminders at the
  // same minute should sound like one alarm, not five.
  if (rang > 0) {
    chimeDue();
    listDirty_ = true;          // the card just became "Atrasado" on screen
    pendingFullRefresh_ = true;
    lastNavMs_ = nowMs;
  }

  // Housekeeping on the same 5 s tick: a completed reminder is purged during a
  // repaint, so without this it would survive until the next scheduled refresh
  // -- which is itself 5 minutes, i.e. it would roughly double the time the
  // user actually observes. Only the cheap comparison runs here; marking the
  // list dirty lets the existing path do the removal and the redraw.
  if (rang == 0) {
    for (size_t i = 0; i < store_.count(); i++) {
      const MemoEntry& e = store_.at(i);
      if (vmDoneExpired(e.done, e.doneAt, now, MemoStore::kDoneTtlSeconds)) {
        listDirty_ = true;
        pendingFullRefresh_ = true;   // a card is disappearing, not just moving
        lastNavMs_ = nowMs;
        break;
      }
    }
  }

  const time_t next = vmNextWatermark(now, alertWatermark_);
  if (next != alertWatermark_) {
    alertWatermark_ = next;
    store_.setAlertWatermark(alertWatermark_);
  }
}

void VoiceMemoApp::flushPendingRedraw()
{
  if (!listDirty_) return;
  if (recording_ || busy_) return;
  if (millis() - lastNavMs_ < kNavSettleMs) return;

  const bool pick = pendingHintPickFirst_;
  const bool full = pendingFullRefresh_;
  pendingHintPickFirst_ = false;
  pendingFullRefresh_ = false;
  listDirty_ = false;
  drawTodoList(uiStr(pick ? UiStringId::kHintPickFirst
                          : UiStringId::kHintAdd), false, false,
               /*partial=*/!full);
}

void VoiceMemoApp::pollNavButtons()
{
  if (recording_ || busy_) return;
  const unsigned long now = millis();

  // KEY2 (left) moves the selection up, KEY1 (middle) moves it down. Both are
  // active low with internal pull-ups, debounced exactly like KEY0, and act on
  // the press edge so a held key does not scroll away.
  const bool rawK2 = digitalRead(kKey2Pin);
  if (rawK2 != lastRawKey2_) { debounceKey2Ms_ = now; lastRawKey2_ = rawK2; }
  if ((now - debounceKey2Ms_) > kDebounceDelayMs && rawK2 != stableKey2_) {
    stableKey2_ = rawK2;
    if (stableKey2_ == LOW) moveSelection(-1);
  }

  const bool rawK1 = digitalRead(kKey1Pin);
  if (rawK1 != lastRawKey1_) { debounceKey1Ms_ = now; lastRawKey1_ = rawK1; }
  if ((now - debounceKey1Ms_) > kDebounceDelayMs && rawK1 != stableKey1_) {
    stableKey1_ = rawK1;
    if (stableKey1_ == LOW) moveSelection(1);
  }
}

void VoiceMemoApp::pollTouch()
{
#if !VM_HAS_TOUCH
  return;
#else
  // Ignore touches during recording / network calls so a stray finger does
  // not interrupt the current operation.
  if (recording_ || busy_) return;
  if (!touch_.available()) return;

  uint16_t tx = 0, ty = 0;
  if (!touch_.poll(&tx, &ty)) return;

  const int idx = ui_.hitTestCheckbox(tx, ty);
  if (idx < 0) return;

  Serial1.printf("[touch] toggle row %d\n", idx);
  store_.toggleDone(static_cast<size_t>(idx), rtc_.nowEpoch());
  drawTodoList(uiStr(UiStringId::kHintAdd), false, false);
#endif
}

void VoiceMemoApp::pollScheduledRefresh()
{
  if (recording_ || busy_) return;
  const unsigned long now = millis();
  if (now - lastListRefreshMs_ < kListRefreshMs) return;

  // KEY0 wins over the timed e-paper refresh, including the debounce window.
  // KEY0 优先级高于定时刷屏  包括按键防抖尚未稳定的短窗口。
  const bool rawButton = digitalRead(kKey0Pin);
  const bool key0MayBeActive =
      rawButton == LOW
      || lastRawButton_ == LOW
      || stableButton_ == LOW
      || rawButton != stableButton_
      || now - debounceMs_ <= kDebounceDelayMs;
  if (key0MayBeActive) return;

  drawTodoList(uiStr(UiStringId::kHintAdd), false, true, /*partial=*/true);
}

void VoiceMemoApp::loop()
{
  pollButton();
  captureChunk();
  pollNavButtons();
  pollDueAlarm();
  flushPendingRedraw();
  pollTouch();
  pollScheduledRefresh();
}
