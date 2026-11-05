#!/usr/bin/env python3
"""Lock-screen clock glyphs: Liberation Sans Bold (the Home clock face), 76 px digits, squeezed
horizontally only if "00:00" would not fit in 220 px. Writes src/ui/lock_clock_glyphs.h
(mm_hero_clock: glyphs 0-9 : -, 1 bpp MSB first, 1 = ink)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
REPO=Path(__file__).resolve().parents[1]
W=240;CLOCK_H=64;GLYPHS='0123456789:-';FONT=REPO/'assets/fonts/LiberationSans-Bold.ttf'
def glyphs():
    font=ImageFont.truetype(str(FONT),140);x0,y0,x1,y1=font.getbbox('0',anchor='ls')
    size=int(140*CLOCK_H/(y1-y0));font=ImageFont.truetype(str(FONT),size)
    x0,y0,x1,y1=font.getbbox('0',anchor='ls');top=y0;height=y1-y0
    d0=font.getbbox('0',anchor='ls');dc=font.getbbox(':',anchor='ls');natural=4*(d0[2]-d0[0]+4)+(dc[2]-dc[0]+2)-4
    squeeze=min(1.0,(W-2*10)/natural);out=[]
    for ch in GLYPHS:
        gx0,gy0,gx1,gy1=font.getbbox(ch,anchor='ls');w=gx1-gx0
        im=Image.new('L',(w+4,height+4),0);ImageDraw.Draw(im).text((2-gx0,2-top),ch,font=font,fill=255,anchor='ls')
        if squeeze<1.0:im=im.resize((max(1,int(round(im.width*squeeze))),im.height),Image.Resampling.LANCZOS)
        ink=im.point(lambda v:0 if v>=128 else 255,'1');advance=ink.width if ch!=':' else ink.width-2
        out.append((ch,ink,advance))
    return out,height+4,squeeze
def header_bits(im):
    bw=(im.width+7)//8;rows=[]
    for y in range(im.height):
        for bx in range(bw):
            v=0
            for b in range(8):
                x=bx*8+b
                if x<im.width and im.getpixel((x,y))==0:v|=0x80>>b
            rows.append(f'0x{v:02x}')
    return rows
if __name__=='__main__':
    gs,gh,squeeze=glyphs();blob=[];entries=[]
    for ch,g,a in gs:
        offset=len(blob);blob+=header_bits(g);entries.append(f"{{'{ch}',{g.width},{g.height},{a},{offset}}}")
    (REPO/'src/ui/lock_clock_glyphs.h').write_text('#pragma once\n#include <stdint.h>\n// Lock-screen clock digits (tools/make_lock_clock.py): Liberation Sans Bold like the Home clock, 1 bpp MSB first, 1 = ink.\n'
        'namespace mm_hero_clock {\nstruct Glyph { char ch; uint8_t w,h,advance; uint16_t offset; };\n'
        f'constexpr int height={gh};\ninline constexpr Glyph glyphs[]={{{",".join(entries)}}};\nconstexpr int count={len(entries)};\ninline constexpr uint8_t bitmap[]={{{",".join(blob)}}};\n}}\n')
    print('clock glyphs written, height',gh,'squeeze',round(squeeze,2),'bytes',len(blob))
