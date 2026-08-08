// FontActive.h -- picks the embedded TrueType face for the current build and
// exposes it under one name, so TextRenderer never needs to know which
// language it is rendering.
//
// Only included when VM_UI_TTF is 1 (Chinese or Portuguese builds). English
// builds use the built-in CP437 bitmap font and never see this header, so no
// font bytes are linked into the English binary.

#pragma once

#include "UiLang.h"

#if VM_LANG_ZH

#include "FontZH.h"
static const unsigned char* const vm_font_data = vm_font_zh;
static const size_t                vm_font_len  = vm_font_zh_len;

// Chinese ideographs are square and need the full cell to stay legible, so one
// "size unit" maps 1:1 onto the 8 px cell the bitmap font used.
#define VM_UI_TTF_PX_PER_UNIT 8

#elif VM_LANG_PT

#include "FontLatin.h"
static const unsigned char* const vm_font_data = vm_font_latin;
static const size_t                vm_font_len  = vm_font_latin_len;

// DejaVu Sans is a proportional Latin face: at a given em size its cap height
// is well under the em, so matching the old 8 px bitmap cell 1:1 would render
// visibly smaller than the English build. 10 px per unit lands the cap height
// near the bitmap font's 7 px while keeping advance widths comparable.
// Layout measures through measureText(), so widths adapt on their own.
#define VM_UI_TTF_PX_PER_UNIT 10

#else
#error "FontActive.h included in a build with no TrueType face (VM_UI_TTF is 0)."
#endif
