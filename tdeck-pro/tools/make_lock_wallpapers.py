#!/usr/bin/env python3
"""Lock-screen wallpapers: the user's Matterhorn artworks in assets/sources/wallpapers/ converted to 240x302
1 bpp image + mask pairs, drawn from just under the status bar (y 18) to the bottom of the screen. Each artwork
is drawn black on white with the mountain in its lower half; it is scaled (no reshaping) so it covers the width
and its lowest ink reaches the bottom edge, cropped around the summit, which lands on row 88 (inside the lower third of the clock digits). The slopes run
under the message card and on to the bottom edge (the renderer puts a white plate behind the repeater line).
Plain threshold, no dithering: the near-white sky stays clear on e-paper. The mask is opaque from each column's
first ink row down, so the mountain hides the digits while the sky stays transparent. Writes
src/ui/lock_wallpapers.h and assets/wallpapers/lock-<name>-1bit.png / -1bit-3x.png / -mask.png."""
from pathlib import Path
from PIL import Image
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from packbits import pack  # same folder: lock art is stored PackBits-packed
REPO=Path(__file__).resolve().parents[1]
W,H=240,302;SUMMIT_Y=88
# (identifier, Settings name, Settings detail, source file)
SOURCES=[("route","Mesh route","Matterhorn with the mesh nodes",REPO/"assets/sources/wallpapers/matterhorn-route.png"),
         ("pines","Pines","Snowy peak above a pine forest",REPO/"assets/sources/wallpapers/matterhorn-pines.png"),
         ("engraving","Engraving","Ink-drawn Matterhorn and forest",REPO/"assets/sources/wallpapers/matterhorn-engraving.png")]
def summit(im):
    """Top ink row of the artwork and the centre x of its ink there."""
    px=im.load()
    for y in range(im.height):
        xs=[x for x in range(im.width) if px[x,y]<128]
        if xs:return y,(min(xs)+max(xs))//2
    raise SystemExit("no ink")
def convert(src):
    """Scale so the art spans the full width and its lowest ink row reaches the bottom edge with the summit on
    SUMMIT_Y (whichever needs more: the wider result is cropped around the summit column)."""
    im=Image.open(src).convert("L");sy,sx=summit(im)
    bottom=im.point(lambda v:255 if v<128 else 0).getbbox()[3]
    s=max(W/im.width,(H-SUMMIT_Y)/(bottom-sy))
    small=im.resize((round(im.width*s),round(im.height*s)),Image.LANCZOS)
    left=min(max(round(sx*s)-W//2,0),small.width-W)
    canvas=Image.new("L",(W,H),255);canvas.paste(small,(-left,round(SUMMIT_Y-sy*s)))
    image=canvas.point(lambda v:0 if v<128 else 255)
    mask=Image.new("L",(W,H),0);mp=mask.load();ip=image.load()
    for x in range(W):
        y=0
        while y<H and ip[x,y]:y+=1
        for yy in range(y,H):mp[x,yy]=255
    return image,mask
def rows(img,ink):
    px=img.load();out=[]
    for y in range(H):
        row=bytearray((W+7)//8)
        for x in range(W):
            if (px[x,y]==0)==ink:row[x>>3]|=0x80>>(x&7)
        out+=row
    return bytes(out)
def c_array(name,data):
    body=",".join("0x%02x"%b for b in pack(data))
    return "inline constexpr uint8_t %s[]={%s};\n"%(name,body)
def main():
    out=REPO/"assets/wallpapers";out.mkdir(parents=True,exist_ok=True)
    header=["#pragma once","#include <stdint.h>",'#include "packbits.h"',
     f"// Lock-screen wallpapers (tools/make_lock_wallpapers.py): {W}x{H} 1 bpp MSB first from y 18, PackBits (src/ui/packbits.h), image 1 = ink, mask 1 = mountain (0 = sky).",
     "namespace mm_wallpaper {",f"constexpr int width={W},height={H},summitY={SUMMIT_Y};","struct Wallpaper{const char *name,*detail;const uint8_t *image,*mask;};"]
    table=[]
    for ident,name,detail,src in SOURCES:
        image,mask=convert(src)
        image.save(out/f"lock-{ident}-1bit.png");mask.save(out/f"lock-{ident}-mask.png")
        image.resize((W*3,H*3),Image.NEAREST).save(out/f"lock-{ident}-1bit-3x.png")
        header.append(c_array(f"{ident}_image",rows(image,True)).rstrip());header.append(c_array(f"{ident}_mask",rows(mask,False)).rstrip())
        table.append(f'{{"{name}","{detail}",{ident}_image,{ident}_mask}}')
        mp=mask.load();top=min(y for x in range(W) for y in range(H) if mp[x,y]);print(ident,"mask top row",top)
    header.append("inline constexpr Wallpaper list[]={"+",".join(table)+"};")
    header.append("constexpr int count=sizeof(list)/sizeof(list[0]);")
    header.append("}")
    (REPO/"src/ui/lock_wallpapers.h").write_text("\n".join(header)+"\n")
    print("wrote src/ui/lock_wallpapers.h")
main()
