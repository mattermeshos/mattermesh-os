#pragma once
#include <algorithm>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
namespace device {
// Web-Mercator slippy-map maths of the Maps app (OpenStreetMap 256x256 tiles).
constexpr int MapTileSize=256,MapMinZoom=6,MapMaxZoom=16;
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
// Tiles covering a w x h view centred on c: visible ones first, then a ring of
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
// T-Deck Plus: OpenStreetMap PNG tiles cached as downloaded, decoded to RGB565 (panel byte order) when shown.
constexpr size_t MapTileColorBytes=size_t(MapTileSize)*MapTileSize*2;
inline void mapTilePathOsm(char *out,size_t size,int z,uint32_t x,uint32_t y){snprintf(out,size,"/maps/osm/%d/%lu/%lu.png",z,(unsigned long)x,(unsigned long)y);}
// NIGHT look for the colour map (panel byte order, in place). Lightness is inverted keeping the hue: pale land
// becomes dark grey, black labels light; white minor roads a mid grey above the land, water a dark blue under it,
// coloured roads only dimmed (they stay warm), woods/parks dark green. Integer YCbCr, one pass, no tables.
inline void mapNightLook(uint16_t *view,int count){
 for(int i=0;i<count;i++){uint16_t c=uint16_t(view[i]<<8|view[i]>>8);int r=(c>>11&31)*255/31,g=(c>>5&63)*255/63,b=(c&31)*255/31;
  int y=(r*77+g*150+b*29)>>8,cb=b-y,cr=r-y,hi=r>g?(r>b?r:b):(g>b?g:b),lo=r<g?(r<b?r:b):(g<b?g:b),ny,k;// k: chroma kept, /16
  if(hi-lo>=48&&r>=g-8&&r>b+24){ny=y*9/16;k=13;}// motorway / trunk / primary / secondary roads, POI tints
  else if(hi-lo<12&&y>=248){ny=78;k=0;}// white road fill
  else{ny=26+(255-y)*196/255;k=9;if(b>r+16&&b>=g-6){ny=ny*7/16;k=12;}}// inverted; water darker than land
  int nr=ny+cr*k/16,nb=ny+cb*k/16,ng=(ny*256-nr*77-nb*29)/150;
  nr=nr<0?0:nr>255?255:nr;ng=ng<0?0:ng>255?255:ng;nb=nb<0?0:nb>255?255:nb;
  uint16_t o=uint16_t((nr>>3)<<11|(ng>>2)<<5|(nb>>3));view[i]=uint16_t(o<<8|o>>8);}
}
// Clip a segment to the rectangle [x0,x0+w) x [y0,y0+h) (Liang-Barsky); false when fully outside.
inline bool mapClipSegment(double &ax,double &ay,double &bx,double &by,double x0,double y0,double w,double h){
 double t0=0,t1=1,dx=bx-ax,dy=by-ay,p[4]={-dx,dx,-dy,dy},q[4]={ax-x0,x0+w-1-ax,ay-y0,y0+h-1-ay};
 for(int i=0;i<4;i++){if(p[i]==0){if(q[i]<0)return false;continue;}double r=q[i]/p[i];if(p[i]<0){if(r>t1)return false;if(r>t0)t0=r;}else{if(r<t0)return false;if(r<t1)t1=r;}}
 double sx=ax,sy=ay;ax=sx+t0*dx;ay=sy+t0*dy;bx=sx+t1*dx;by=sy+t1*dy;return true;
}
// Highest zoom (<= maxZoom) at which all points fit in w x h minus a margin; centre returned in lat/lon.
inline int mapFitZoom(const double *lat,const double *lon,int count,int w,int h,int margin,int maxZoom,double &centerLat,double &centerLon){
 if(count<=0)return -1;int zoom=MapMinZoom;
 for(int z=maxZoom;z>=MapMinZoom;z--){double minX=1e18,maxX=-1e18,minY=1e18,maxY=-1e18;for(int i=0;i<count;i++){auto c=mapProject(lat[i],lon[i],z);minX=fmin(minX,c.x);maxX=fmax(maxX,c.x);minY=fmin(minY,c.y);maxY=fmax(maxY,c.y);}
  if(maxX-minX<=w-2*margin&&maxY-minY<=h-2*margin||z==MapMinZoom){zoom=z;mapUnproject({(minX+maxX)/2,(minY+maxY)/2},z,centerLat,centerLon);break;}}
 return zoom;
}
// Copy an RGB565 tile whose top-left corner is at (sx,sy) into a w x h RGB565 view, clipped.
inline void mapBlitColor(const uint16_t *tile,int sx,int sy,uint16_t *view,int w,int h){
 int x0=sx<0?0:sx,y0=sy<0?0:sy,x1=sx+MapTileSize<w?sx+MapTileSize:w,y1=sy+MapTileSize<h?sy+MapTileSize:h;if(x1<=x0||y1<=y0)return;
 for(int y=y0;y<y1;y++)memcpy(view+y*w+x0,tile+(y-sy)*MapTileSize+(x0-sx),size_t(x1-x0)*2);
}
}
