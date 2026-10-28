"""Pack native 1-bit chat fonts (ASCII and Latin-1: à è é ì ò ù ...) plus a Cyrillic table (U+0400..U+0491, Russian
and Ukrainian); keep the original splash/fonts untouched.

GFXfont cannot address code points above 255, so Cyrillic is a second font (TextCyr) whose code 32 is U+0400:
Components draws cp with that font as code cp-0x400+32."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CYR_FIRST, CYR_LAST = 0x400, 0x491


def pack(font, codes):
    data, glyphs = [], []
    for code in codes:
        char = chr(code)
        if 127 <= code < 160 or code == 0xAD or 0x483 <= code <= 0x489:  # controls, soft hyphen, combining marks
            glyphs.append((len(data), 0, 0, 0, 0, 0))
            continue
        if code == 0xA0:  # no-break space draws as a space
            char = ' '
        x, y, right, bottom = font.getbbox(char, anchor='ls')
        width, height = right - x, bottom - y
        image = Image.new('L', (max(width, 1), max(height, 1)), 255)
        ImageDraw.Draw(image).text((-x, -y), char, font=font, fill=0, anchor='ls')
        offset = len(data)
        bits = [int(pixel < 160) for pixel in image.get_flattened_data()] if width * height else []
        bits += [0] * (-len(bits) % 8)
        data += [sum(bits[i + j] << (7 - j) for j in range(8)) for i in range(0, len(bits), 8)]
        glyphs.append((offset, width, height, math.ceil(font.getlength(char)), x, y))
    return data, ','.join('{' + ','.join(map(str, glyph)) + '}' for glyph in glyphs)


for name, size, line_height in [('body', 13, 15), ('meta', 11, 13), ('title', 16, 20)]:
    font = ImageFont.truetype(str(ROOT / 'assets/fonts/LiberationSans-Bold.ttf'), size)
    data, packed = pack(font, range(32, 256))
    cyr_data, cyr_packed = pack(font, range(CYR_FIRST, CYR_LAST + 1))
    (ROOT / f'src/ui/chat_{name}_font.h').write_text(
        '#pragma once\n#include <Adafruit_GFX.h>\n'
        f'namespace mm_chat_{name} {{\n'
        'const uint8_t bitmap[] PROGMEM={' + ','.join(map(str, data)) + '};\n'
        'const GFXglyph glyphs[] PROGMEM={' + packed + '};\n'
        'const GFXfont Text PROGMEM={(uint8_t*)bitmap,(GFXglyph*)glyphs,32,255,'
        + str(line_height) + '};\n'
        '// Cyrillic U+0400..U+0491: code 32 is U+0400 (see tools/build_chat_fonts.py).\n'
        'const uint8_t cyrBitmap[] PROGMEM={' + ','.join(map(str, cyr_data)) + '};\n'
        'const GFXglyph cyrGlyphs[] PROGMEM={' + cyr_packed + '};\n'
        f'const GFXfont TextCyr PROGMEM={{(uint8_t*)cyrBitmap,(GFXglyph*)cyrGlyphs,32,{32 + CYR_LAST - CYR_FIRST},'
        + str(line_height) + '};\n}\n')
