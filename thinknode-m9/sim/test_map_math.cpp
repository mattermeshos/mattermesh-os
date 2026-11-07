#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include "services/map_math.h"
using namespace device;
int main(){
 // Projection: Breuil-Cervinia at z14 is tile 8539/5833 (slippy-map reference), round-trip within 1e-9 deg.
 auto c=mapProject(45.934,7.629,14);assert(int(c.x/256)==8539&&int(c.y/256)==5833);double lat,lon;mapUnproject(c,14,lat,lon);assert(fabs(lat-45.934)<1e-9&&fabs(lon-7.629)<1e-9);
 auto zero=mapProject(0,0,6);assert(fabs(zero.x-mapWorldSize(6)/2)<1e-9&&fabs(zero.y-mapWorldSize(6)/2)<1e-9);assert(mapProject(89.9,0,6).y>-1e-6&&mapProject(-89.9,0,6).y<mapWorldSize(6)+1e-6);
 assert(fabs(mapMetersPerPixel(0,0)-156543.03392)<1e-6&&fabs(mapMetersPerPixel(60,1)-39135.76)<0.01);
 // View 240x222 centred on a tile corner: 4 visible tiles first, then the 12-tile ring.
 MapTileRef t[64];MapCoord corner{1000.0*256,1000.0*256};int n=mapVisibleTiles(corner,12,240,222,0,t,64);assert(n==4);
 for(int i=0;i<n;i++){assert(t[i].x==999||t[i].x==1000);assert(t[i].screenX==-136||t[i].screenX==120);assert(t[i].screenY==-145||t[i].screenY==111);}
 n=mapVisibleTiles(corner,12,240,222,1,t,64);assert(n==16);for(int i=4;i<16;i++)assert(t[i].x<998||t[i].x>1000||t[i].y<998||t[i].y>1000||t[i].x==998||t[i].x==1001||t[i].y==998||t[i].y==1001);
 // Antimeridian wrap and polar clipping.
 n=mapVisibleTiles({10,10},6,240,222,0,t,64);for(int i=0;i<n;i++){assert(t[i].y==0);assert(t[i].x==0||t[i].x==63);}assert(n==2);
 assert(mapVisibleTiles(corner,12,240,222,1,t,3)==3);
 // Segment clipping to the map area.
 {double ax=-10,ay=50,bx=300,by=50;assert(mapClipSegment(ax,ay,bx,by,0,0,240,222)&&ax==0&&bx==239&&ay==50&&by==50);
  ax=10;ay=10;bx=20;by=20;assert(mapClipSegment(ax,ay,bx,by,0,0,240,222)&&ax==10&&by==20);
  ax=-5;ay=-5;bx=-1;by=300;assert(!mapClipSegment(ax,ay,bx,by,0,0,240,222));
  ax=-100;ay=111;bx=120;by=-100;assert(mapClipSegment(ax,ay,bx,by,0,0,240,222)&&ax>=0&&ay>=0&&bx>=0&&by>=0&&ax<240&&ay<222);}
 // Fit: Zermatt-Chatillon fits around z10-11; a single point gets maxZoom; all points end up inside the margin box.
 {double la[]={46.02,45.75},lo[]={7.75,7.62},cl,cn;int z=mapFitZoom(la,lo,2,240,222,20,16,cl,cn);assert(z>=9&&z<=11&&fabs(cl-45.885)<0.01&&fabs(cn-7.685)<0.01);
  auto c=mapProject(cl,cn,z);for(int i=0;i<2;i++){auto p=mapProject(la[i],lo[i],z);assert(fabs(p.x-c.x)<=100&&fabs(p.y-c.y)<=91);}
  assert(mapFitZoom(la,lo,1,240,222,20,15,cl,cn)==15&&fabs(cl-46.02)<1e-9);double far[]={45.9,-33.9},farLon[]={7.6,151.2};assert(mapFitZoom(far,farLon,2,240,222,20,16,cl,cn)==MapMinZoom);assert(mapFitZoom(la,lo,0,240,222,20,16,cl,cn)==-1);}
 {// NIGHT look: pale land dark, black labels light, white roads above the land, water below it, warm roads stay warm.
  auto px=[](int r,int g,int b){uint16_t c=uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));return uint16_t(c<<8|c>>8);};
  auto luma=[](uint16_t p){uint16_t c=uint16_t(p<<8|p>>8);int r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;return (r*77+g*150+b*29)>>8;};
  uint16_t v[5]={px(242,239,233),px(0,0,0),px(255,255,255),px(170,211,223),px(252,214,164)};mapNightLook(v,5);
  assert(luma(v[0])<50&&luma(v[1])>200&&luma(v[2])>luma(v[0])+20&&luma(v[3])<luma(v[0]));
  uint16_t road=uint16_t(v[4]<<8|v[4]>>8);assert(((road>>11)&31)*8>(road&31)*8+24);}
  char osm[48];mapTilePathOsm(osm,sizeof(osm),15,17196,11862);assert(std::string(osm)=="/maps/osm/15/17196/11862.png");
 std::cout<<"PASS: Web-Mercator projection/round-trip, tile ring order/wrap/clip, SD tile path, tone curve, line-preserving dither, clipped 1bpp blit, segment clipping, route fit zoom, NIGHT look, OSM tile path\n";
}
