#include "TextRenderer.h"

#include "UiLang.h"

#if !VM_UI_TTF
#include "Cp437.h"
#endif

#if VM_UI_TTF

#include "OpenFontRender.h"
#include "FontActive.h"  // vm_font_data / vm_font_len for this build

// OpenFontRender's FileSupport.h declares these file hooks for its file-based
// loadFont path. We load the font from memory and never open a file, but the
// symbols must still resolve at link time, so provide inert stubs.
FT_FILE *OFR_fopen(const char *, const char *) { return nullptr; }
void OFR_fclose(FT_FILE *) {}
size_t OFR_fread(void *, size_t, size_t, FT_FILE *) { return 0; }
int OFR_fseek(FT_FILE *, long int, int) { return -1; }
long int OFR_ftell(FT_FILE *) { return -1; }

namespace {

// Single renderer bound to the panel (Chinese build only).
OpenFontRender g_ofr;

// The panel OFR draws into, and the current glyph color. The pixel hooks below
// are non-capturing (so they convert cleanly to std::function) and read these
// globals instead of capturing the display reference.
EPaper* g_disp = nullptr;
uint16_t g_ink = 0;

// Maps the bitmap "size unit" (the old setTextSize scale, ~8 px per unit) to
// OpenFontRender pixels. Tune on hardware so Chinese glyphs visually match the
// former bitmap sizes.
constexpr int VM_TTF_PX_PER_UNIT = VM_UI_TTF_PX_PER_UNIT;

}  // namespace

#else  // English build: map alignment onto TFT_eSPI text datums.

namespace {

uint8_t toTftDatum(TextAlign a) {
  switch (a) {
    case TextAlign::TopLeft:      return TL_DATUM;
    case TextAlign::TopCenter:    return TC_DATUM;
    case TextAlign::TopRight:     return TR_DATUM;
    case TextAlign::MiddleLeft:   return ML_DATUM;
    case TextAlign::MiddleCenter: return MC_DATUM;
    case TextAlign::MiddleRight:  return MR_DATUM;
    case TextAlign::BottomLeft:   return BL_DATUM;
    case TextAlign::BottomCenter: return BC_DATUM;
    case TextAlign::BottomRight:  return BR_DATUM;
  }
  return TL_DATUM;
}

}  // namespace

#endif

bool TextRenderer::begin(EPaper& display)
{
  display_ = &display;
#if VM_UI_TTF
  g_disp = &display;
  g_ofr.setDrawer(static_cast<TFT_eSPI&>(display));
  // The gray16 panel is a 4-bit sprite: drawPixel keeps only the low 4 bits of
  // the color as a gray index (Sprite.cpp). OpenFontRender anti-aliases by
  // blending fg/bg as RGB565, so the value it feeds drawPixel has a meaningless
  // low nibble here -- the glyph comes out hollow and blurry. Override OFR's
  // pixel hooks to paint SOLID ink (g_ink, a real gray index) for every covered
  // pixel via the panel's own virtual drawPixel (the path MemoUI uses). This
  // trades anti-aliasing for crisp, solid Chinese text.
#if VM_SCREEN_MODE == VM_SCREEN_GRAY4
  // gray4 keeps anti-aliasing. The panel's four ink levels are the indices
  // 0..3, which occupy only the low bits of RGB565's blue channel, so OFR's
  // per-channel fg/bg blend lands on a value that is already a valid gray
  // index -- the edge blend survives instead of being rounded to solid ink.
  // This matters for Latin text: at UI sizes a solid-ink glyph reads as
  // jagged, which is exactly what the gray16 workaround below causes.
  g_ofr.set_drawPixel([](int32_t px, int32_t py, uint16_t blended) {
    if (g_disp) g_disp->drawPixel(px, py, blended & 0x03);
  });
#else
  // gray16 (E1003): drawPixel keeps only the low 4 bits of the color, and
  // OFR's RGB565 blend leaves a meaningless nibble there -- glyphs come out
  // hollow. Paint solid ink instead, trading anti-aliasing for legibility.
  g_ofr.set_drawPixel([](int32_t px, int32_t py, uint16_t) {
    if (g_disp) g_disp->drawPixel(px, py, g_ink);
  });
#endif
  g_ofr.set_drawFastHLine([](int32_t px, int32_t py, int32_t pw, uint16_t) {
    if (g_disp) {
      for (int32_t i = 0; i < pw; ++i) g_disp->drawPixel(px + i, py, g_ink);
    }
  });
  // loadFont returns non-zero on failure. The font is embedded in flash
  // (FontZH.h), so there is no filesystem to mount.
  if (g_ofr.loadFont(vm_font_data, vm_font_len)) {
    fontReady_ = false;
    Serial1.println("[ofr] loadFont (embedded) failed");
    return false;
  }
  g_ofr.showCredit();   // FreeType FTL license attribution
  fontReady_ = true;
  return true;
#else
  // Turn the panel's UTF-8 decoder OFF: with it on, "a-acute" (U+00E1) would
  // index glyph 225 instead of CP437's 0xA0. Cp437.h does the mapping and
  // hands over raw glyph indices, so the decoder must stay out of the way.
  display.setAttribute(UTF8_SWITCH, false);
  fontReady_ = true;    // bitmap font is always available
  return true;
#endif
}

void TextRenderer::drawText(const String& text, int x, int y, int sizeUnit,
                            TextAlign align, uint16_t color, uint16_t bg)
{
  if (!display_) return;
#if VM_UI_TTF
  // Never call into OpenFontRender without a loaded font: it dereferences a
  // null face and crashes. Degrade by skipping the glyphs instead.
  if (!fontReady_) return;

  // Solid ink the pixel hooks paint for this glyph run.
  g_ink = color;

  const unsigned px = static_cast<unsigned>(sizeUnit * VM_TTF_PX_PER_UNIT);
  g_ofr.setFontSize(px);
  // "%s" wrapper: getTextWidth is printf-style, so a literal '%' in the text
  // would otherwise be read as a format specifier.
  const int w = static_cast<int>(g_ofr.getTextWidth("%s", text.c_str()));
  const int h = static_cast<int>(px);

  // Resolve the anchor to a top-left origin ourselves, then draw with
  // Align::TopLeft. OFR's other alignment modes are reported as fiddly, and
  // TopLeft is the mode whose behavior is least ambiguous.
  int ox = x;
  int oy = y;
  switch (align) {
    case TextAlign::TopCenter:
    case TextAlign::MiddleCenter:
    case TextAlign::BottomCenter: ox = x - w / 2; break;
    case TextAlign::TopRight:
    case TextAlign::MiddleRight:
    case TextAlign::BottomRight:  ox = x - w; break;
    default: break;
  }
  switch (align) {
    case TextAlign::MiddleLeft:
    case TextAlign::MiddleCenter:
    case TextAlign::MiddleRight:  oy = y - h / 2; break;
    case TextAlign::BottomLeft:
    case TextAlign::BottomCenter:
    case TextAlign::BottomRight:  oy = y - h; break;
    default: break;
  }

  FT_BBox bbox;
  FT_Error error;
  g_ofr.drawHString(text.c_str(), ox, oy, color, bg,
                    Align::TopLeft, Drawing::Execute, bbox, error);
#else
  display_->setTextSize(sizeUnit);
  display_->setTextColor(color, bg, true);
  display_->setTextDatum(toTftDatum(align));
  // UTF-8 decoding is off (see begin()), so the panel treats each byte as a
  // glyph index -- which is exactly what the CP437 mapping produces.
  display_->drawString(vmUtf8ToCp437(text), x, y);
#endif
}

int TextRenderer::measureText(const String& text, int sizeUnit)
{
  if (!display_) return 0;
#if VM_UI_TTF
  if (!fontReady_) return 0;
  g_ofr.setFontSize(static_cast<unsigned>(sizeUnit * VM_TTF_PX_PER_UNIT));
  return static_cast<int>(g_ofr.getTextWidth("%s", text.c_str()));
#else
  display_->setTextSize(sizeUnit);
  // Measure the same bytes that will be drawn, or wrapping would use one
  // length and rendering another.
  return static_cast<int>(display_->textWidth(vmUtf8ToCp437(text)));
#endif
}
