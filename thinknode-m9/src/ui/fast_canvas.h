#pragma once
#include <algorithm>
#include <Adafruit_GFX.h>
#include <stdint.h>
namespace mm {
// RGB565 canvas for the T-Deck Plus. Adafruit_GFX fills rectangles column by column (writeFastVLine), which on a
// row-major frame in PSRAM touches one cache line per pixel; here rectangles and the clear are written row by row,
// two pixels per 32-bit store. Rotation is never used by the UI, so the fast paths assume rotation 0.
class FastCanvas16 : public GFXcanvas16 {
 static void fillRow(uint16_t *p,int n,uint16_t color){
  if(n<=0)return;if(uintptr_t(p)&2){*p++=color;n--;}
  uint32_t pair=uint32_t(color)<<16|color;uint32_t *q=reinterpret_cast<uint32_t*>(p);for(int i=0;i<n/2;i++)q[i]=pair;if(n&1)p[n-1]=color;
 }
public:
 // A clear is skipped when the canvas is already one flat colour and nothing was drawn since (the frame loop and the
 // page renderers both clear: the second clear used to cost a whole PSRAM pass).
 bool flat=false;uint16_t flatColor=0;
 FastCanvas16(uint16_t w,uint16_t h):GFXcanvas16(w,h){}
 void fillScreen(uint16_t color) override{if(flat&&flatColor==color)return;uint16_t *b=getBuffer();if(b)fillRow(b,int(WIDTH)*HEIGHT,color);flat=true;flatColor=color;}
 void drawPixel(int16_t x,int16_t y,uint16_t color) override{flat=false;GFXcanvas16::drawPixel(x,y,color);}
 void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t color) override{flat=false;GFXcanvas16::drawFastVLine(x,y,h,color);}
 // GFX fonts: glyph rows go straight into the buffer (Adafruit_GFX writes every set bit through two virtual calls).
 size_t write(uint8_t c) override{
  if(!gfxFont||textsize_x!=1||textsize_y!=1||c=='\n'||c=='\r')return GFXcanvas16::write(c);
  uint8_t first=gfxFont->first;if(c<first||c>gfxFont->last)return 1;
  const GFXglyph &glyph=gfxFont->glyph[c-first];const uint8_t *bits=gfxFont->bitmap+glyph.bitmapOffset;
  int w=glyph.width,h=glyph.height,x0=cursor_x+glyph.xOffset,y0=cursor_y+glyph.yOffset;uint16_t *b=getBuffer();
  if(b&&w&&h){flat=false;uint32_t bit=0;
   // Anti-aliased faces (ui/aa_fonts.h): 4-bit coverage, blended over the frame (colours in panel byte order).
   (void)bit;coverage(x0,y0,bits,w,h,textcolor);
  }
  cursor_x+=glyph.xAdvance;return 1;
 }
 // Translucent rectangle: `ink` over the frame with coverage a (0..15), e.g. a veil over a picture.
 void tint(int x,int y,int w,int h,uint16_t ink,int a){uint16_t *b=getBuffer();if(!b)return;int x1=std::min(int(WIDTH),x+w),y1=std::min(int(HEIGHT),y+h);x=std::max(0,x);y=std::max(0,y);if(x>=x1||y>=y1)return;flat=false;
  if(a>=15){for(int yy=y;yy<y1;yy++){uint16_t *row=b+size_t(yy)*WIDTH;for(int xx=x;xx<x1;xx++)row[xx]=ink;}return;}
  const uint16_t i=uint16_t(ink<<8|ink>>8);const int ir=i>>11,ig=(i>>5)&63,ib=i&31;// ink unpacked once per call
  for(int yy=y;yy<y1;yy++){uint16_t *row=b+size_t(yy)*WIDTH;for(int xx=x;xx<x1;xx++)row[xx]=mixInk(row[xx],ir,ig,ib,a);}}
 // Blend of one pixel: a = 0..15 coverage of `ink` over the frame pixel (both in panel byte order).
 static inline uint16_t blend(uint16_t dst,uint16_t ink,int a){
  if(a>=15)return ink;uint16_t d=uint16_t(dst<<8|dst>>8),i=uint16_t(ink<<8|ink>>8);int dr=d>>11,dg=(d>>5)&63,db=d&31,ir=i>>11,ig=(i>>5)&63,ib=i&31;
  uint16_t m=uint16_t(((dr+(ir-dr)*a/15)<<11)|((dg+(ig-dg)*a/15)<<5)|(db+(ib-db)*a/15));return uint16_t(m<<8|m>>8);}
 // Ink over the frame pixel with coverage a (1..14), the ink already unpacked: same arithmetic as blend().
 static inline uint16_t mixInk(uint16_t dst,int ir,int ig,int ib,int a){uint16_t d=uint16_t(dst<<8|dst>>8);int dr=d>>11,dg=(d>>5)&63,db=d&31;
  uint16_t m=uint16_t(((dr+(ir-dr)*a/15)<<11)|((dg+(ig-dg)*a/15)<<5)|(db+(ib-db)*a/15));return uint16_t(m<<8|m>>8);}
 // 4-bit coverage bitmap in one colour (AA glyphs, icons): clipped once per call, the ink unpacked once.
 void coverage(int x0,int y0,const uint8_t *bits,int w,int h,uint16_t color){
  uint16_t *b=getBuffer();if(!b||w<=0||h<=0)return;const int xs=x0<0?-x0:0,xe=std::min(w,int(WIDTH)-x0);if(xs>=xe)return;flat=false;
  uint16_t ink=uint16_t(color<<8|color>>8);const int ir=ink>>11,ig=(ink>>5)&63,ib=ink&31;
  for(int yy=0;yy<h;yy++){int py=y0+yy;if(py<0||py>=int(HEIGHT))continue;uint16_t *row=b+py*int(WIDTH)+x0;uint32_t bit=uint32_t(yy)*w+xs;
   for(int xx=xs;xx<xe;xx++,bit++){int a=(bits[bit>>1]>>((bit&1)?0:4))&15;if(!a)continue;row[xx]=a==15?color:mixInk(row[xx],ir,ig,ib,a);}}}
 // 4-bit coverage sprite (ui/aa_icons.h) in one colour: two pixels per byte, high nibble first.
 void drawAlpha(int x0,int y0,const uint8_t *bits,int w,int h,uint16_t color){coverage(x0,y0,bits,w,h,color);}
 // RGB565 sprite with a 4-bit alpha plane (ui/emoji_color.h), drawn at its own size.
 void drawRGBA(int x0,int y0,const uint16_t *rgb,const uint8_t *alpha,int w,int h){
  uint16_t *b=getBuffer();if(!b||w<=0||h<=0)return;const int xs=x0<0?-x0:0,xe=std::min(w,int(WIDTH)-x0);if(xs>=xe)return;flat=false;
  for(int yy=0;yy<h;yy++){int py=y0+yy;if(py<0||py>=int(HEIGHT))continue;uint16_t *row=b+py*int(WIDTH)+x0;uint32_t bit=uint32_t(yy)*w+xs;
   for(int xx=xs;xx<xe;xx++,bit++){int a=(alpha[bit>>1]>>((bit&1)?0:4))&15;if(a)row[xx]=blend(row[xx],rgb[bit],a);}}}
 // Same sprite scaled to size x size (nearest neighbour), for the few places that need another size.
 void drawRGBAScaled(int x0,int y0,const uint16_t *rgb,const uint8_t *alpha,int w,int h,int size){
  if(size==w&&size==h){drawRGBA(x0,y0,rgb,alpha,w,h);return;}uint16_t *b=getBuffer();if(!b||size<=0||size>64)return;
  const int xs=x0<0?-x0:0,xe=std::min(size,int(WIDTH)-x0);if(xs>=xe)return;flat=false;int sx[64];for(int xx=xs;xx<xe;xx++)sx[xx]=xx*w/size;
  for(int yy=0;yy<size;yy++){int py=y0+yy;if(py<0||py>=int(HEIGHT))continue;const uint32_t base=uint32_t(yy*h/size)*w;uint16_t *row=b+py*int(WIDTH)+x0;
   for(int xx=xs;xx<xe;xx++){uint32_t bit=base+sx[xx];int a=(alpha[bit>>1]>>((bit&1)?0:4))&15;if(a)row[xx]=blend(row[xx],rgb[bit],a);}}}
 // Sprite reduced to size x size by averaging (box filter): every output pixel is the mean of the source area it
 // covers, colour weighted by alpha (premultiplied), so thin strokes and small details fade instead of vanishing as
 // with nearest neighbour. Integer weights: source pixel i spans [i*size,(i+1)*size), output o spans [o*w,(o+1)*w).
 // Separable: per output row the covered source rows are summed once into col[], then each output pixel sums a few
 // columns; 32-bit maths, one division per pixel. out(x,y,ink,a) gets columns [xs,xe) of every row, ink in panel
 // byte order, a = 1..15. Sprites up to 32 px (the 20/30 px sets) and size in [w/4,w): false otherwise.
 template<class Out> static bool areaReduce(const uint16_t *rgb,const uint8_t *alpha,int w,int h,int size,int xs,int xe,Out out){
  if(size<=0||size>=w||size>=h||w>32||h>32||w>4*size||h>4*size)return false;
  struct Span{uint8_t first,count,weight[6];};Span sx[32],sy[32];
  auto spans=[size](Span *s,int src){for(int o=0;o<size;o++){int lo=o*src,hi=lo+src,i=lo/size;s[o].first=uint8_t(i);s[o].count=0;
   for(;i*size<hi;i++)s[o].weight[s[o].count++]=uint8_t(std::min(hi,(i+1)*size)-std::max(lo,i*size));}};
  spans(sx,w);spans(sy,h);const uint32_t total=uint32_t(w)*h,invTotal=((1u<<24)+total/2)/total;// alpha mean without a division per pixel
  struct Sum{uint32_t a,r,g,b;};Sum col[32];
  for(int yy=0;yy<size;yy++){const Span &vy=sy[yy];
   for(int x=0;x<w;x++)col[x]={0,0,0,0};
   for(int j=0;j<vy.count;j++){const uint32_t wy=vy.weight[j];uint32_t bit=uint32_t(vy.first+j)*w;
    for(int x=0;x<w;x++,bit++){int a=(alpha[bit>>1]>>((bit&1)?0:4))&15;if(!a)continue;uint32_t k=wy*a;uint16_t c=rgb[bit];c=uint16_t(c<<8|c>>8);
     Sum &s=col[x];s.a+=k;s.r+=k*(c>>11);s.g+=k*((c>>5)&63);s.b+=k*(c&31);}}
   for(int xx=xs;xx<xe;xx++){const Span &vx=sx[xx];uint32_t sa=0,sr=0,sg=0,sb=0;
    for(int i=0;i<vx.count;i++){const Sum &s=col[vx.first+i];const uint32_t wx=vx.weight[i];sa+=wx*s.a;sr+=wx*s.r;sg+=wx*s.g;sb+=wx*s.b;}
    int a=int((sa*invTotal+(1u<<23))>>24);if(!a)continue;// sa = coverage x area weight (<= 32*32*15): the output alpha is its mean
    const uint32_t inv=((1u<<24)+sa/2)/sa;// 24-bit reciprocal (16 bits are too coarse for large sa); sums <= 63*sa keep v*inv < 2^32
    // Rounded means can land one above the field (31.6 -> 32): clamp, or a full red wraps to 0 (yellow turns green).
    auto mean=[inv](uint32_t v,uint32_t top){uint32_t m=(v*inv+(1u<<23))>>24;return m>top?top:m;};
    uint16_t m=uint16_t(mean(sr,31)<<11|mean(sg,63)<<5|mean(sb,31));out(xx,yy,uint16_t(m<<8|m>>8),a);}}
  return true;}
 // Same, drawn straight into the frame (nearest neighbour when areaReduce does not apply).
 void drawRGBAArea(int x0,int y0,const uint16_t *rgb,const uint8_t *alpha,int w,int h,int size){
  uint16_t *b=getBuffer();if(!b)return;const int xs=x0<0?-x0:0,xe=std::min(size,int(WIDTH)-x0);if(xs>=xe)return;
  bool drawn=areaReduce(rgb,alpha,w,h,size,xs,xe,[&](int x,int y,uint16_t ink,int a){int py=y0+y;if(py<0||py>=int(HEIGHT))return;flat=false;
   uint16_t &p=b[py*int(WIDTH)+x0+x];p=blend(p,ink,a);});
  if(!drawn)drawRGBAScaled(x0,y0,rgb,alpha,w,h,size);}
 // Opaque RGB565 image in panel byte order (map view): row copies, clipped.
 void blit(int x0,int y0,const uint16_t *px,int w,int h){
  uint16_t *b=getBuffer();if(!b||w<=0||h<=0)return;flat=false;int cx0=x0<0?0:x0,cy0=y0<0?0:y0,cx1=x0+w>int(WIDTH)?int(WIDTH):x0+w,cy1=y0+h>int(HEIGHT)?int(HEIGHT):y0+h;if(cx1<=cx0||cy1<=cy0)return;
  for(int y=cy0;y<cy1;y++)memcpy(b+y*int(WIDTH)+cx0,px+(y-y0)*w+(cx0-x0),size_t(cx1-cx0)*2);}
 void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color) override{
  uint16_t *b=getBuffer();if(!b||w<=0||h<=0)return;flat=false;
  if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>int(WIDTH))w=int(WIDTH)-x;if(y+h>int(HEIGHT))h=int(HEIGHT)-y;if(w<=0||h<=0)return;
  for(int row=y;row<y+h;row++)fillRow(b+row*int(WIDTH)+x,w,color);
 }
 void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t color) override{fillRect(x,y,w,1,color);}
 // Adafruit_GFX::fillRoundRect, row by row: its corners are column runs (one cache line per pixel on this frame).
 // The column half-heights come from the same midpoint walk as fillCircleHelper, so the pixels are identical.
 void fillRoundRectRows(int16_t x,int16_t y,int16_t w,int16_t h,int16_t r,uint16_t color){
  int16_t max=(w<h?w:h)/2;if(r>max)r=max;if(r<=0||r>63){fillRoundRect(x,y,w,h,r,color);return;}
  int8_t col[64];for(int d=0;d<=r;d++)col[d]=-1;col[0]=int8_t(r);
  int16_t f=1-r,ddx=1,ddy=-2*r,cx=0,cy=r,px=0,py=r;
  while(cx<cy){if(f>=0){cy--;ddy+=2;f+=ddy;}cx++;ddx+=2;f+=ddx;
   if(cx<(cy+1)&&cy>col[cx])col[cx]=int8_t(cy);
   if(cy!=py){if(px>col[py])col[py]=int8_t(px);py=cy;}
   px=cx;}
  // Row v above/below the straight band reaches the farthest column whose half-height covers it.
  int8_t span[64];for(int v=0;v<=r;v++){span[v]=0;for(int d=r;d>=1;d--)if(col[d]>=v){span[v]=int8_t(d);break;}}
  fillRect(x+r,y,w-2*r,h,color);
  for(int row=0;row<h;row++){int v=row<r?r-row:row>h-r-1?row-(h-r-1):0;int e=span[v];if(!e)continue;
   fillRect(x+r-e,y+row,e,1,color);fillRect(x+w-r,y+row,e,1,color);}
 }
};
}
