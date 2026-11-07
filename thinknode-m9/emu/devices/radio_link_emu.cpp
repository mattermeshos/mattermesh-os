// Emulator companion link (services/companion_ble.h): instead of BLE, a TCP connection to the companion port of
// another emulator (frames '<' len16 out, '>' len16 in, as the app over Wi-Fi). The "scan" finds one companion
// when MM_EMU_LINK=host:port is set (name MM_EMU_LINK_NAME, default "MeshCore-Alpha"); pairing succeeds only when
// the PIN given to radioLinkConnect equals MM_EMU_LINK_PIN (default 123456), so tests can check a wrong PIN.
#include "services/companion_ble.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstring>
namespace device {
namespace {
std::mutex lock;RadioLinkStatus status;std::vector<RadioLinkPeer> peers;std::deque<std::vector<uint8_t>> inbox;
std::atomic<int> fd{-1};std::thread reader;std::atomic<bool> stopping{false};std::mutex connecting;// one connector thread at a time owns reader
std::atomic<uint32_t> lost{0};std::atomic<bool> wantedOff{false};
void setPhase(RadioLinkPhase p,const char *message){std::lock_guard<std::mutex> h(lock);status.phase=p;status.message=message;status.revision++;}
const char *env(const char *k,const char *d){const char *v=getenv(k);return v&&*v?v:d;}
bool readAll(int s,uint8_t *b,size_t n){size_t got=0;while(got<n){ssize_t r=recv(s,b+got,n-got,0);if(r<=0)return false;got+=size_t(r);}return true;}
void closeSocket(){int s=fd.exchange(-1);if(s>=0){shutdown(s,SHUT_RDWR);close(s);}if(reader.joinable()&&reader.get_id()!=std::this_thread::get_id())reader.join();}
void readLoop(int s){
 for(;;){uint8_t h[3];if(!readAll(s,h,1))break;if(h[0]!='>')continue;if(!readAll(s,h+1,2))break;size_t n=size_t(h[1])|size_t(h[2])<<8;std::vector<uint8_t> f(n);if(n&&!readAll(s,f.data(),n))break;
  std::lock_guard<std::mutex> g(lock);if(inbox.size()<512)inbox.push_back(std::move(f));else lost++;}
 if(!stopping){std::lock_guard<std::mutex> g(lock);if(status.phase==RadioLinkPhase::Connected){status.drops++;status.phase=RadioLinkPhase::Failed;status.message="Disconnected";status.revision++;}}}
}
void radioLinkScan(uint32_t ms){
 {std::lock_guard<std::mutex> h(lock);peers.clear();}setPhase(RadioLinkPhase::Scanning,"Searching...");
 std::thread([ms]{usleep(std::min<uint32_t>(ms,400)*1000);std::lock_guard<std::mutex> h(lock);
  if(getenv("MM_EMU_LINK")){RadioLinkPeer p;p.name=env("MM_EMU_LINK_NAME","MeshCore-Alpha");p.address=env("MM_EMU_LINK","");p.rssi=-55;peers.push_back(p);}
  if(status.phase==RadioLinkPhase::Scanning){status.phase=RadioLinkPhase::Off;status.message=peers.empty()?"No companion found":"1 found";status.revision++;}}).detach();}
std::vector<RadioLinkPeer> radioLinkPeers(){std::lock_guard<std::mutex> h(lock);return peers;}
void radioLinkConnect(const RadioLinkPeer &peer,uint32_t pin){
 {std::lock_guard<std::mutex> h(lock);if(status.phase==RadioLinkPhase::Connected&&fd>=0&&status.address==peer.address)return;}/* already linked: as the device, nothing to do */
 wantedOff=false;stopping=true;closeSocket();stopping=false;setPhase(RadioLinkPhase::Connecting,"Connecting...");
 std::string target=peer.address.c_str();uint32_t expected=uint32_t(strtoul(env("MM_EMU_LINK_PIN","123456"),nullptr,10));std::string name=peer.name.c_str();
 std::thread([target,pin,expected,name]{std::lock_guard<std::mutex> one(connecting);
  size_t colon=target.rfind(':');if(colon==std::string::npos){setPhase(RadioLinkPhase::Failed,"Companion not reachable");return;}
  sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(uint16_t(atoi(target.c_str()+colon+1)));inet_pton(AF_INET,target.substr(0,colon).c_str(),&a.sin_addr);
  int s=socket(AF_INET,SOCK_STREAM,0);if(s<0||connect(s,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0){if(s>=0)close(s);setPhase(RadioLinkPhase::Failed,"Companion not reachable");return;}
  usleep(150*1000);setPhase(RadioLinkPhase::Connecting,"Pairing...");usleep(150*1000);
  if(pin!=expected){close(s);setPhase(RadioLinkPhase::Failed,"Pairing failed: check the PIN");return;}
  {std::lock_guard<std::mutex> g(lock);inbox.clear();status.connects++;status.name=name.c_str();status.address=target.c_str();}
  if(stopping||wantedOff){close(s);return;}/* Off during the connect, as the device */fd=s;reader=std::thread(readLoop,s);setPhase(RadioLinkPhase::Connected,"Connected");}).detach();}
void radioLinkDisconnect(bool){wantedOff=true;stopping=true;closeSocket();stopping=false;{std::lock_guard<std::mutex> g(lock);inbox.clear();}setPhase(RadioLinkPhase::Off,"Off");}
void radioLinkForgetBond(const String&,uint8_t){}
bool radioLinkHoldsStack(){return false;}
uint32_t radioLinkRevision(){std::lock_guard<std::mutex> h(lock);return status.revision;}
RadioLinkStatus radioLinkStatus(){std::lock_guard<std::mutex> h(lock);RadioLinkStatus s=status;s.lost=lost;return s;}
bool radioLinkTake(uint8_t *frame,size_t &len){std::lock_guard<std::mutex> h(lock);while(!inbox.empty()&&inbox.front().size()>184){inbox.pop_front();lost++;}/* as the device ring: oversize frames dropped */
 if(inbox.empty())return false;auto &f=inbox.front();memcpy(frame,f.data(),f.size());len=f.size();inbox.pop_front();return true;}
bool radioLinkSend(const uint8_t *frame,size_t len){int s=fd;if(s<0||!len||len>0xFFFF)return false;std::vector<uint8_t> b(3+len);b[0]='<';b[1]=uint8_t(len&0xff);b[2]=uint8_t(len>>8);memcpy(b.data()+3,frame,len);
 return send(s,b.data(),b.size(),MSG_NOSIGNAL)==ssize_t(b.size());}
}
