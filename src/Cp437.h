// Cp437.h -- UTF-8 to CP437 transliteration for the built-in bitmap font.
//
// Seeed_GFX's built-in 5x7 font is the classic CP437 set: 256 glyphs, of which
// the upper half holds most Western European accents. Its glyph index is a
// RAW BYTE, not a Unicode code point, so drawing UTF-8 directly puts "a-acute"
// (U+00E1 = 225) at slot 225 -- a Greek letter -- instead of slot 0xA0.
//
// Verified on the actual font data (scripts/inspect_glcdfont.py), CP437 has:
//     a-grave a-acute a-circumflex  c-cedilla  e-acute e-circumflex
//     i-acute o-acute o-circumflex  u-acute u-diaeresis
//     C-cedilla E-acute
// and does NOT have a-tilde or o-tilde -- the two Portuguese needs most. Those
// degrade to bare "a" / "o", which still reads ("amanha", "nao"), whereas a
// wrong glyph does not.
//
// Rendering through the bitmap font rather than an embedded TrueType face is a
// deliberate choice: on this 4-gray e-paper panel a pixel font drawn at its
// design size is visibly sharper than a rasterized outline at UI sizes.

#ifndef VOICE_MEMO_CP437_H
#define VOICE_MEMO_CP437_H

#include <Arduino.h>

// Maps one Unicode code point to a CP437 byte. Returns 0 when the point has
// no sensible slot, so callers can skip it rather than draw a wrong glyph.
inline uint8_t vmUnicodeToCp437(uint32_t cp)
{
  if (cp < 0x80) return static_cast<uint8_t>(cp);   // plain ASCII
  switch (cp) {
    // Present in CP437, drawn correctly.
    case 0x00E0: return 0x85;  // a grave
    case 0x00E1: return 0xA0;  // a acute
    case 0x00E2: return 0x83;  // a circumflex
    case 0x00E4: return 0x84;  // a diaeresis
    case 0x00E7: return 0x87;  // c cedilla
    case 0x00E8: return 0x8A;  // e grave
    case 0x00E9: return 0x82;  // e acute
    case 0x00EA: return 0x88;  // e circumflex
    case 0x00ED: return 0xA1;  // i acute
    case 0x00F3: return 0xA2;  // o acute
    case 0x00F4: return 0x93;  // o circumflex
    case 0x00F6: return 0x94;  // o diaeresis
    case 0x00FA: return 0xA3;  // u acute
    case 0x00FC: return 0x81;  // u diaeresis
    case 0x00C7: return 0x80;  // C cedilla
    case 0x00C9: return 0x90;  // E acute
    case 0x00F1: return 0xA4;  // n tilde
    case 0x00D1: return 0xA5;  // N tilde
    case 0x00BA: return 0xA7;  // masculine ordinal
    case 0x00AA: return 0xA6;  // feminine ordinal

    // Absent from CP437 -- fall back to the unaccented letter.
    case 0x00E3: return 'a';   // a tilde
    case 0x00F5: return 'o';   // o tilde
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: return 'A';
    case 0x00C8: case 0x00CA: return 'E';
    case 0x00CD: return 'I';
    case 0x00D3: case 0x00D4: case 0x00D5: return 'O';
    case 0x00DA: case 0x00DC: return 'U';

    // Punctuation the LLM may emit despite the prompt.
    case 0x2018: case 0x2019: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x2026: return '.';
    default: return 0;         // no glyph: caller skips it
  }
}

// Converts a UTF-8 String into raw CP437 bytes ready for the bitmap font.
// Call this on every string handed to drawString()/textWidth() once UTF-8
// decoding is switched OFF on the panel, so widths and glyphs agree.
inline String vmUtf8ToCp437(const String& in)
{
  String out;
  out.reserve(in.length());
  const size_t n = in.length();
  for (size_t i = 0; i < n; ) {
    const uint8_t c = static_cast<uint8_t>(in[i]);
    uint32_t cp = c;
    size_t len = 1;
    if ((c & 0xE0) == 0xC0)      { cp = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }

    if (len > 1) {
      if (i + len > n) break;    // truncated sequence: stop cleanly
      for (size_t k = 1; k < len; k++) {
        cp = (cp << 6) | (static_cast<uint8_t>(in[i + k]) & 0x3F);
      }
    }
    i += len;

    const uint8_t mapped = vmUnicodeToCp437(cp);
    if (mapped) out += static_cast<char>(mapped);
  }
  return out;
}

#endif  // VOICE_MEMO_CP437_H
