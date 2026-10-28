"""Generate crisp native one-bit UI fonts and emoji glyphs. No runtime dependencies."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from fontTools.ttLib import TTFont
import math
ROOT=Path(__file__).resolve().parents[1]
for name,size,line_height,scale in [('lock_clock',62,64,1),('nav_label',11,13,.80)]:
 font=ImageFont.truetype(str(ROOT/'assets/fonts/LiberationSans-Bold.ttf'),size);data=[];glyphs=[]
 for cp in range(32,127):
  ch=chr(cp);x,y,right,bottom=font.getbbox(ch,anchor='ls');w,h=right-x,bottom-y
  im=Image.new('L',(max(w,1),max(h,1)),255);ImageDraw.Draw(im).text((-x,-y),ch,font=font,fill=0,anchor='ls')
  if scale!=1:im=im.resize((max(1,round(w*scale)),max(1,h)),Image.Resampling.LANCZOS);w=round(w*scale);x=round(x*scale)
  bits=[int(v<160) for v in im.get_flattened_data()] if w*h else [];bits += [0]*(-len(bits)%8);offset=len(data)
  data += [sum(bits[i+j]<<(7-j) for j in range(8)) for i in range(0,len(bits),8)]
  glyphs.append((offset,w,h,max(1,round(font.getlength(ch)*scale)),x,y))
 packed=','.join('{'+','.join(map(str,g))+'}' for g in glyphs)
 (ROOT/f'src/ui/{name}_font.h').write_text('#pragma once\n#include <Adafruit_GFX.h>\nnamespace mm_'+name+' {\nconst uint8_t bitmap[] PROGMEM={'+','.join(map(str,data))+'};\nconst GFXglyph glyphs[] PROGMEM={'+packed+'};\nconst GFXfont Text PROGMEM={(uint8_t*)bitmap,(GFXglyph*)glyphs,32,126,'+str(line_height)+'};\n}\n')
fontpath=ROOT/'assets/fonts/NotoColorEmoji.ttf';tt=TTFont(fontpath);cmap=tt.getBestCmap();font=ImageFont.truetype(str(fontpath),109)
entries=[];atlas=Image.new('RGB',(14*32,14*8),'white');samples=[]
for cp in sorted(cmap):
 if cp<0x2000 or cp in (0x200d,0xfe0e,0xfe0f) or 0x1f3fb<=cp<=0x1f3ff:continue
 ch=chr(cp);x,y,r,b=font.getbbox(ch);im=Image.new('RGBA',(r-x,b-y));ImageDraw.Draw(im).text((-x,-y),ch,font=font,embedded_color=True)
 box=im.getbbox()
 if not box:continue
 im=im.crop(box);im.thumbnail((14,14),Image.Resampling.LANCZOS);cell=Image.new('RGBA',(14,14));cell.alpha_composite(im,((14-im.width)//2,(14-im.height)//2))
 alpha=cell.getchannel('A').point(lambda v:255 if v>=90 else 0);padded=Image.new('L',(16,16));padded.paste(alpha,(1,1));eroded=padded.filter(ImageFilter.MinFilter(3)).crop((1,1,15,15));gray=Image.new('RGBA',(14,14),'white');gray.alpha_composite(cell);gray=gray.convert('L')
 bits=[];mono=Image.new('1',(14,14),1)
 for yy in range(14):
  for xx in range(14):
   ink=bool(alpha.getpixel((xx,yy))) and (not eroded.getpixel((xx,yy)) or gray.getpixel((xx,yy))<140)
   mono.putpixel((xx,yy),0 if ink else 1)
 for yy in range(14):
  for byte in range(2):
   bits.append(sum((1 if byte*8+j<14 and not mono.getpixel((byte*8+j,yy)) else 0)<<(7-j) for j in range(8)))
 if cp==0x2705:
  mono=Image.new('1',(14,14),1);d=ImageDraw.Draw(mono);d.line([(2,7),(5,11),(12,2)],fill=0,width=2);bits=[]
  for yy in range(14):
   for byte in range(2):bits.append(sum((1 if byte*8+j<14 and not mono.getpixel((byte*8+j,yy)) else 0)<<(7-j) for j in range(8)))
 entries.append('{'+str(cp)+',{'+','.join(map(str,bits))+'}}')
 if len(samples)<256:atlas.paste(mono,(len(samples)%32*14,len(samples)//32*14));samples.append(cp)
(ROOT/'src/ui/emoji_bitmap.h').write_text('#pragma once\n#include <stdint.h>\nnamespace mm_emoji {\nstruct Glyph {uint32_t code;uint8_t pixels[28];};\nconst Glyph glyphs[] PROGMEM={'+','.join(entries)+'};\nconstexpr int count=sizeof(glyphs)/sizeof(glyphs[0]);\n}\n')
(ROOT/'screenshots/mattermesh/emoji-atlas.png').parent.mkdir(parents=True,exist_ok=True);atlas.resize((atlas.width*3,atlas.height*3),Image.Resampling.NEAREST).save(ROOT/'screenshots/mattermesh/emoji-atlas.png')
print('Generated lock clock, navigation label and',len(entries),'monochrome emoji glyphs')
