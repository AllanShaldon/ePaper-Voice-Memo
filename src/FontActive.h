// FontActive.h -- picks the embedded TrueType face for the current build and
// exposes it under one name, so TextRenderer never needs to know which
// language it is rendering.
//
// Only included when VM_UI_TTF is 1 (Chinese builds). Latin-script builds use
// the built-in CP437 bitmap font via Cp437.h and never see this header, so no
// font bytes are linked into those binaries.

#pragma once

#include "UiLang.h"

#if VM_LANG_ZH

#include "FontZH.h"
static const unsigned char* const vm_font_data = vm_font_zh;
static const size_t                vm_font_len  = vm_font_zh_len;

// Chinese ideographs are square and need the full cell to stay legible, so one
// "size unit" maps 1:1 onto the 8 px cell the bitmap font used.
#define VM_UI_TTF_PX_PER_UNIT 8

#else
#error "FontActive.h included in a build with no TrueType face (VM_UI_TTF is 0)."
#endif
