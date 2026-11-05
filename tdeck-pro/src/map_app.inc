// Maps app (section 9): OpenStreetMap 1 bpp tiles from services/map_tiles, own
// GPS position and MeshCore nodes that advertise a position. Included by main.cpp.
// Also the static packet-route picture shown from a received message (MAP).
// The Maps app draws in mm::MapPageArea (whole page), the packet-route picture in mm::MapArea.
struct MapView {int zoom=13;double lat=0,lon=0;bool follow=false,ready=false;} mapView;
static uint8_t *mapViewBits=nullptr;static int mapVisibleCount=0,mapMissingCount=0;
// Zoom limit (z6 out, z16 in): the map stays put and the status label says so until the next pan or zoom.
static int mapZoomLimit=0;// -1 out, +1 in
static uint8_t *mapAllocView(const mm::Rect &a){return static_cast<uint8_t*>(heap_caps_malloc(size_t((a.w+7)/8)*a.h,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));}
// Composes the tiles around (lat,lon,z) into bits; returns how many visible tiles are missing.
static int mapComposeTiles(uint8_t *bits,const mm::Rect &a,double lat,double lon,int z,int margin){
 const int w=a.w,h=a.h;auto center=device::mapProject(lat,lon,z);
 device::MapTileRef refs[32];int visible=device::mapVisibleTiles(center,z,w,h,0,refs,32),total=margin?device::mapVisibleTiles(center,z,w,h,margin,refs,32):visible;
 memset(bits,0,size_t((w+7)/8)*h);int missing=0;
 for(int i=0;i<total;i++){if(i>=visible){device::mapTilePrefetch(z,refs[i].x,refs[i].y);continue;}const uint8_t *tile=device::mapTile(z,refs[i].x,refs[i].y,true);if(tile)device::mapBlit(tile,refs[i].screenX,refs[i].screenY,bits,w,h);else missing++;}
 mapVisibleCount=visible;return missing;
}
// Screen position inside area a for a view centered on (clat,clon) at zoom z (unclipped).
static void mapScreen(const mm::Rect &a,double lat,double lon,double clat,double clon,int z,double &x,double &y){
 auto c=device::mapProject(clat,clon,z),p=device::mapProject(lat,lon,z);double dx=p.x-c.x,world=device::mapWorldSize(z);if(dx>world/2)dx-=world;else if(dx<-world/2)dx+=world;
 x=a.x+a.w/2+dx;y=a.y+a.h/2+(p.y-c.y);
}
static bool mapInside(const mm::Rect &a,double x,double y){return x>=a.x&&x<a.x+a.w&&y>=a.y&&y<a.y+a.h;}
static std::string mapDistance(int meters){return meters>=1000?std::to_string(meters/1000)+" km":std::to_string(meters)+" m";}
static void mapScale(double lat,int z,mm::State &v){
 double metersPerPixel=device::mapMetersPerPixel(lat,z);static const int steps[]={10,20,50,100,200,500,1000,2000,5000,10000,20000,50000,100000,200000};
 for(int meters:steps){int px=int(meters/metersPerPixel);if(px>80)break;if(px>=20){v.mapScalePx=px;v.mapScaleLabel=mapDistance(meters);}}
}

void mapOpen(){
 auto gps=device::locationState();
 if(!mapView.ready){if(gps.configured){mapView.lat=gps.latitude;mapView.lon=gps.longitude;mapView.zoom=13;}else{mapView.lat=device::MapHomeLat;mapView.lon=device::MapHomeLon;mapView.zoom=device::MapHomeZoom;}mapView.ready=true;}
 mapView.follow=gps.fix;if(gps.fix){mapView.lat=gps.latitude;mapView.lon=gps.longitude;}
 mapZoomLimit=0;device::mapTilesRescan();device::mapTilesForget();
}
// Leaving the app (Back, Home, lock, serial): drop the queued downloads so the
// worker stops fetching, and release the composed view (PSRAM) until next open.
void mapClose(){device::mapTilesForget();if(mapViewBits){heap_caps_free(mapViewBits);mapViewBits=nullptr;}mapVisibleCount=mapMissingCount=0;}
void mapPan(int dx,int dy){
 auto c=device::mapProject(mapView.lat,mapView.lon,mapView.zoom);double world=device::mapWorldSize(mapView.zoom);
 c.x=fmod(c.x+dx+world,world);c.y=constrain(c.y+dy,0.0,world-1);device::mapUnproject(c,mapView.zoom,mapView.lat,mapView.lon);
 mapView.follow=false;mapZoomLimit=0;device::mapTilesForget();render();
}
void mapZoom(int delta){int zoom=constrain(mapView.zoom+delta,device::MapMinZoom,device::MapMaxZoom);if(zoom==mapView.zoom){mapZoomLimit=delta<0?-1:1;render();return;}mapZoomLimit=0;mapView.zoom=zoom;device::mapTilesForget();render();}
void mapCenter(){
 auto gps=device::locationState();
 mapZoomLimit=0;if(gps.fix||gps.configured){mapView.lat=gps.latitude;mapView.lon=gps.longitude;mapView.follow=gps.fix;if(mapView.zoom<12)mapView.zoom=13;notice="";}
 else notice="No GPS position";
 device::mapTilesForget();render();
}
// W/A/S/D pan a third of the view, I/O (or +/-) zoom, C or ENTER center on GPS.
bool mapKey(char c){
 switch(c){case 'w':mapPan(0,-mm::MapPageArea.h/3);return true;case 's':mapPan(0,mm::MapPageArea.h/3);return true;case 'a':mapPan(-mm::MapPageArea.w/3,0);return true;case 'd':mapPan(mm::MapPageArea.w/3,0);return true;
  case 'i':case '+':case '=':mapZoom(1);return true;case 'o':case '-':mapZoom(-1);return true;case 'c':case '\n':mapCenter();return true;}
 return false;
}
// Floating buttons: back, zoom out / center / zoom in. A tap on the map brings that point to the center.
bool mapTap(int x,int y){
 if(mm::MapBackFloat.contains(x,y)){back();return true;}
 if(mm::mapButtonRect(0).contains(x,y)){mapZoom(-1);return true;}if(mm::mapButtonRect(1).contains(x,y)){mapCenter();return true;}if(mm::mapButtonRect(2).contains(x,y)){mapZoom(1);return true;}
 const auto &a=mm::MapPageArea;if(a.contains(x,y)){mapPan(x-(a.x+a.w/2),y-(a.y+a.h/2));return true;}
 return false;
}
void mapCompose(mm::State &v){
 const auto &a=mm::MapPageArea;v.mapFull=true;
 if(!mapViewBits)mapViewBits=mapAllocView(a);if(!mapViewBits)return;
 auto gps=device::locationState();if(mapView.follow&&gps.fix){mapView.lat=gps.latitude;mapView.lon=gps.longitude;}
 const int w=a.w,h=a.h,z=mapView.zoom;int missing=mapComposeTiles(mapViewBits,a,mapView.lat,mapView.lon,z,1);
 mapMissingCount=missing;v.mapBits=mapViewBits;v.mapFollow=mapView.follow;
 auto center=device::mapProject(mapView.lat,mapView.lon,z);double north,west,south,east;device::mapUnproject({center.x-w/2.0,center.y-h/2.0},z,north,west);device::mapUnproject({center.x+w/2.0,center.y+h/2.0},z,south,east);
 matter::NodePin pins[24];int count=matter::nodePins(south,west,north,east,pins,24);
 for(int i=0;i<count;i++){double x,y;mapScreen(a,pins[i].lat,pins[i].lon,mapView.lat,mapView.lon,z,x,y);if(!mapInside(a,x,y))continue;mm::MapPin pin;pin.x=lround(x);pin.y=lround(y);pin.repeater=pins[i].repeater;pin.name=mm::senderDisplayName(pins[i].name);v.mapPins.push_back(pin);}
 if(gps.fix||gps.configured){double x,y;mapScreen(a,gps.latitude,gps.longitude,mapView.lat,mapView.lon,z,x,y);if(mapInside(a,x,y)){v.mapSelfX=lround(x);v.mapSelfY=lround(y);}}
 mapScale(mapView.lat,z,v);
 auto tiles=device::mapTilesState();v.mapStatus="Z"+std::to_string(z);
 if(missing)v.mapStatus+=!tiles.sd?" / NO SD":tiles.online?" / LOADING "+std::to_string(missing):" / NO WI-FI";
 else if(!gps.fix&&mapView.follow)v.mapStatus+=" / NO FIX";
 if(mapZoomLimit)v.mapStatus+=mapZoomLimit<0?" / MAX ZOOM OUT":" / MAX ZOOM IN";
}

// ---- Static packet route (received message -> MAP) ------------------------
// Sender (step 0, unique name with a position), each path hash resolved to a
// unique repeater with a position (1..n), ending at this T-Deck. The picture is
// built once: a low-detail map (zoom <= 12) fitted to the stops, visible tiles
// fetched for at most RouteLoadMs, then frozen. No pan, zoom or buttons.
constexpr int RouteMaxZoom=12;constexpr uint32_t RouteLoadMs=25000;
struct RouteStop {double lat=0,lon=0;bool placed=false;int step=0;std::string name;};
struct RouteView {std::vector<RouteStop> stops;bool self=false,map=false,frozen=true;double selfLat=0,selfLon=0,lat=0,lon=0;int zoom=10;uint32_t openedAt=0;std::string summary;std::vector<std::string> legend;
 std::vector<mm::MapSegment> segments;std::vector<mm::MapPin> pins;int selfX=-1,selfY=-1;} routeView;
static uint8_t *routeBits=nullptr;
static void routeOverlay(){
 auto &r=routeView;r.segments.clear();r.pins.clear();r.selfX=r.selfY=-1;if(!r.map)return;
 struct Point {double x=0,y=0;bool placed=false;};std::vector<Point> points;
 const auto &a=mm::MapArea;for(auto &stop:r.stops){Point p;if(stop.placed){mapScreen(a,stop.lat,stop.lon,r.lat,r.lon,r.zoom,p.x,p.y);p.placed=true;}points.push_back(p);}
 if(r.self){Point p;mapScreen(a,r.selfLat,r.selfLon,r.lat,r.lon,r.zoom,p.x,p.y);p.placed=true;points.push_back(p);if(mapInside(a,p.x,p.y)){r.selfX=lround(p.x);r.selfY=lround(p.y);}}
 int last=-1;for(int i=0;i<int(points.size());i++){if(!points[i].placed)continue;if(last>=0){double ax=points[last].x,ay=points[last].y,bx=points[i].x,by=points[i].y;
   if(device::mapClipSegment(ax,ay,bx,by,a.x,a.y,a.w,a.h)){mm::MapSegment seg;seg.x0=lround(ax);seg.y0=lround(ay);seg.x1=lround(bx);seg.y1=lround(by);seg.dashed=i-last>1;r.segments.push_back(seg);}}last=i;}
 for(int i=0;i<int(r.stops.size());i++){if(!points[i].placed||!mapInside(a,points[i].x,points[i].y))continue;mm::MapPin pin;pin.x=lround(points[i].x);pin.y=lround(points[i].y);pin.step=r.stops[i].step;pin.repeater=pin.step>0;r.pins.push_back(pin);}
}
void routeOpen(const std::string &path,const std::string &sender){
 auto &r=routeView;r=RouteView();matter::NodePin pin;int matches=0;
 RouteStop origin;origin.name=mm::senderDisplayName(sender);origin.placed=!sender.empty()&&matter::nodePinsForName(sender.c_str(),&pin,1,matches)==1&&matches==1;if(origin.placed){origin.lat=pin.lat;origin.lon=pin.lon;}r.stops.push_back(origin);
 int hops=0,placed=0;
 if(path.rfind("flood:",0)==0){std::string rest=path.substr(6);size_t at=0;
  while(at<rest.size()){size_t comma=rest.find(',',at);std::string hash=rest.substr(at,comma==std::string::npos?std::string::npos:comma-at);at=comma==std::string::npos?rest.size():comma+1;if(hash.empty())continue;
   RouteStop stop;stop.step=++hops;stop.placed=matter::repeaterPinsForPrefix(hash.c_str(),&pin,1,matches)==1&&matches==1;stop.name=stop.placed?mm::senderDisplayName(pin.name):"["+hash+"]"+(matches>1?"?":"");if(stop.placed){stop.lat=pin.lat;stop.lon=pin.lon;placed++;}r.stops.push_back(stop);}
  r.summary=hops?std::to_string(hops)+" HOPS / "+std::to_string(placed)+" ON MAP":"DIRECT RECEPTION";
 }else r.summary=path.empty()?"NO PATH SAVED":"PATH NOT RECORDED";
 auto gps=device::locationState();r.self=gps.fix||gps.configured;r.selfLat=gps.latitude;r.selfLon=gps.longitude;
 std::vector<double> lat,lon;for(auto &stop:r.stops)if(stop.placed){lat.push_back(stop.lat);lon.push_back(stop.lon);}if(r.self){lat.push_back(r.selfLat);lon.push_back(r.selfLon);}
 if(lat.size()>1){r.zoom=device::mapFitZoom(lat.data(),lon.data(),lat.size(),mm::MapArea.w,mm::MapArea.h,24,RouteMaxZoom,r.lat,r.lon);r.map=true;}
 else if(lat.size()==1){r.lat=lat[0];r.lon=lon[0];r.zoom=10;r.map=true;}
 for(auto &stop:r.stops){if(stop.step==0){if(!stop.name.empty())r.legend.push_back("S "+stop.name+(stop.placed?"":" (?)"));}else r.legend.push_back(std::to_string(stop.step)+" "+stop.name);}
 r.legend.push_back(r.self?"> this T-Deck":"> this T-Deck (no GPS)");
 routeOverlay();r.frozen=!r.map;r.openedAt=millis();device::mapTilesForget();
}
// Leaving the route picture (CLOSE, Back, lock): drop pending tile downloads, release the
// PSRAM view and the stop/legend lists; routeOpen rebuilds everything from scratch.
void routeClose(){if(!routeView.frozen){routeView.frozen=true;device::mapTilesForget();}if(routeBits){heap_caps_free(routeBits);routeBits=nullptr;}routeView=RouteView();}
bool routeLoading(){return !routeView.frozen;}
void routeCompose(mm::State &v){
 auto &r=routeView;v.messageRouteView=true;v.mapLegend=r.legend;v.mapRoute=r.segments;v.mapPins=r.pins;v.mapSelfX=r.selfX;v.mapSelfY=r.selfY;
 if(!r.map){v.mapStatus=r.summary+" / NO POSITIONS";return;}
 if(!routeBits)routeBits=mapAllocView(mm::MapArea);if(!routeBits){v.mapStatus=r.summary;return;}
 if(!r.frozen){int missing=mapComposeTiles(routeBits,mm::MapArea,r.lat,r.lon,r.zoom,0);
  if(!missing||!device::mapTilesState().online||uint32_t(millis()-r.openedAt)>=RouteLoadMs){r.frozen=true;device::mapTilesForget();}}
 v.mapBits=routeBits;v.mapStatus=r.summary+(r.frozen?"":" / LOADING");mapScale(r.lat,r.zoom,v);
}
