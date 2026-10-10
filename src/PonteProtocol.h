// PonteProtocol.h -- the reminder list as the bridge (ponte) sends it.
//
// Since 2026-10-09 the bridge on the home server is the ONE store of reminders:
// this device and the WhatsApp bridge read and write the same list. The device
// keeps a local copy only to draw and to ring on time without the network.
//
// Pure C++11 (std::string), no Arduino, so test/test_ponte_protocol runs it on
// the host. The bridge answers, for every call:
//   {"agora":<utc>,"lembretes":[{"id":1,"texto":"...","quandoTs":<utc|0>,
//    "temHorario":true,"concluido":false,"origem":"ponte"}, ...], ...}
// quandoTs is UTC; this device's RTC keeps LOCAL wall time as an epoch, so the
// conversion is quandoTs + tz offset (see vmPonteLocalEpoch).

#ifndef VOICE_MEMO_PONTE_PROTOCOL_H
#define VOICE_MEMO_PONTE_PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

struct PonteItem {
  long long id = 0;
  std::string texto;
  long long quandoTs = 0;   // UTC epoch; 0 = no time
  bool temHorario = false;
};

namespace ponte_detail {

inline void appendUtf8(std::string& out, unsigned long cp)
{
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

inline bool hex4(const std::string& s, size_t i, unsigned long& v)
{
  if (i + 4 > s.size()) return false;
  v = 0;
  for (size_t k = i; k < i + 4; k++) {
    const char c = s[k];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= static_cast<unsigned long>(c - '0');
    else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned long>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned long>(c - 'A' + 10);
    else return false;
  }
  return true;
}

// Position just after `"key":` (and spaces) inside [from, to), or npos.
inline size_t valueStart(const std::string& s, const char* key, size_t from, size_t to)
{
  const std::string needle = std::string("\"") + key + "\":";
  const size_t p = s.find(needle, from);
  if (p == std::string::npos || p >= to) return std::string::npos;
  size_t i = p + needle.size();
  while (i < to && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
  return i < to ? i : std::string::npos;
}

// Decodes a JSON string starting at the opening quote; `end` = index after the closing quote.
inline bool readString(const std::string& s, size_t i, std::string& out, size_t& end)
{
  if (i >= s.size() || s[i] != '"') return false;
  out.clear();
  for (size_t k = i + 1; k < s.size(); k++) {
    const char c = s[k];
    if (c == '"') { end = k + 1; return true; }
    if (c != '\\') { out += c; continue; }
    if (++k >= s.size()) return false;
    switch (s[k]) {
      case 'n': out += ' '; break;   // one-line cards: a newline would break the layout
      case 't': out += ' '; break;
      case 'r': break;
      case 'b': case 'f': break;
      case 'u': {
        unsigned long cp = 0;
        if (!hex4(s, k + 1, cp)) return false;
        k += 4;
        if (cp >= 0xD800 && cp <= 0xDBFF && k + 6 < s.size() && s[k + 1] == '\\' && s[k + 2] == 'u') {
          unsigned long lo = 0;
          if (hex4(s, k + 3, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            k += 6;
          }
        }
        appendUtf8(out, cp);
        break;
      }
      default: out += s[k]; break;   // \" \\ \/
    }
  }
  return false;
}

inline bool readNumber(const std::string& s, const char* key, size_t from, size_t to, long long& v)
{
  const size_t i = valueStart(s, key, from, to);
  if (i == std::string::npos) return false;
  if (s[i] != '-' && (s[i] < '0' || s[i] > '9')) return false;
  v = strtoll(s.c_str() + i, nullptr, 10);
  return true;
}

inline bool readBool(const std::string& s, const char* key, size_t from, size_t to, bool& v)
{
  const size_t i = valueStart(s, key, from, to);
  if (i == std::string::npos) return false;
  if (s.compare(i, 4, "true") == 0) { v = true; return true; }
  if (s.compare(i, 5, "false") == 0) { v = false; return true; }
  return false;
}

// Index of the '}' that closes the object opened at `open`, skipping strings.
inline size_t objectEnd(const std::string& s, size_t open)
{
  int depth = 0;
  bool inStr = false;
  for (size_t k = open; k < s.size(); k++) {
    const char c = s[k];
    if (inStr) {
      if (c == '\\') k++;
      else if (c == '"') inStr = false;
      continue;
    }
    if (c == '"') inStr = true;
    else if (c == '{') depth++;
    else if (c == '}' && --depth == 0) return k;
  }
  return std::string::npos;
}

}  // namespace ponte_detail

// Parses the bridge answer into `out` (replaced) and the server clock into
// `agoraUtc`. false = not a list answer (HTTP error page, truncated body...):
// the caller keeps its local copy instead of wiping the screen.
inline bool vmPonteParseLista(const std::string& json, std::vector<PonteItem>& out, long long& agoraUtc)
{
  using namespace ponte_detail;
  agoraUtc = 0;
  readNumber(json, "agora", 0, json.size(), agoraUtc);
  const size_t arr = valueStart(json, "lembretes", 0, json.size());
  if (arr == std::string::npos || json[arr] != '[') return false;

  std::vector<PonteItem> lista;
  size_t i = arr + 1;
  while (i < json.size()) {
    while (i < json.size() && (json[i] == ' ' || json[i] == ',' || json[i] == '\n' || json[i] == '\r' || json[i] == '\t')) i++;
    if (i >= json.size()) return false;
    if (json[i] == ']') { out.swap(lista); return true; }
    if (json[i] != '{') return false;
    const size_t fim = objectEnd(json, i);
    if (fim == std::string::npos) return false;

    PonteItem it;
    if (!readNumber(json, "id", i, fim, it.id) || it.id <= 0) return false;
    const size_t t = valueStart(json, "texto", i, fim);
    size_t depoisTexto = 0;
    if (t == std::string::npos || !readString(json, t, it.texto, depoisTexto)) return false;
    readNumber(json, "quandoTs", i, fim, it.quandoTs);
    if (!readBool(json, "temHorario", i, fim, it.temHorario)) it.temHorario = it.quandoTs > 0;
    if (it.quandoTs <= 0) it.temHorario = false;
    lista.push_back(it);
    i = fim + 1;
  }
  return false;
}

// One string field of the answer ("aviso", "transcricao"); "" when absent.
inline std::string vmPonteCampo(const std::string& json, const char* key)
{
  using namespace ponte_detail;
  const size_t i = valueStart(json, key, 0, json.size());
  std::string v;
  size_t fim = 0;
  if (i == std::string::npos || !readString(json, i, v, fim)) return std::string();
  return v;
}

// JSON string literal (with quotes) for a POST body.
inline std::string vmPonteJsonString(const std::string& s)
{
  std::string o = "\"";
  for (size_t k = 0; k < s.size(); k++) {
    const unsigned char c = static_cast<unsigned char>(s[k]);
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c < 0x20) o += ' ';
    else o += static_cast<char>(c);
  }
  return o + "\"";
}

// --- merge rule (local copy x bridge list) -------------------------------
// A local reminder that is NOT in the list the bridge just sent: keep it?
//   - never uploaded (remoteId 0): yes, it goes up on the next sync;
//   - a local change still waiting to be sent (pendSync): yes, ours is newer;
//   - done here and still inside the "show it checked off" window: yes, so the
//     card stays visible the usual 5 minutes instead of vanishing on sync;
//   - anything else: no -- it was finished or cancelled elsewhere (panel,
//     WhatsApp), and the bridge is the store.
inline bool vmPonteKeepLocalOnly(long long remoteId, bool done, long long doneAt,
                                 bool pendSync, long long now, long long ttl)
{
  if (remoteId <= 0) return true;
  if (pendSync) return true;
  if (done && doneAt > 0 && now > 0 && now - doneAt < ttl) return true;
  return false;
}

// A reminder present in BOTH: whose "done" wins? The local one only while its
// change has not reached the bridge yet; otherwise the bridge's (the list only
// carries not-done items, so that means "not done").
inline bool vmPonteLocalDoneWins(bool pendSync)
{
  return pendSync;
}

// Bridge time (UTC) -> this device's local wall-clock epoch, and back.
inline long long vmPonteLocalEpoch(long long quandoTsUtc, long tzOffsetSeconds)
{
  return quandoTsUtc > 0 ? quandoTsUtc + tzOffsetSeconds : 0;
}
inline long long vmPonteUtcEpoch(long long localEpoch, long tzOffsetSeconds)
{
  return localEpoch > 0 ? localEpoch - tzOffsetSeconds : 0;
}

#endif  // VOICE_MEMO_PONTE_PROTOCOL_H
