#include "web_panel.h"
#include "m9_devices.h"
#include <Arduino.h>
#include <emu_hal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
namespace webpanel {
namespace {
std::string pageDir,skinPath,secret;
std::mutex m;std::condition_variable changed;
std::vector<uint16_t> shown;int shownW=0,shownH=0;uint32_t seq=1;int light=0,kbd=0;std::deque<uint8_t> pending;
uint32_t lastCopy=0;

// Main thread (pump hook): hand the queued key bytes to the keyboard chip, publish the LCD when it changed.
void pump(){
 std::deque<uint8_t> keys;{std::lock_guard<std::mutex> l(m);keys.swap(pending);}
 for(uint8_t b:keys){
  if(b>=0xB4&&b<=0xB7){static const int dx[]={-1,0,0,1},dy[]={0,-1,1,0};m9dev::roll(dx[b-0xB4],dy[b-0xB4]);}
  else m9dev::key(300+b,true);
 }
 uint32_t now=millis();if(uint32_t(now-lastCopy)<50)return;lastCopy=now;
 int w=m9dev::frameWidth(),h=m9dev::frameHeight();const uint16_t *p=m9dev::frame();
 int lv=m9dev::displayOn()?m9dev::backlight():0,kb=m9dev::keyboardLight();
 std::lock_guard<std::mutex> l(m);
 if(w==shownW&&h==shownH&&lv==light&&kb==kbd&&!memcmp(shown.data(),p,size_t(w)*h*2))return;
 shown.assign(p,p+size_t(w)*h);shownW=w;shownH=h;light=lv;kbd=kb;seq++;changed.notify_all();
}

// Runs of one colour as (count, colour) uint16 pairs: a UI frame shrinks 10-30x, which matters over a phone's network.
std::string pack(const std::vector<uint16_t> &px){
 std::vector<uint16_t> out;out.reserve(px.size()/4);
 for(size_t i=0;i<px.size();){size_t j=i+1;while(j<px.size()&&px[j]==px[i]&&j-i<65535)j++;out.push_back(uint16_t(j-i));out.push_back(px[i]);i=j;if(out.size()>px.size())break;}
 return std::string(reinterpret_cast<const char*>(out.data()),out.size()*2);
}
std::string readFile(const std::string &path){std::ifstream f(path,std::ios::binary);if(!f)return {};std::ostringstream s;s<<f.rdbuf();return s.str();}
void reply(int fd,const char *status,const char *type,const std::string &body,const std::string &extra=""){
 std::string head=std::string("HTTP/1.1 ")+status+"\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(body.size())+
  "\r\nCache-Control: no-store\r\nConnection: close\r\n"+extra+"\r\n";
 std::string all=head+body;size_t off=0;while(off<all.size()){ssize_t n=send(fd,all.data()+off,all.size()-off,MSG_NOSIGNAL);if(n<=0)break;off+=size_t(n);}
}
std::string query(const std::string &target,const char *key){
 auto q=target.find('?');if(q==std::string::npos)return {};std::string k=std::string(key)+"=";
 for(size_t at=q+1;at<target.size();){size_t end=target.find('&',at);if(end==std::string::npos)end=target.size();
  if(!target.compare(at,k.size(),k))return target.substr(at+k.size(),end-at-k.size());at=end+1;}
 return {};
}
void serve(int fd){
 char buf[2048];size_t got=0;
 while(got<sizeof(buf)-1){ssize_t n=recv(fd,buf+got,sizeof(buf)-1-got,0);if(n<=0)break;got+=size_t(n);buf[got]=0;if(strstr(buf,"\r\n\r\n"))break;}
 buf[got]=0;char method[8]={},target[512]={};if(sscanf(buf,"%7s %511s",method,target)!=2){close(fd);return;}
 std::string t=target,path=t.substr(0,t.find('?'));
 // With a secret (--web-key) every address starts with /SECRET/: a link shared over a public tunnel is the only way in.
 if(!secret.empty()){std::string pre="/"+secret+"/";if(path.compare(0,pre.size(),pre)){reply(fd,"404 Not Found","text/plain","not found\n");close(fd);return;}path=path.substr(pre.size()-1);}
 if(path=="/"||path=="/index.html"){std::string page=readFile(pageDir+"/m9.html");
  if(page.empty())reply(fd,"404 Not Found","text/plain","emu/web/m9.html missing\n");else reply(fd,"200 OK","text/html; charset=utf-8",page);}
 else if(path=="/skin.png"){std::string png=readFile(skinPath);if(png.empty())reply(fd,"404 Not Found","text/plain","no --skin picture\n");else reply(fd,"200 OK","image/png",png);}
 else if(path=="/key"){std::string c=query(t,"c");unsigned v=0;
  if(c.size()==2&&sscanf(c.c_str(),"%2x",&v)==1){{std::lock_guard<std::mutex> l(m);pending.push_back(uint8_t(v));}reply(fd,"200 OK","text/plain","ok\n");}
  else reply(fd,"400 Bad Request","text/plain","key?c=HH\n");}
 else if(path=="/frame"){uint32_t since=uint32_t(strtoul(query(t,"since").c_str(),nullptr,10));
  std::unique_lock<std::mutex> l(m);changed.wait_for(l,std::chrono::seconds(1),[&]{return seq!=since&&shownW>0;});
  if(seq==since||!shownW){l.unlock();reply(fd,"204 No Content","text/plain","");}
  else{std::string body=pack(shown),enc=body.size()<shown.size()*2?"rle":"raw";
   if(enc=="raw")body.assign(reinterpret_cast<const char*>(shown.data()),shown.size()*2);
   std::string extra="X-Enc: "+enc+"\r\nX-Seq: "+std::to_string(seq)+"\r\nX-W: "+std::to_string(shownW)+"\r\nX-H: "+std::to_string(shownH)+
    "\r\nX-Light: "+std::to_string(light)+"\r\nX-Kbd: "+std::to_string(kbd)+"\r\nAccess-Control-Expose-Headers: X-Enc, X-Seq, X-W, X-H, X-Light, X-Kbd\r\n";
   l.unlock();reply(fd,"200 OK","application/octet-stream",body,extra);}}
 else reply(fd,"404 Not Found","text/plain","not found\n");
 close(fd);
}
}

bool start(int port,const std::string &dir,const std::string &skin,const std::string &key){
 pageDir=dir;skinPath=skin;secret=key;
 int s=socket(AF_INET,SOCK_STREAM,0);if(s<0)return false;int on=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&on,sizeof on);
 sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(uint16_t(port));a.sin_addr.s_addr=htonl(INADDR_ANY);// phone on the same Wi-Fi
 if(bind(s,reinterpret_cast<sockaddr*>(&a),sizeof a)||listen(s,16)){perror("web panel");close(s);return false;}
 emu::addPumpHook(pump);
 std::thread([s]{for(;;){int fd=accept(s,nullptr,nullptr);if(fd<0)continue;std::thread(serve,fd).detach();}}).detach();
 printf("EMU_WEB http://<this PC>:%d/%s (skin %s)\n",port,key.empty()?"":"<secret>/",skin.empty()?"none":skin.c_str());fflush(stdout);
 return true;
}
}
