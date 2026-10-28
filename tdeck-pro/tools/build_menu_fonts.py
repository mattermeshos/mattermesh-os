"""Pack native 1-bit chat fonts; keep the original splash/fonts untouched."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
for name, size, line_height in [('clock', 40, 44), ('label', 9, 11)]:
    font = ImageFont.truetype(str(ROOT / 'assets/fonts/LiberationSans-Bold.ttf'), size)
    data, glyphs = [], []
    for code in range(32, 59 if name == 'clock' else 127):
        char = chr(code)
        x, y, right, bottom = font.getbbox(char, anchor='ls')
        width, height = right - x, bottom - y
        image = Image.new('L', (max(width, 1), max(height, 1)), 255)
        ImageDraw.Draw(image).text((-x, -y), char, font=font, fill=0, anchor='ls')
        offset = len(data)
        bits = [int(pixel < 160) for pixel in image.get_flattened_data()] if width * height else []
        bits += [0] * (-len(bits) % 8)
        data += [sum(bits[i + j] << (7 - j) for j in range(8)) for i in range(0, len(bits), 8)]
        glyphs.append((offset, width, height, math.ceil(font.getlength(char)), x, y))
    packed = ','.join('{' + ','.join(map(str, glyph)) + '}' for glyph in glyphs)
    (ROOT / f'src/ui/menu_{name}_font.h').write_text(
        '#pragma once\n#include <Adafruit_GFX.h>\n'
        f'namespace mm_menu_{name} {{\n'
        'const uint8_t bitmap[] PROGMEM={' + ','.join(map(str, data)) + '};\n'
        'const GFXglyph glyphs[] PROGMEM={' + packed + '};\n'
        'const GFXfont Text PROGMEM={(uint8_t*)bitmap,(GFXglyph*)glyphs,32,' + str(58 if name == 'clock' else 126) + ','
        + str(line_height) + '};\n}\n')
