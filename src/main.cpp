/*
 * VoiceMemoReminder -- hold KEY0 to record, release to create a memo.
 *
 * This file is the PlatformIO entry point and the ONLY file you have to edit
 * to make the device work on your network with your provider keys.
 * Everything else lives in dedicated modules:
 *
 *   VoiceMemoApp.*    application orchestrator (button, lifecycle)
 *   AudioCapture.*    PDM microphone + WAV buffer
 *   RtcClock.*        PCF8563 real-time clock
 *   MemoStore.*       reminder list + NVS persistence
 *   SpeechClient.*    speech-to-text upload
 *   MemoClient.*      LLM rewrite into {memo, due} JSON
 *   MemoUI.*          all e-paper drawing
 *   JsonUtil.h        tiny shared JSON helpers
 *   driver.h          device model + screen capability selector
 *
 * To add a new STT provider, edit SpeechClient.cpp and add one case.
 * To change the LLM prompt, edit MemoClient.cpp.
 * To redesign the screen, edit MemoUI.cpp.
 * Reminders persist across power cycles via NVS automatically.
 */

#include <Arduino.h>
#include <Preferences.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "UiLang.h"
#include "VoiceMemoApp.h"

#ifndef VM_WIFI_SSID
#define VM_WIFI_SSID ""
#endif
#ifndef VM_WIFI_PASSWORD
#define VM_WIFI_PASSWORD ""
#endif
#ifndef VM_GROQ_API_KEY
#define VM_GROQ_API_KEY ""
#endif
// Bridge (ponte): base URL + this device's own token. Both set = the bridge is
// the reminder store and understands the voice (no AI key needed on board).
#ifndef VM_PONTE_URL
#define VM_PONTE_URL ""
#endif
#ifndef VM_PONTE_TOKEN
#define VM_PONTE_TOKEN ""
#endif

static String nvsWifiSsid;
static String nvsWifiPass;
static String nvsApiKey;
static String nvsPonteUrl;
static String nvsPonteToken;

static void loadNvsCredentials()
{
  Preferences prefs;
  if (!prefs.begin("config", true)) {
    nvsWifiSsid = VM_WIFI_SSID;
    nvsWifiPass = VM_WIFI_PASSWORD;
    nvsApiKey   = VM_GROQ_API_KEY;
    nvsPonteUrl   = VM_PONTE_URL;
    nvsPonteToken = VM_PONTE_TOKEN;
    return;
  }
  nvsWifiSsid = prefs.getString("wifiSsid", VM_WIFI_SSID);
  nvsWifiPass = prefs.getString("wifiPass", VM_WIFI_PASSWORD);
  nvsApiKey   = prefs.getString("apiKey", VM_GROQ_API_KEY);
  nvsPonteUrl   = prefs.getString("ponteUrl", VM_PONTE_URL);
  nvsPonteToken = prefs.getString("ponteToken", VM_PONTE_TOKEN);
  prefs.end();
}

static VoiceMemoConfig buildConfig()
{
  return {
    .wifiSsid     = nvsWifiSsid.c_str(),
    .wifiPassword = nvsWifiPass.c_str(),

    .speech = {
      .provider = VM_SPEECH_OPENAI_COMPATIBLE,
      .url      = "https://api.groq.com/openai/v1/audio/transcriptions",
      .apiKey   = nvsApiKey.c_str(),
      .model    = "whisper-large-v3-turbo",
      // Whisper auto-detects when this is empty, but naming the language up
      // front measurably cuts mis-transcription on short clips -- which is
      // exactly what a one-sentence reminder is.
      .language = VM_LANG_ZH ? "zh" : (VM_LANG_PT ? "pt" : ""),
    },

    .memo = {
      .provider = VM_MEMO_OPENAI_COMPATIBLE,
      .url      = "https://api.groq.com/openai/v1/chat/completions",
      .apiKey   = nvsApiKey.c_str(),
      // llama-3.3-70b-versatile was retired by Groq and started answering 404,
      // which silently pushed every reminder into the "now + 1 hour" fallback.
      .model    = "openai/gpt-oss-120b",
      // gpt-oss is a reasoning model: "low" keeps the round trip under a
      // second, and "hidden" drops the reasoning field so the naive parser in
      // JsonUtil.h reads the answer and not the model's train of thought.
      .extraParams = "\"reasoning_effort\":\"low\","
                     "\"reasoning_format\":\"hidden\",",
    },

    .audio = {
      .sampleRate       = 16000,
      .maxRecordSeconds = 20,
    },

    .httpTimeoutMs = 45000,

    .ponte = {
      .url   = nvsPonteUrl.c_str(),
      .token = nvsPonteToken.c_str(),
    },
  };
}

static VoiceMemoApp* app;

void setup()
{
  loadNvsCredentials();
  static VoiceMemoConfig cfg = buildConfig();
  static VoiceMemoApp instance(cfg);
  app = &instance;
  app->begin();
}

void loop()
{
  if (app) app->loop();
}
