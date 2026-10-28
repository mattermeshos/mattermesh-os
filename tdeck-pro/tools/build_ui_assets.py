"""Deterministic editable icon set and monochrome GFX fonts; build-time only."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json,math,shutil
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets';OUT=ROOT/'src/ui';OUT.mkdir(exist_ok=True)
icons={}
def add(name,*ops):icons[name]=list(ops)
def line(*xy):return ['line',list(xy)]
def rect(x,y,w,h):return ['rect',[x,y,w,h]]
def circle(x,y,r,fill=False):return ['circle',[x,y,r],fill]
def poly(*xy):return ['poly',list(xy)]
add('messages',rect(3,5,24,17),line(8,22,7,28,16,22),circle(9,13,1,True),circle(15,13,1,True),circle(21,13,1,True))
add('mesh',line(16,7,6,25,26,25,16,7),circle(16,7,4),circle(6,25,4),circle(26,25,4))
add('maps',line(3,8,11,4,21,8,29,4,29,25,21,29,11,25,3,29,3,8),line(11,4,11,25),line(21,8,21,29))
add('weather',circle(11,11,5),line(11,1,11,3),line(1,11,3,11),line(3,3,5,5),circle(17,20,7),circle(8,23,4),circle(25,23,4),line(8,27,25,27))
add('radio',rect(7,9,18,21),line(11,1,11,9),line(12,16,20,16),line(12,20,20,20),circle(19,26,1,True))
add('files',line(2,10,2,6,13,6,17,10,29,10,29,27,2,27,2,10))
add('terminal',rect(2,4,28,24),line(7,11,12,16,7,21),line(16,22,23,22))
add('notes',rect(5,3,20,26),line(9,9,21,9),line(9,14,21,14),line(9,19,17,19),line(17,27,28,16))
add('tools',line(6,27,25,8),circle(8,25,4),line(22,3,21,10,27,11,29,5))
add('settings',line(3,8,29,8),line(3,16,29,16),line(3,24,29,24),rect(8,5,5,6),rect(21,13,5,6),rect(12,21,5,6))
add('gps',circle(16,16,10),circle(16,16,2,True),line(16,1,16,8),line(16,24,16,31),line(1,16,8,16),line(24,16,31,16))
add('info',circle(16,16,13),circle(16,9,1,True),line(16,14,16,24),line(13,24,19,24))
add('battery',rect(2,8,25,16),rect(27,12,3,8),rect(6,12,16,8))
add('signal',rect(3,24,4,6),rect(10,18,4,12),rect(17,11,4,19),rect(24,3,4,27))
add('wifi',line(2,9,8,5,16,3,24,5,30,9),line(7,15,12,12,20,12,25,15),line(12,21,16,19,20,21),circle(16,27,2,True))
add('bluetooth',line(16,2,16,30,25,22,7,9),line(7,23,25,10,16,2))
add('contacts',circle(11,10,5),circle(24,13,4),line(2,29,2,24,6,19,16,19,20,24,20,29),line(21,21,27,21,31,27,31,29))
add('keyboard',rect(2,6,28,21),*[rect(5+6*x,10+5*y,2,2) for y in range(2) for x in range(4)],line(8,23,24,23))
add('compass',circle(16,16,13),poly(21,8,12,12,8,24,20,20),circle(16,16,1,True))
add('clock',circle(16,16,13),line(16,6,16,16,23,20))
add('lock',rect(6,14,20,15),line(10,14,10,7,13,3,19,3,22,7,22,14),circle(16,21,2,True))
add('power',line(16,1,16,16),line(9,7,4,13,4,22,10,29,22,29,28,22,28,13,23,7))
add('back',line(21,3,8,16,21,29))
add('search',circle(13,13,9),line(20,20,30,30))
add('compose',rect(3,7,22,22),line(14,21,29,6,26,3,11,18,10,23,14,21))
add('menu',line(4,7,28,7),line(4,16,28,16),line(4,25,28,25))
add('send',poly(2,3,30,16,2,29,7,16),line(7,16,30,16))
add('plus',line(16,4,16,28),line(4,16,28,16))
add('chevron_right',line(10,4,22,16,10,28))
add('chevron_down',line(4,10,16,22,28,10))
add('mountain',poly(2,28,8,16,11,21,18,3,30,28),line(18,3,17,13,21,11,25,21),line(18,3,13,20,18,15,21,24))
add('grid',*[rect(4+15*x,4+15*y,9,9) for y in range(2) for x in range(2)])
add('cellular',rect(8,3,16,26),line(12,7,20,7),circle(16,25,1,True))
add('vibration',rect(10,6,12,20),line(5,8,2,13,5,18,2,23),line(27,8,30,13,27,18,30,23))
icon_source=ASSETS/'sources/icons.json'
if icon_source.exists():icons=json.loads(icon_source.read_text())
else:icon_source.write_text(json.dumps(icons,indent=2))
def pack(im,rowpad=False):
 w,h=im.size;bits=[]
 for y in range(h):
  for x in range(w):bits.append(1 if im.getpixel((x,y))<128 else 0)
  if rowpad:bits.extend([0]*((-w)%8))
 return bytes(sum((bits[i+j] if i+j<len(bits) else 0)<<(7-j) for j in range(8)) for i in range(0,len(bits),8))
headers=['#pragma once','#include <Arduino.h>','namespace mm_assets {']
for name,ops in icons.items():
 svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><g fill="none" stroke="#000" stroke-width="2.4" stroke-linejoin="round" stroke-linecap="round">']
 im=Image.new('L',(128,128),255);d=ImageDraw.Draw(im)
 for op in ops:
  kind,p=op[:2];coords=[v*4 for v in p]
  if kind=='line':
   svg.append('<polyline points="'+' '.join(f'{p[i]},{p[i+1]}' for i in range(0,len(p),2))+'"/>');d.line(coords,fill=0,width=10,joint='curve')
  elif kind=='poly':
   svg.append('<polygon points="'+' '.join(f'{p[i]},{p[i+1]}' for i in range(0,len(p),2))+'"/>');d.line(coords+coords[:2],fill=0,width=10,joint='curve')
  elif kind=='rect':
   x,y,w,h=p;svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}"/>');d.rectangle([x*4,y*4,(x+w)*4,(y+h)*4],outline=0,width=10)
  else:
   x,y,r=p;fill=op[2];svg.append(f'<circle cx="{x}" cy="{y}" r="{r}"'+(' fill="#000"' if fill else '')+'/>');d.ellipse([(x-r)*4,(y-r)*4,(x+r)*4,(y+r)*4],fill=0 if fill else None,outline=0,width=10)
 svg.append('</g></svg>');(ASSETS/'icons'/('icon_'+name+'.svg')).write_text(''.join(svg))
 for size in (12,16,24,32):
  bmp=im.resize((size,size),Image.Resampling.LANCZOS).point(lambda v:255 if v>155 else 0,'1')
  bmp.save(ASSETS/'icons'/f'icon_{name}_{size}.png')
  raw=pack(bmp,True);headers.append(f'const uint8_t icon_{name}_{size}[] PROGMEM={{'+','.join(f'0x{v:02x}' for v in raw)+'};')
headers.append('}');(OUT/'icons_bitmap.h').write_text('\n'.join(headers))
fonts=[('Small',10,'Bold'),('Body',11,'Regular'),('Bold',11,'Bold'),('Title',13,'Bold'),('Brand',18,'Bold'),('Clock',30,'Bold')]
fhead=['#pragma once','#include <Adafruit_GFX.h>','namespace mm_fonts {']
meta={}
for name,size,weight in fonts:
 source=ASSETS/'fonts'/f'LiberationSans-{weight}.ttf'
 if not source.exists():shutil.copy2(Path('/usr/share/fonts/liberation')/source.name,source)
 font=ImageFont.truetype(str(source),size);asc,desc=font.getmetrics();data=bytearray();glyphs=[]
 for code in range(32,127):
  ch=chr(code);box=font.getbbox(ch,anchor='ls');x0,y0,x1,y1=box;w,h=x1-x0,y1-y0
  im=Image.new('L',(max(1,w),max(1,h)),255);ImageDraw.Draw(im).text((-x0,-y0),ch,font=font,fill=0,anchor='ls')
  im=im.point(lambda v:255 if v>185 else 0,'1');off=len(data)
  if w and h:data.extend(pack(im))
  glyphs.append((off,w,h,max(1,math.ceil(font.getlength(ch))),x0,y0))
 fhead.append(f'const uint8_t {name}Bitmap[] PROGMEM={{'+','.join(str(v) for v in data)+'};')
 fhead.append(f'const GFXglyph {name}Glyphs[] PROGMEM={{'+','.join('{'+','.join(str(v) for v in g)+'}' for g in glyphs)+'};')
 fhead.append(f'const GFXfont {name} PROGMEM={{(uint8_t*){name}Bitmap,(GFXglyph*){name}Glyphs,32,126,{asc+desc+1}}};')
 meta[name]={'ascent':asc,'line':asc+desc+1,'pixel_size':size,'source':source.name}
fhead.append('}');(OUT/'fonts_bitmap.h').write_text('\n'.join(fhead));(ASSETS/'fonts/metrics.json').write_text(json.dumps(meta,indent=2))
print(f'Generated {len(icons)} editable icons, 4 bitmap sizes and 6 firmware fonts.')
