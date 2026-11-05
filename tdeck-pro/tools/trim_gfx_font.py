#!/usr/bin/env python3
"""Trim a generated Adafruit GFXfont header to a character range, keeping each kept glyph byte-identical.

  trim_gfx_font.py src/ui/lock_clock_font.h --font Text --first 45 --last 57
Used for fonts that only ever draw digits (LockClock: weather temperature, Clock: percentages).
"""
import argparse, re
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("header", type=Path)
p.add_argument("--font", required=True, help="GFXfont name in the header")
p.add_argument("--bitmap", default="bitmap")
p.add_argument("--glyphs", default="glyphs")
p.add_argument("--first", type=int, required=True)
p.add_argument("--last", type=int, required=True)
a = p.parse_args()
text = a.header.read_text()

def array(name):
    m = re.search(r"(const (?:uint8_t|GFXglyph) " + re.escape(name) + r"\[\] PROGMEM=\{)(.*?)(\};)", text, re.S)
    if not m:
        raise SystemExit(f"array {name} not found")
    return m

bm = array(a.bitmap); gm = array(a.glyphs)
bitmap = [int(x) for x in bm.group(2).replace("\n", "").split(",") if x.strip()]
glyphs = [tuple(int(v) for v in g.split(",")) for g in re.findall(r"\{([^{}]*)\}", gm.group(2))]
fm = re.search(r"const GFXfont " + re.escape(a.font) + r" PROGMEM=\{\(uint8_t\*\)" + a.bitmap + r",\(GFXglyph\*\)" + a.glyphs + r",(\d+),(\d+),(\d+)\};", text)
if not fm:
    raise SystemExit("font definition not found")
first, last, advance = (int(x) for x in fm.groups())
assert first <= a.first <= a.last <= last and len(glyphs) == last - first + 1
out_bitmap, out_glyphs = [], []
for code in range(a.first, a.last + 1):
    off, w, h, xa, xo, yo = glyphs[code - first]
    size = (w * h + 7) // 8
    out_glyphs.append((len(out_bitmap), w, h, xa, xo, yo))
    out_bitmap += bitmap[off:off + size]
text = (text[:bm.start(2)] + ",".join(map(str, out_bitmap)) + text[bm.end(2):])
gm = array(a.glyphs)
text = text[:gm.start(2)] + ",".join("{" + ",".join(map(str, g)) + "}" for g in out_glyphs) + text[gm.end(2):]
text = text.replace(fm.group(0), fm.group(0).replace(f",{first},{last},{advance}}};", f",{a.first},{a.last},{advance}}};"))
a.header.write_text(text)
print(f"{a.header}: {a.font} {first}..{last} -> {a.first}..{a.last}, bitmap {len(bitmap)} -> {len(out_bitmap)} bytes")
