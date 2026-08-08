// UiLang.h -- compile-time UI language selection and the fixed-string table.
//
// The whole firmware is built for exactly one language, chosen at compile time
// by the build flag VM_UI_LANG_ZH (set in the Chinese platformio env). This
// header is the single source of truth both for that decision (VM_LANG_ZH) and
// for every fixed, non-user-generated UI word.
//
// Each word is exposed as an English / Chinese pair (uiStrEn / uiStrZh) with a
// macro picking the active column (uiStr). The pair shape lets a native unit
// test assert both languages from one build, mirroring DateLabels.h.

#ifndef VOICE_MEMO_UI_LANG_H
#define VOICE_MEMO_UI_LANG_H

// 1 for the Chinese build, 0 otherwise.
#if defined(VM_UI_LANG_ZH)
  #define VM_LANG_ZH 1
#else
  #define VM_LANG_ZH 0
#endif

// 1 for the Brazilian Portuguese build, 0 otherwise.
#if defined(VM_UI_LANG_PT)
  #define VM_LANG_PT 1
#else
  #define VM_LANG_PT 0
#endif

// Only Chinese needs an embedded TrueType face: no CJK ideograph exists in the
// built-in bitmap font. Portuguese stays on the bitmap font and transliterates
// through Cp437.h instead -- measured on the panel, a pixel font at its design
// size reads sharper than a rasterized outline at these sizes, and CP437
// already carries every Portuguese accent except a-tilde and o-tilde.
#if VM_LANG_ZH
  #define VM_UI_TTF 1
#else
  #define VM_UI_TTF 0
#endif

// One id per fixed UI string. kCount is a sentinel for iteration in tests.
enum class UiStringId {
  kAppName,        // header logo word
  kHintAdd,        // main idle hint
  kHintTooShort,   // recording shorter than the minimum
  kHintNoWifi,     // reminder dropped because WiFi is down
  kHintMaxLen,     // recording hit the maximum length
  kEmptyList,      // empty reminder list placeholder
  kProcessing,     // header "processing" tag
  kReminders,      // list title / fallback badge
  kBootStarting,   // boot splash: starting
  kBootWifi,       // boot splash: connecting WiFi
  kSomeDay,        // due label when the entry has no due time
  kOverdue,        // due label when the entry is past due
  kNoSpeech,       // placeholder memo when speech was not recognized
  kHintPickFirst,  // KEY0 clicked with no card selected
  kCount
};

// English column.
inline const char* uiStrEn(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "Voice Memo";
    case UiStringId::kHintAdd:      return "Hold KEY0 to add. Tap a box to check off.";
    case UiStringId::kHintTooShort: return "Hold KEY0 for at least one second.";
    case UiStringId::kHintNoWifi:   return "Reminder skipped because WiFi is unavailable.";
    case UiStringId::kHintMaxLen:   return "Stopped at max length. Hold KEY0 for another memo.";
    case UiStringId::kEmptyList:    return "Hold KEY0 and speak to add your first reminder.";
    case UiStringId::kProcessing:   return "Processing";
    case UiStringId::kReminders:    return "Reminders";
    case UiStringId::kBootStarting: return "Starting...";
    case UiStringId::kBootWifi:     return "Connecting WiFi...";
    case UiStringId::kSomeDay:      return "Some day";
    case UiStringId::kOverdue:      return "Overdue";
    case UiStringId::kNoSpeech:     return "No speech recognized.";
    case UiStringId::kHintPickFirst: return "Pick a reminder with KEY2 / KEY1 first, then click KEY0.";
    default:                        return "";
  }
}

// Chinese column.
inline const char* uiStrZh(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "语音备忘录";
    case UiStringId::kHintAdd:      return "长按 KEY0 添加  点方框勾选完成";
    case UiStringId::kHintTooShort: return "请长按 KEY0 至少一秒";
    case UiStringId::kHintNoWifi:   return "WiFi 不可用 本次提醒未保存";
    case UiStringId::kHintMaxLen:   return "已到最长录音 再次长按 KEY0 继续";
    case UiStringId::kEmptyList:    return "长按 KEY0 说话 添加第一条提醒";
    case UiStringId::kProcessing:   return "处理中";
    case UiStringId::kReminders:    return "提醒";
    case UiStringId::kBootStarting: return "启动中";
    case UiStringId::kBootWifi:     return "连接 WiFi";
    case UiStringId::kSomeDay:      return "某天";
    case UiStringId::kOverdue:      return "已逾期";
    case UiStringId::kNoSpeech:     return "未识别到语音";
    case UiStringId::kHintPickFirst: return "先用 KEY2 / KEY1 选中一条  再按 KEY0";
    default:                        return "";
  }
}

// Brazilian Portuguese column.
inline const char* uiStrPt(UiStringId id) {
  switch (id) {
    case UiStringId::kAppName:      return "Lembretes";
    case UiStringId::kHintAdd:      return "KEY0 segure grava, clique conclui. KEY2 sobe, KEY1 desce.";
    case UiStringId::kHintTooShort: return "Segure KEY0 por pelo menos um segundo.";
    case UiStringId::kHintNoWifi:   return "Lembrete descartado: WiFi indisponível.";
    case UiStringId::kHintMaxLen:   return "Limite de gravação atingido. Segure KEY0 de novo.";
    case UiStringId::kEmptyList:    return "Segure KEY0 e fale para criar seu primeiro lembrete.";
    case UiStringId::kProcessing:   return "Processando";
    case UiStringId::kReminders:    return "Lembretes";
    case UiStringId::kBootStarting: return "Iniciando...";
    case UiStringId::kBootWifi:     return "Conectando WiFi...";
    case UiStringId::kSomeDay:      return "Algum dia";
    case UiStringId::kOverdue:      return "Atrasado";
    case UiStringId::kNoSpeech:     return "Nada reconhecido.";
    case UiStringId::kHintPickFirst: return "Escolha um lembrete com KEY2 / KEY1 e depois clique no KEY0.";
    default:                        return "";
  }
}

// Active column for the current build.
#if VM_LANG_ZH
inline const char* uiStr(UiStringId id) { return uiStrZh(id); }
#elif VM_LANG_PT
inline const char* uiStr(UiStringId id) { return uiStrPt(id); }
#else
inline const char* uiStr(UiStringId id) { return uiStrEn(id); }
#endif

// Stable per-language tag persisted beside the reminder blob. Adding a
// language means adding a value here, never renumbering an existing one --
// the numbers live in users' NVS.
#if VM_LANG_ZH
  #define VM_LANG_TAG 1
#elif VM_LANG_PT
  #define VM_LANG_TAG 2
#else
  #define VM_LANG_TAG 0
#endif

// True when a stored language tag requires wiping the reminder store: the tag
// is missing (-1) or differs from the firmware's language. Pure for testing.
inline bool vmShouldWipeForLanguage(int storedTag, int firmwareTag) {
  return storedTag != firmwareTag;
}

#endif  // VOICE_MEMO_UI_LANG_H
