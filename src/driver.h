#ifndef VOICE_MEMO_REMINDER_DRIVER_H
#define VOICE_MEMO_REMINDER_DRIVER_H

// The device target is selected by platformio.ini via a build flag:
//   -D VOICE_MEMO_DEVICE_E1001 / E1002 / E1003
// E1004 is intentionally unsupported because it has no onboard microphone.
// Fall back to E1003 when built without an explicit target (e.g. Arduino IDE).
#if !defined(VOICE_MEMO_DEVICE_E1001) && \
    !defined(VOICE_MEMO_DEVICE_E1002) && \
    !defined(VOICE_MEMO_DEVICE_E1003)
  #define VOICE_MEMO_DEVICE_E1003
#endif

#define VM_SCREEN_GRAY4   1
#define VM_SCREEN_MONO    4
#define VM_SCREEN_COLOR6  2
#define VM_SCREEN_GRAY16  3

#if defined(VOICE_MEMO_DEVICE_E1001)
  #define BOARD_SCREEN_COMBO 520
  #define VM_DEVICE_NAME "reTerminal E1001"
  #define VM_SCREEN_MODE VM_SCREEN_GRAY4
  #define VM_LED_PIN 6
  #define VM_HAS_TOUCH 0
  #define VM_VISIBLE_MEMO_MAX 4
  #define VM_BATTERY_ENABLE_PIN 21
#elif defined(VOICE_MEMO_DEVICE_E1002)
  #define BOARD_SCREEN_COMBO 521
  #define VM_DEVICE_NAME "reTerminal E1002"
  #define VM_SCREEN_MODE VM_SCREEN_COLOR6
  #define VM_LED_PIN 6
  #define VM_HAS_TOUCH 0
  #define VM_VISIBLE_MEMO_MAX 4
  #define VM_BATTERY_ENABLE_PIN 21
#elif defined(VOICE_MEMO_DEVICE_E1003)
  #define BOARD_SCREEN_COMBO 522
  #define VM_DEVICE_NAME "reTerminal E1003"
  #define VM_SCREEN_MODE VM_SCREEN_GRAY16
  #define VM_LED_PIN 16
  #define VM_HAS_TOUCH 1
  #define VM_VISIBLE_MEMO_MAX 8
  #define VM_BATTERY_ENABLE_PIN 40
#else
  #error "Select VOICE_MEMO_DEVICE_E1001, VOICE_MEMO_DEVICE_E1002, or VOICE_MEMO_DEVICE_E1003."
#endif

// Optional 1-bit pipeline, selected by the build flag VM_UI_MONO.
//
// Grayscale on this panel is produced by driving the particles through several
// waveform passes -- those passes ARE the flicker. Mono is a single pass, and
// it is also the only mode EPaper::updataPartial() can read: that function
// walks the buffer at stride = width/8, i.e. 1 bpp, while initGrayMode() sets
// a 4 bpp buffer. So "stop flashing" and "refresh only a region" are the same
// switch, not two.
#if defined(VM_UI_MONO)
  #undef VM_SCREEN_MODE
  #define VM_SCREEN_MODE VM_SCREEN_MONO
#endif

#endif
