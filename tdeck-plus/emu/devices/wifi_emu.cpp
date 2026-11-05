// Emulator Wi-Fi (emu/stubs/WiFi.h): a virtual access point and real TCP sockets.
// Station: scanNetworks finds the AP after ~1.5 s; begin(ssid,pass) associates after ~0.8 s and gets an address
// after ~0.4 s more (STA_CONNECTED / GOT_IP events from the main-thread pump, as the ESP32 event task would send),
// or fails with AUTH_FAIL / NO_AP_FOUND. The address reported is the PC's LAN address so a phone on the same
// network can reach the emulator's servers. Sockets are non-blocking host sockets.
#include <WiFi.h>
#include <emu_hal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
WiFiClass WiFi;
namespace {
std::string apSsid="EMU-WiFi",apPassword="meshcore";int portOffset=0;
wifi_mode_t mode_=WIFI_OFF;wl_status_t status_=WL_DISCONNECTED;std::string joinSsid,joinPassword;
enum class Join {Idle,Associating,Addressing,Up} join=Join::Idle;uint32_t joinAt=0;
int16_t scanState=WIFI_SCAN_FAILED;uint32_t scanStarted=0;
WiFiEventFuncCb events;bool hooked=false;
void emit(arduino_event_id_t id,uint8_t reason=0){if(!events)return;arduino_event_info_t info{};info.wifi_sta_disconnected.reason=reason;events(id,info);}
void fail(uint8_t reason,wl_status_t s){join=Join::Idle;status_=s;emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,reason);}
// Station state machine, run from the main-thread pump.
void tick(){
 uint32_t now=millis();
 if(scanState==WIFI_SCAN_RUNNING&&now-scanStarted>=1500)scanState=1;
 if(join==Join::Associating&&now-joinAt>=800){
  if(joinSsid!=apSsid){fail(WIFI_REASON_NO_AP_FOUND,WL_NO_SSID_AVAIL);return;}
  if(!apPassword.empty()&&joinPassword!=apPassword){fail(WIFI_REASON_AUTH_FAIL,WL_CONNECT_FAILED);return;}
  join=Join::Addressing;joinAt=now;emit(ARDUINO_EVENT_WIFI_STA_CONNECTED);}
 if(join==Join::Addressing&&now-joinAt>=400){join=Join::Up;status_=WL_CONNECTED;emit(ARDUINO_EVENT_WIFI_STA_GOT_IP);}
}
void hook(){if(!hooked){hooked=true;emu::addPumpHook(tick);}}
// First non-loopback IPv4 address of the PC (127.0.0.1 if none).
IPAddress lanAddress(){
 ifaddrs *list=nullptr;IPAddress out(127,0,0,1);if(getifaddrs(&list)!=0)return out;
 for(auto *i=list;i;i=i->ifa_next){if(!i->ifa_addr||i->ifa_addr->sa_family!=AF_INET||(i->ifa_flags&0x8/*IFF_LOOPBACK*/))continue;
  uint32_t a=reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr.s_addr;out=IPAddress(reinterpret_cast<uint8_t*>(&a));break;}
 freeifaddrs(list);return out;
}
void nonBlocking(int fd){fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);}
}
namespace emuwifi {
void configure(const char *ssid,const char *password,int offset){if(ssid)apSsid=ssid;apPassword=password?password:"";portOffset=offset;}
}

// ---- WiFiClass ----
bool WiFiClass::mode(wifi_mode_t m){hook();mode_=m;if(m==WIFI_OFF){join=Join::Idle;status_=WL_DISCONNECTED;}return true;}
wifi_mode_t WiFiClass::getMode(){return mode_;}
wl_status_t WiFiClass::begin(const char *ssid,const char *password){
 hook();if(mode_==WIFI_OFF)mode_=WIFI_STA;joinSsid=ssid?ssid:"";joinPassword=password?password:"";
 join=Join::Associating;joinAt=millis();status_=WL_DISCONNECTED;return status_;
}
bool WiFiClass::disconnect(bool wifiOff,bool){bool was=join==Join::Up;join=Join::Idle;status_=WL_DISCONNECTED;if(wifiOff)mode_=WIFI_OFF;if(was)emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,WIFI_REASON_ASSOC_LEAVE);return true;}
wl_status_t WiFiClass::status(){return mode_==WIFI_OFF?WL_NO_SHIELD:status_;}
int WiFiClass::onEvent(WiFiEventFuncCb cb){hook();events=cb;return 1;}
int16_t WiFiClass::scanNetworks(bool async,bool){hook();if(mode_==WIFI_OFF)return WIFI_SCAN_FAILED;scanStarted=millis();scanState=async?WIFI_SCAN_RUNNING:1;return scanState;}
int16_t WiFiClass::scanComplete(){return scanState;}
void WiFiClass::scanDelete(){scanState=WIFI_SCAN_FAILED;}
String WiFiClass::SSID(){return join==Join::Up?String(joinSsid.c_str()):String();}
String WiFiClass::SSID(uint8_t i){return i==0&&scanState>0?String(apSsid.c_str()):String();}
String WiFiClass::psk(){return join==Join::Up?String(joinPassword.c_str()):String();}
int32_t WiFiClass::RSSI(){return join==Join::Up?-55:0;}
int32_t WiFiClass::RSSI(uint8_t i){return i==0&&scanState>0?-55:0;}
wifi_auth_mode_t WiFiClass::encryptionType(uint8_t){return apPassword.empty()?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK;}
uint8_t *WiFiClass::BSSID(uint8_t){static uint8_t b[6]={0x02,0x45,0x4D,0x55,0x41,0x50};return b;}
IPAddress WiFiClass::localIP(){return join==Join::Up?lanAddress():IPAddress(0,0,0,0);}
uint8_t *WiFiClass::macAddress(uint8_t *mac){static const uint8_t m[6]={0x02,0x4D,0x4D,0x45,0x4D,0x55};memcpy(mac,m,6);return mac;}
String WiFiClass::macAddress(){return "02:4D:4D:45:4D:55";}

// ---- sockets ----
struct WiFiClient::Socket {
 int fd=-1;int peeked=-1;
 ~Socket(){if(fd>=0)close(fd);}
 bool alive(){if(fd<0)return false;if(peeked>=0)return true;uint8_t b;ssize_t n=recv(fd,&b,1,MSG_PEEK|MSG_DONTWAIT);
  if(n==0){close(fd);fd=-1;return false;}if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK){close(fd);fd=-1;return false;}return true;}
};
WiFiClient::WiFiClient(){}
WiFiClient::WiFiClient(std::shared_ptr<Socket> s):sock(std::move(s)){}
WiFiClient::~WiFiClient(){}
int WiFiClient::connect(const char *host,uint16_t port){return connect(host,port,3000);}
int WiFiClient::connect(IPAddress ip,uint16_t port){return connect(ip.toString().c_str(),port,3000);}
int WiFiClient::connect(const char *host,uint16_t port,int32_t timeoutMs){
 stop();if(WiFi.status()!=WL_CONNECTED)return 0;
 addrinfo hints{},*res=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
 if(getaddrinfo(host,std::to_string(port).c_str(),&hints,&res)!=0||!res)return 0;
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0){freeaddrinfo(res);return 0;}nonBlocking(fd);
 int r=::connect(fd,res->ai_addr,res->ai_addrlen);freeaddrinfo(res);
 if(r<0&&errno==EINPROGRESS){pollfd p{fd,POLLOUT,0};int err=0;socklen_t len=sizeof(err);
  if(poll(&p,1,timeoutMs)<=0||getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&len)<0||err){close(fd);return 0;}}
 else if(r<0){close(fd);return 0;}
 sock=std::make_shared<Socket>();sock->fd=fd;return 1;
}
int WiFiClient::fd() const{return sock?sock->fd:-1;}
IPAddress WiFiClient::remoteIP() const{sockaddr_in a{};socklen_t n=sizeof(a);if(!sock||sock->fd<0||getpeername(sock->fd,reinterpret_cast<sockaddr*>(&a),&n)<0||a.sin_family!=AF_INET)return IPAddress(0,0,0,0);uint32_t v=ntohl(a.sin_addr.s_addr);return IPAddress(v>>24,(v>>16)&255,(v>>8)&255,v&255);}
uint8_t WiFiClient::connected(){return sock&&(sock->alive()||available()>0);}
void WiFiClient::stop(){if(sock&&sock->fd>=0){close(sock->fd);sock->fd=-1;}sock.reset();}
void WiFiClient::setNoDelay(bool on){if(sock&&sock->fd>=0){int v=on;setsockopt(sock->fd,IPPROTO_TCP,TCP_NODELAY,&v,sizeof(v));}}
int WiFiClient::available(){if(!sock||sock->fd<0)return sock&&sock->peeked>=0?1:0;int n=0;uint8_t buf[4096];ssize_t r=recv(sock->fd,buf,sizeof(buf),MSG_PEEK|MSG_DONTWAIT);if(r>0)n=int(r);return n+(sock->peeked>=0?1:0);}
int WiFiClient::read(){uint8_t b;return read(&b,1)==1?b:-1;}
int WiFiClient::read(uint8_t *buf,size_t n){
 if(!sock||!n)return -1;size_t k=0;if(sock->peeked>=0){buf[k++]=uint8_t(sock->peeked);sock->peeked=-1;}
 if(k<n&&sock->fd>=0){ssize_t r=recv(sock->fd,buf+k,n-k,MSG_DONTWAIT);if(r>0)k+=size_t(r);else if(r==0){close(sock->fd);sock->fd=-1;}}
 return k?int(k):-1;
}
int WiFiClient::peek(){if(!sock)return -1;if(sock->peeked<0&&sock->fd>=0){uint8_t b;if(recv(sock->fd,&b,1,MSG_DONTWAIT)==1)sock->peeked=b;}return sock->peeked;}
size_t WiFiClient::write(uint8_t b){return write(&b,1);}
size_t WiFiClient::write(const uint8_t *buf,size_t n){
 if(!sock||sock->fd<0)return 0;size_t done=0;uint32_t start=millis();
 while(done<n&&millis()-start<2000){ssize_t r=send(sock->fd,buf+done,n-done,MSG_NOSIGNAL|MSG_DONTWAIT);
  if(r>0)done+=size_t(r);else if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){pollfd p{sock->fd,POLLOUT,0};poll(&p,1,20);}else break;}
 return done;
}
WiFiServer::WiFiServer(uint16_t p):port(p){}
WiFiServer::~WiFiServer(){end();}
void WiFiServer::begin(){
 end();fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return;int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
 sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(uint16_t(port+portOffset));
 if(bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0||listen(fd,4)<0){fprintf(stderr,"EMU_WIFI cannot listen on port %d: %s\n",port+portOffset,strerror(errno));close(fd);fd=-1;return;}
 nonBlocking(fd);printf("EMU_WIFI listening tcp=%d\n",port+portOffset);fflush(stdout);
}
void WiFiServer::end(){if(fd>=0){close(fd);fd=-1;}}
WiFiClient WiFiServer::available(){
 if(fd<0||WiFi.status()!=WL_CONNECTED)return WiFiClient();int c=::accept(fd,nullptr,nullptr);if(c<0)return WiFiClient();
 nonBlocking(c);if(noDelay){int v=1;setsockopt(c,IPPROTO_TCP,TCP_NODELAY,&v,sizeof(v));}
 auto s=std::make_shared<WiFiClient::Socket>();s->fd=c;return WiFiClient(s);
}
