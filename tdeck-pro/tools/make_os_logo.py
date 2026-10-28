#!/usr/bin/env python3
"""MatterMesh OS logo: Matterhorn peak in a rounded badge with a mesh route (nodes + hops)
down its face. Writes assets/logo/*.png/svg and src/ui/os_logo_bitmap.h (1 bpp, e-ink).
When assets/logo/mattermesh-logo-source.png exists (the drawn logo, black ink on white) it is used instead of the
geometry below: cropped to the badge, framed like the drawn badge (2..62 of 64) and scaled to every size."""
from pathlib import Path
from PIL import Image, ImageDraw
REPO=Path(__file__).resolve().parents[1]
# Geometry in a 64-unit box.
BADGE=(2,2,62,62,14)
PEAK=[(6,52),(12,44),(15,43),(30,9),(34,7.5),(36,11),(38,22),(44,26),(47,33),(52,37),(58,52)]  # west face, hooked summit, stepped Hornli ridge
ROUTE=[(32,18),(26,33),(42,37),(21,46)]          # summit -> repeaters -> valley: a packet's hops
def draw(scale):
    S=64*scale;im=Image.new('L',(S,S),255);d=ImageDraw.Draw(im);p=lambda x,y:(x*scale,y*scale)
    x0,y0,x1,y1,r=BADGE;d.rounded_rectangle([p(x0,y0),p(x1,y1)],radius=r*scale,fill=0)
    d.polygon([p(*v) for v in PEAK],fill=255)
    d.rectangle([p(7,52),p(57,55.5)],fill=255)        # ground band under the mountain
    w=int(2.6*scale)
    for a,b in zip(ROUTE,ROUTE[1:]):d.line([p(*a),p(*b)],fill=0,width=w)
    for i,(x,y) in enumerate(ROUTE):
        rr=(3.8 if i in (0,len(ROUTE)-1) else 2.8)*scale;d.ellipse([x*scale-rr,y*scale-rr,x*scale+rr,y*scale+rr],fill=0)
        if i in (0,len(ROUTE)-1):ri=1.6*scale;d.ellipse([x*scale-ri,y*scale-ri,x*scale+ri,y*scale+ri],fill=255)
    return im
SOURCE=REPO/'assets/logo/mattermesh-logo-source.png'
def master():
    if not SOURCE.exists():return draw(16)
    im=Image.open(SOURCE).convert('L');box=im.point(lambda v:255 if v<128 else 0).getbbox();im=im.crop(box)
    side=max(im.size);pad=round(side*2/60);out=Image.new('L',(side+2*pad,side+2*pad),255);out.paste(im,(pad+(side-im.width)//2,pad+(side-im.height)//2));return out
def onebit(size):
    big=master().resize((size,size),Image.Resampling.LANCZOS);return big.point(lambda v:0 if v<128 else 255,'1')
def svg():
    x0,y0,x1,y1,r=BADGE;pts=' '.join(f'{x},{y}' for x,y in PEAK);route=' '.join(f'{x},{y}' for x,y in ROUTE)
    dots=''.join(f'<circle cx="{x}" cy="{y}" r="{3.8 if i in (0,len(ROUTE)-1) else 2.8}" fill="#152131"/>'+(f'<circle cx="{x}" cy="{y}" r="1.6" fill="#fff"/>' if i in (0,len(ROUTE)-1) else '') for i,(x,y) in enumerate(ROUTE))
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}" rx="{r}" fill="#152131"/>'
            f'<polygon points="{pts}" fill="#fff"/><rect x="7" y="52" width="50" height="3.5" fill="#fff"/>'
            f'<polyline points="{route}" fill="none" stroke="#152131" stroke-width="2.6" stroke-linejoin="round" stroke-linecap="round"/>{dots}</svg>\n')
def header(images):
    out=['#pragma once','#include <stdint.h>','// MatterMesh OS logo (tools/make_os_logo.py), 1 bpp MSB first, 1 = ink.','namespace mm_logo {']
    for size,im in images:
        rows=[];w=(size+7)//8
        for y in range(size):
            for bx in range(w):
                v=0
                for b in range(8):
                    x=bx*8+b
                    if x<size and im.getpixel((x,y))==0:v|=0x80>>b
                rows.append(f'0x{v:02x}')
        out.append(f'constexpr int size{size}={size};inline constexpr uint8_t logo{size}[]={{{",".join(rows)}}};')
    out.append('}');return '\n'.join(out)+'\n'
if __name__=='__main__':
    out=REPO/'assets/logo';out.mkdir(parents=True,exist_ok=True)
    master().resize((512,512),Image.Resampling.LANCZOS).save(out/'mattermesh-logo-512.png')
    if SOURCE.exists():(out/'mattermesh-logo.svg').unlink(missing_ok=True)# a drawn logo has no vector form here
    else:(out/'mattermesh-logo.svg').write_text(svg())
    imgs=[(s,onebit(s)) for s in (112,56,32)]
    for s,im in imgs:im.save(out/f'mattermesh-logo-{s}-1bit.png')
    (REPO/'src/ui/os_logo_bitmap.h').write_text(header(imgs))
    print('logo written',[s for s,_ in imgs])
