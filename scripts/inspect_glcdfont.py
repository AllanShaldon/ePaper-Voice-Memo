#!/usr/bin/env python3
"""Dev helper: render glyphs out of Seeed_GFX's built-in 5x7 bitmap font.

Answers one question the datasheet does not: which accented characters the
built-in font actually draws, and at which byte index. Run from the project
root after a build (the library lives under .pio/libdeps/<env>/).
"""
import glob
import re
import sys

paths = glob.glob(".pio/libdeps/*/Seeed_GFX/Fonts/glcdfont.c")
if not paths:
    sys.exit("glcdfont.c not found -- build an env first")

src = open(paths[0]).read()
body = src[src.index("{") + 1:src.rindex("}")]
vals = [int(x, 16) for x in re.findall(r"0[xX]([0-9a-fA-F]{2})", body)]
print("font: %s" % paths[0])
print("glyph bytes: %d  ->  glyphs: %d" % (len(vals), len(vals) // 5))


def render(idx):
    cols = vals[idx * 5:idx * 5 + 5]
    if len(cols) < 5:
        return ["(out of range)"]
    return ["".join("#" if (c >> bit) & 1 else "." for c in cols)
            for bit in range(8)]


# CP437 slots that would hold the accents Portuguese needs.
CANDIDATES = [
    (0x82, "e-acute"), (0x83, "a-circumflex"), (0x85, "a-grave"),
    (0x87, "c-cedilla"), (0x88, "e-circumflex"), (0x93, "o-circumflex"),
    (0xA0, "a-acute"), (0xA1, "i-acute"), (0xA2, "o-acute"),
    (0xA3, "u-acute"), (0xC6, "a-tilde?"), (0xE4, "o-tilde?"),
    (0x80, "C-cedilla?"), (0x90, "E-acute?"), (0x81, "u-diaeresis?"),
]

for idx, name in CANDIDATES:
    print("--- 0x%02X  (%s) ---" % (idx, name))
    for row in render(idx):
        print("    " + row)
