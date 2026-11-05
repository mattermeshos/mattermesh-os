#pragma once
#include <algorithm>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifndef MAP_LINE_DELTA
#define MAP_LINE_DELTA 22
#define MAP_LINE_MAX 205
#endif
namespace device {
// Web-Mercator slippy-map maths and the 1-bit tile format of the Maps app.
// Tiles are OpenStreetMap 256x256 PNGs, converted once to 1 bpp (MSB first,
// 1 = black) and cached on SD as /maps/osm1/<z>/<x>/<y>.mmt.
constexpr int MapTileSize=256,MapTileStride=MapTileSize/8,MapTileBytes=MapTileSize*MapTileStride,MapMinZoom=6,MapMaxZoom=16;
// View without a position (Maps, Plus radar): Breuil-Cervinia under the Matterhorn, valley in sight.
constexpr double MapHomeLat=45.934,MapHomeLon=7.629;constexpr int MapHomeZoom=12;
struct MapCoord {double x=0,y=0;MapCoord(){}MapCoord(double px,double py):x(px),y(py){}}; // world pixels at a zoom level
struct MapTileRef {uint8_t z=0;uint32_t x=0,y=0;int screenX=0,screenY=0;};
inline double mapWorldSize(int z){return double(MapTileSize)*double(1u<<z);}
inline MapCoord mapProject(double latitude,double longitude,int z){
 double lat=fmax(-85.05112878,fmin(85.05112878,latitude))*M_PI/180,n=mapWorldSize(z);
 return {(longitude+180)/360*n,(1-asinh(tan(lat))/M_PI)/2*n};
}
inline void mapUnproject(MapCoord c,int z,double &latitude,double &longitude){double n=mapWorldSize(z);longitude=c.x/n*360-180;latitude=atan(sinh(M_PI*(1-2*c.y/n)))*180/M_PI;}
inline double mapMetersPerPixel(double latitude,int z){return 156543.03392*cos(latitude*M_PI/180)/double(1u<<z);}
// Tiles covering a w x h view centered on c: visible ones first, then a ring of
// `margin` tiles for prefetch. X wraps around the antimeridian; Y is clipped.
inline int mapVisibleTiles(MapCoord c,int z,int w,int h,int margin,MapTileRef *out,int capacity){
 long n=1L<<z,left=lround(floor(c.x-w/2.0)),top=lround(floor(c.y-h/2.0));
 long tx0=long(floor(left/double(MapTileSize))),ty0=long(floor(top/double(MapTileSize)));
 long tx1=long(floor((left+w-1)/double(MapTileSize))),ty1=long(floor((top+h-1)/double(MapTileSize)));int count=0;
 for(int ring=0;ring<=margin;ring++)for(long ty=ty0-ring;ty<=ty1+ring;ty++)for(long tx=tx0-ring;tx<=tx1+ring;tx++){
  bool inner=tx>=tx0-ring+1&&tx<=tx1+ring-1&&ty>=ty0-ring+1&&ty<=ty1+ring-1;if(ring&&inner)continue;
  if(ty<0||ty>=n||count>=capacity)continue;MapTileRef &t=out[count++];t.z=uint8_t(z);t.x=uint32_t(((tx%n)+n)%n);t.y=uint32_t(ty);t.screenX=int(tx*MapTileSize-left);t.screenY=int(ty*MapTileSize-top);
 }
 return count;
}
inline void mapTilePath(char *out,size_t size,int z,uint32_t x,uint32_t y){snprintf(out,size,"/maps/osm1/%d/%lu/%lu.mmt",z,(unsigned long)x,(unsigned long)y);}
// OpenStreetMap colors to an e-ink tone (0 black .. 255 white) by map feature: paper, residential, parks and
// minor roads white; woods a sparse dot, buildings and water light textures (sea stays visible); motorways,
// trunk and primary roads dark, secondary roads mid grey; labels, rails and borders black.
inline uint8_t mapLuma565(uint16_t c){int r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;return uint8_t((r*77+g*150+b*29)>>8);}
inline uint8_t mapOsmTone(uint16_t c){
 int r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31,y=(r*77+g*150+b*29)>>8,hi=r>g?(r>b?r:b):(g>b?g:b),lo=r<g?(r<b?r:b):(g<b?g:b),chroma=hi-lo;
 if(y<120)return 0;                                             // labels, rails, borders, icons
 if(b>r+25&&b>=g-12&&chroma>=24)return 196;                       // water (#aad3df)
 if(r>g+40&&r>b+40)return g<175?40:110;                          // motorway (#e892a2) / trunk (#f9b29c)
 if(r>=230&&g>=195&&g<=225&&b<190)return 150;                     // primary (#fcd6a4)
 if(r>=230&&g>=235&&b<205)return 200;                             // secondary (#f7fabf)
 if(g>r+18&&g>b+25)return y<205?212:250;                          // woods (#add19e) / parks and grass
 if(chroma<12&&y>=244)return 255;                                 // white minor road fill
 if(y>=216)return 255;                                            // land, residential (#e0dfdf)
 if(chroma<30&&y>=196)return 236;                                 // buildings (#d9d0c9), industrial
 return uint8_t(y);
}
// Atkinson error diffusion, one 256-pixel row at a time. Pixels marked in
// `forced` are drawn black and spread no error (lines stay crisp).
struct MapDither {
 int16_t error[3][MapTileSize+4]={};
 void reset(){memset(error,0,sizeof(error));}
 void line(const uint8_t *gray,uint8_t *bits,const bool *forced=nullptr){
  memset(bits,0,MapTileStride);int16_t *e0=error[0],*e1=error[1],*e2=error[2];
  for(int x=0;x<MapTileSize;x++){if(forced&&forced[x]){bits[x>>3]|=uint8_t(0x80>>(x&7));continue;}int v=gray[x]+e0[x+2];bool black=v<128;int err=(v-(black?0:255))/8;
   if(black)bits[x>>3]|=uint8_t(0x80>>(x&7));e0[x+3]+=err;e0[x+4]+=err;e1[x+1]+=err;e1[x+2]+=err;e1[x+3]+=err;e2[x+2]+=err;}
  memmove(error[0],error[1],sizeof(error[0]));memmove(error[1],error[2],sizeof(error[1]));memset(error[2],0,sizeof(error[2]));
 }
};
// Whole-tile conversion: a pixel clearly darker than its 5x5 neighbourhood is a line (road casing, coast,
// building edge, label) and goes solid black; everything else takes its feature tone and is dithered.
constexpr int MapLineDelta=MAP_LINE_DELTA,MapLineMax=MAP_LINE_MAX;
inline void mapConvertTile(const uint8_t *luma,const uint8_t *tone,uint8_t *bits){
 // 5x5 neighbourhood mean (clipped at the tile edge) by running sums: column sums slide down, a window slides across.
 MapDither dither;bool forced[MapTileSize];int col[MapTileSize]={};
 for(int yy=0;yy<2&&yy<MapTileSize;yy++)for(int x=0;x<MapTileSize;x++)col[x]+=luma[yy*MapTileSize+x];
 for(int y=0;y<MapTileSize;y++){
  if(y+2<MapTileSize)for(int x=0;x<MapTileSize;x++)col[x]+=luma[(y+2)*MapTileSize+x];
  if(y-3>=0)for(int x=0;x<MapTileSize;x++)col[x]-=luma[(y-3)*MapTileSize+x];
  const int rows=std::min(y+2,MapTileSize-1)-std::max(y-2,0)+1;int window=0;for(int xx=0;xx<2&&xx<MapTileSize;xx++)window+=col[xx];
  for(int x=0;x<MapTileSize;x++){if(x+2<MapTileSize)window+=col[x+2];if(x-3>=0)window-=col[x-3];
   const int count=rows*(std::min(x+2,MapTileSize-1)-std::max(x-2,0)+1);
   int l=luma[y*MapTileSize+x];forced[x]=l<=MapLineMax&&l*count+MapLineDelta*count<window;}
  dither.line(tone+y*MapTileSize,bits+y*MapTileStride,forced);
 }
}
// Clip a segment to the rectangle [x0,x0+w) x [y0,y0+h) (Liang-Barsky); false when fully outside.
inline bool mapClipSegment(double &ax,double &ay,double &bx,double &by,double x0,double y0,double w,double h){
 double t0=0,t1=1,dx=bx-ax,dy=by-ay,p[4]={-dx,dx,-dy,dy},q[4]={ax-x0,x0+w-1-ax,ay-y0,y0+h-1-ay};
 for(int i=0;i<4;i++){if(p[i]==0){if(q[i]<0)return false;continue;}double r=q[i]/p[i];if(p[i]<0){if(r>t1)return false;if(r>t0)t0=r;}else{if(r<t0)return false;if(r<t1)t1=r;}}
 double sx=ax,sy=ay;ax=sx+t0*dx;ay=sy+t0*dy;bx=sx+t1*dx;by=sy+t1*dy;return true;
}
// Highest zoom (<= maxZoom) at which all points fit in w x h minus a margin; center returned in lat/lon.
inline int mapFitZoom(const double *lat,const double *lon,int count,int w,int h,int margin,int maxZoom,double &centerLat,double &centerLon){
 if(count<=0)return -1;int zoom=MapMinZoom;
 for(int z=maxZoom;z>=MapMinZoom;z--){double minX=1e18,maxX=-1e18,minY=1e18,maxY=-1e18;for(int i=0;i<count;i++){auto c=mapProject(lat[i],lon[i],z);minX=fmin(minX,c.x);maxX=fmax(maxX,c.x);minY=fmin(minY,c.y);maxY=fmax(maxY,c.y);}
  if(maxX-minX<=w-2*margin&&maxY-minY<=h-2*margin||z==MapMinZoom){zoom=z;mapUnproject({(minX+maxX)/2,(minY+maxY)/2},z,centerLat,centerLon);break;}}
 return zoom;
}
inline bool mapBit(const uint8_t *bits,int stride,int x,int y){return bits[y*stride+(x>>3)]&(0x80>>(x&7));}
// Copy a tile whose top-left corner is at (sx,sy) into a 1 bpp view, clipped.
inline void mapBlit(const uint8_t *tile,int sx,int sy,uint8_t *view,int w,int h){
 int stride=(w+7)/8,x0=sx<0?0:sx,y0=sy<0?0:sy,x1=sx+MapTileSize<w?sx+MapTileSize:w,y1=sy+MapTileSize<h?sy+MapTileSize:h;
 for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){uint8_t mask=uint8_t(0x80>>(x&7));if(mapBit(tile,MapTileStride,x-sx,y-sy))view[y*stride+(x>>3)]|=mask;else view[y*stride+(x>>3)]&=uint8_t(~mask);}
}
}
