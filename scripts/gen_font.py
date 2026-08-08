# Pre-build hook: embed a TTF into the firmware as a C byte array, so the font
# ships inside the binary and needs no SPIFFS / mkspiffs / uploadfs.
# Regenerates each target only when missing or stale.
# Runs both as a PlatformIO pre-script and as a standalone `python3` script.
#
# Targets:
#   data/test_ZH.ttf -> src/FontZH.h (vm_font_zh)   Chinese builds
#
# The table is a list so another language that needs an embedded face can be
# added without touching the logic.
import os

TARGETS = [
    (os.path.join("data", "test_ZH.ttf"),
     os.path.join("src", "FontZH.h"), "vm_font_zh"),
]


def up_to_date(src, out):
    return (os.path.exists(out)
            and os.path.getmtime(out) >= os.path.getmtime(src))


def generate(src, out, symbol):
    with open(src, "rb") as f:
        data = f.read()

    parts = [
        "#pragma once",
        "// Auto-generated from %s by scripts/gen_font.py." % src,
        "// Do not edit by hand; it is regenerated on the relevant build.",
        "#include <cstddef>",
        "",
        "const unsigned char %s[] = {" % symbol,
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        parts.append("  " + "".join("0x%02x," % b for b in chunk))
    parts.append("};")
    parts.append("const size_t %s_len = %d;" % (symbol, len(data)))
    parts.append("")

    with open(out, "w") as f:
        f.write("\n".join(parts))
    print("[gen_font] wrote %s (%d bytes)" % (out, len(data)))


for _src, _out, _symbol in TARGETS:
    if not os.path.exists(_src):
        print("[gen_font] WARNING: %s not found; skipping" % _src)
    elif up_to_date(_src, _out):
        print("[gen_font] %s is up to date" % _out)
    else:
        generate(_src, _out, _symbol)
