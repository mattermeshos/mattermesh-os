// Maps app (section 9): map tiles from services/map_tiles (OpenStreetMap colour),
// own GPS position and MeshCore nodes that advertise a position. Included by main.cpp.
// Also the static packet-route picture shown from a received message (MAP).
struct MapView {int zoom=13;double lat=0,lon=0;bool follow=false,ready=false;} mapView;
// Zoom limit (z6 out, z16 in): the map stays put and the status label says so until the next pan or zoom.
static int mapZoomLimit=0;// -1 out, +1 in
// Map look (Plus): 0 LIGHT = OpenStreetMap as served, 1 NIGHT = the same tiles darkened on the device
// (services/map_math.h mapNightLook), so it also works offline on the cached tiles. NVS "mmmap"/"style".
int mapStyleIndex=0;
void loadMapStyle(){mapStyleIndex=device::prefGetInt("mmmap","style",0)?1:0;}
void mapToggleStyle(){mapStyleIndex^=1;device::prefPutInt("mmmap","style",mapStyleIndex);render();}
static uint8_t *mapViewBits=nullptr;static int mapVisibleCount=0,mapMissingCount=0;
static struct {double lat=0,lon=0;int z=-1,style=-1;uint32_t rev=0;} composed;// what mapViewBits holds (z=-1: nothing)
constexpr size_t MapViewBytes=(size_t(mm::MapArea.w)*mm::MapArea.h*2);// RGB565 view on the Plus
static uint8_t *mapAllocView(){return static_cast<uint8_t*>(heap_caps_malloc(MapViewBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));}
// Composes the tiles around (lat,lon,z) into bits; returns how many visible tiles are missing.
static int mapComposeTiles(uint8_t *bits,double lat,double lon,int z,bool reuse=false){// reuse: bits already hold this view
 const int w=mm::MapArea.w,h=mm::MapArea.h;auto center=device::mapProject(lat,lon,z);device::mapTilesBeginPass();
 device::MapTileRef refs[32];int visible=device::mapVisibleTiles(center,z,w,h,0,refs,32);
 if(reuse)return 0;
 {uint16_t *view=reinterpret_cast<uint16_t*>(bits);for(int i=0;i<w*h;i++)view[i]=mm::Mist;}int missing=0;// missing tiles show the theme mist
 for(int i=0;i<visible;i++){const uint8_t *tile=device::mapTile(z,refs[i].x,refs[i].y,true);if(!tile){missing++;continue;}
  device::mapBlitColor(reinterpret_cast<const uint16_t*>(tile),refs[i].screenX,refs[i].screenY,reinterpret_cast<uint16_t*>(bits),w,h);}
 if(mapStyleIndex==1)device::mapNightLook(reinterpret_cast<uint16_t*>(bits),w*h);
 mapVisibleCount=visible;return missing;
}
// Screen position inside MapArea for a view centred on (clat,clon) at zoom z (unclipped).
static void mapScreen(double lat,double lon,double clat,double clon,int z,double &x,double &y){
 auto c=device::mapProject(clat,clon,z),p=device::mapProject(lat,lon,z);double dx=p.x-c.x,world=device::mapWorldSize(z);if(dx>world/2)dx-=world;else if(dx<-world/2)dx+=world;
 x=mm::MapArea.x+mm::MapArea.w/2+dx;y=mm::MapArea.y+mm::MapArea.h/2+(p.y-c.y);
}
static bool mapInside(double x,double y){return x>=mm::MapArea.x&&x<mm::MapArea.x+mm::MapArea.w&&y>=mm::MapArea.y&&y<mm::MapArea.y+mm::MapArea.h;}
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
void mapClose(){device::mapTilesForget();composed.z=-1;if(mapViewBits){heap_caps_free(mapViewBits);mapViewBits=nullptr;}mapVisibleCount=mapMissingCount=0;}
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
// W/A/S/D pan a third of the view, I/O (or +/-) zoom, C or ENTER centre on GPS.
bool mapKey(char c){
 switch(c){case 'w':mapPan(0,-mm::MapArea.h/3);return true;case 's':mapPan(0,mm::MapArea.h/3);return true;case 'a':mapPan(-mm::MapArea.w/3,0);return true;case 'd':mapPan(mm::MapArea.w/3,0);return true;
  case 'i':case '+':case '=':mapZoom(1);return true;case 'o':case '-':mapZoom(-1);return true;case 'c':case '\n':mapCenter();return true;case 'm':{mapToggleStyle();return true;}}
 return false;
}
// Buttons: zoom out / centre / zoom in. A tap on the map brings that point to the centre.
bool mapTap(int x,int y){
 {if(mm::MapBackFloat.contains(x,y)){back();return true;}if(mm::MapStyleButton.contains(x,y)){mapToggleStyle();return true;}}
 if(mm::mapButtonRect(0).contains(x,y)){mapZoom(-1);return true;}if(mm::mapButtonRect(1).contains(x,y)){mapCenter();return true;}if(mm::mapButtonRect(2).contains(x,y)){mapZoom(1);return true;}
 if(mm::MapArea.contains(x,y)){mapPan(x-(mm::MapArea.x+mm::MapArea.w/2),y-(mm::MapArea.y+mm::MapArea.h/2));return true;}
 return false;
}
void mapCompose(mm::State &v){
 if(!mapViewBits){mapViewBits=mapAllocView();composed.z=-1;}if(!mapViewBits)return;
 auto gps=device::locationState();if(mapView.follow&&gps.fix){mapView.lat=gps.latitude;mapView.lon=gps.longitude;}
 const int w=mm::MapArea.w,h=mm::MapArea.h,z=mapView.zoom;
 // Plus: a frame that only moves pins or the clock reuses the composed view (fill + blit + NIGHT look).
uint32_t rev=device::mapTilesRevision();
 bool reuse=composed.z==z&&composed.lat==mapView.lat&&composed.lon==mapView.lon&&composed.style==mapStyleIndex&&composed.rev==rev;
 int missing=mapComposeTiles(mapViewBits,mapView.lat,mapView.lon,z,reuse);
 if(!reuse){if(missing==0){composed.lat=mapView.lat;composed.lon=mapView.lon;composed.z=z;composed.style=mapStyleIndex;composed.rev=rev;}else composed.z=-1;}
 mapMissingCount=missing;v.mapStyle=mapStyleIndex;v.mapPixels=reinterpret_cast<const uint16_t*>(mapViewBits);v.mapFollow=mapView.follow;
 auto center=device::mapProject(mapView.lat,mapView.lon,z);double north,west,south,east;device::mapUnproject({center.x-w/2.0,center.y-h/2.0},z,north,west);device::mapUnproject({center.x+w/2.0,center.y+h/2.0},z,south,east);
 matter::NodePin pins[24];int count=matter::nodePins(south,west,north,east,pins,24);
 for(int i=0;i<count;i++){double x,y;mapScreen(pins[i].lat,pins[i].lon,mapView.lat,mapView.lon,z,x,y);if(!mapInside(x,y))continue;mm::MapPin pin;pin.x=lround(x);pin.y=lround(y);pin.repeater=pins[i].repeater;pin.name=mm::senderDisplayName(pins[i].name);v.mapPins.push_back(pin);}
 if(gps.fix||gps.configured){double x,y;mapScreen(gps.latitude,gps.longitude,mapView.lat,mapView.lon,z,x,y);if(mapInside(x,y)){v.mapSelfX=lround(x);v.mapSelfY=lround(y);}}
 mapScale(mapView.lat,z,v);
 auto tiles=device::mapTilesState();v.mapStatus="Z"+std::to_string(z);
 if(mapZoomLimit)v.mapStatus+=mapZoomLimit<0?" / MAX ZOOM OUT":" / MAX ZOOM IN";
 if(missing)v.mapStatus+=!tiles.sd?" / NO SD":tiles.online?" / LOADING "+std::to_string(missing):" / NO WI-FI";
 else if(!gps.fix&&mapView.follow)v.mapStatus+=" / NO FIX";
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
 for(auto &stop:r.stops){Point p;if(stop.placed){mapScreen(stop.lat,stop.lon,r.lat,r.lon,r.zoom,p.x,p.y);p.placed=true;}points.push_back(p);}
 if(r.self){Point p;mapScreen(r.selfLat,r.selfLon,r.lat,r.lon,r.zoom,p.x,p.y);p.placed=true;points.push_back(p);if(mapInside(p.x,p.y)){r.selfX=lround(p.x);r.selfY=lround(p.y);}}
 int last=-1;for(int i=0;i<int(points.size());i++){if(!points[i].placed)continue;if(last>=0){double ax=points[last].x,ay=points[last].y,bx=points[i].x,by=points[i].y;
   if(device::mapClipSegment(ax,ay,bx,by,mm::MapArea.x,mm::MapArea.y,mm::MapArea.w,mm::MapArea.h)){mm::MapSegment seg;seg.x0=lround(ax);seg.y0=lround(ay);seg.x1=lround(bx);seg.y1=lround(by);seg.dashed=i-last>1;r.segments.push_back(seg);}}last=i;}
 for(int i=0;i<int(r.stops.size());i++){if(!points[i].placed||!mapInside(points[i].x,points[i].y))continue;mm::MapPin pin;pin.x=lround(points[i].x);pin.y=lround(points[i].y);pin.step=r.stops[i].step;pin.repeater=pin.step>0;r.pins.push_back(pin);}
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
 if(!routeBits)routeBits=mapAllocView();if(!routeBits){v.mapStatus=r.summary;return;}
 if(!r.frozen){int missing=mapComposeTiles(routeBits,r.lat,r.lon,r.zoom);
  if(!missing||!device::mapTilesState().online||uint32_t(millis()-r.openedAt)>=RouteLoadMs){r.frozen=true;device::mapTilesForget();}}
 v.mapPixels=reinterpret_cast<const uint16_t*>(routeBits);v.mapStatus=r.summary+(r.frozen?"":" / LOADING");mapScale(r.lat,r.zoom,v);
}
