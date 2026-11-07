#include "air_protocol.h"
#include <cerrno>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
volatile sig_atomic_t stopping=0;
void stop(int) { stopping=1; }
struct Client { int fd; std::string name,input,output; size_t sent=0; bool dead=false; };
}
int main(int argc,char** argv) {
  try {
    unsigned port=7300; std::string topologyPath,logPath;
    for(int i=1;i<argc;++i) {
      std::string arg=argv[i];
      if(i+1>=argc) throw std::runtime_error("expected --port N, --topology FILE or --log FILE");
      std::string value=argv[++i];
      if(arg=="--port") {
        size_t end; auto n=std::stoul(value,&end);
        if(end!=value.size() || !n || n>65535) throw std::runtime_error("invalid port");
        port=unsigned(n);
      } else if(arg=="--topology") topologyPath=value;
      else if(arg=="--log") logPath=value;
      else throw std::runtime_error("unknown option: "+arg);
    }
    air::Medium medium;
    if(!topologyPath.empty()) {
      std::ifstream file(topologyPath); if(!file) throw std::runtime_error("cannot open topology");
      medium.topology.read(file);
    }
    std::ofstream log;
    if(!logPath.empty()) { log.open(logPath,std::ios::app); if(!log) throw std::runtime_error("cannot open log"); }
    uint64_t epoch=air::nowUs();
    medium.log=[&](const air::Medium::Flight& f) {
      if(!log.is_open()) return;
      log << (air::nowUs()-epoch) << "us sender=" << f.sender << " receivers=";
      for(auto& r:f.receivers) log << r.name << ':' << r.link.rssi << '/' << r.link.snr << '/' << (!r.link.enabled?"unheard":!r.heard?"loss":r.ok?"ok":"crc") << ',';
      log << " length=" << f.frame.payload.size() << " hex=" << air::hex(f.frame.payload) << '\n'; log.flush();
    };
    int listener=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
    if(listener<0) throw std::runtime_error("socket failed");
    int yes=1; setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(uint16_t(port));
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) || listen(listener,64)) {
      close(listener); throw std::runtime_error("cannot bind/listen on loopback port");
    }
    std::signal(SIGTERM,stop); std::signal(SIGINT,stop);
    std::vector<std::shared_ptr<Client>> clients;
    std::cout << "emu-air listening on 127.0.0.1:" << port << std::endl;
    while(!stopping) {
      medium.pump(air::nowUs());
      std::vector<pollfd> polls{{listener,POLLIN,0}};
      for(auto& c:clients) polls.push_back({c->fd,short(POLLIN|(c->sent<c->output.size()?POLLOUT:0)),0});
      int rc=::poll(polls.data(),polls.size(),2);
      if(rc<0) { if(errno==EINTR) continue; throw std::runtime_error("poll failed"); }
      // Existing descriptors correspond to the snapshot; accept only after processing them.
      for(size_t i=0;i<clients.size();++i) {
        auto c=clients[i]; short events=polls[i+1].revents;
        if(events&(POLLERR|POLLHUP|POLLNVAL)) c->dead=true;
        if(!c->dead && (events&POLLOUT)) {
          auto n=send(c->fd,c->output.data()+c->sent,c->output.size()-c->sent,MSG_NOSIGNAL);
          if(n>0) c->sent+=size_t(n);
          else if(n==0 || (errno!=EAGAIN && errno!=EINTR)) c->dead=true;
          if(c->sent==c->output.size()) {c->output.clear(); c->sent=0;}
        }
        if(!c->dead && (events&POLLIN)) {
          char b[4096]; auto n=recv(c->fd,b,sizeof(b),0);
          if(n>0) c->input.append(b,size_t(n));
          else if(n==0 || (errno!=EAGAIN && errno!=EINTR)) c->dead=true;
          if(c->input.size()>65536) c->dead=true;
          size_t end;
          while(!c->dead && (end=c->input.find('\n'))!=std::string::npos) {
            std::istringstream line(c->input.substr(0,end)); c->input.erase(0,end+1);
            std::string kind; line>>kind;
            if(c->name.empty()) {
              unsigned version=0; std::string name,extra;
              if(kind!="HELLO" || !(line>>version>>name) || version!=1 || !air::validName(name) || (line>>extra)) {c->dead=true; break;}
              bool duplicate=false;
              for(auto& other:clients) if(other!=c && other->name==name) duplicate=true;
              if(duplicate) {c->dead=true; break;}
              c->name=name; c->output+="READY 1\n";
              std::weak_ptr<Client> weak=c;
              auto enqueue=[weak](const std::string& s) {if(auto p=weak.lock()) {
                if(p->output.size()-p->sent+s.size()>1048576) p->dead=true;
                else {
                  if(p->sent>65536) { p->output.erase(0,p->sent); p->sent=0; }
                  p->output+=s;
                }
              }};
              medium.add(name,[enqueue](const AirFrame& f,double r,double s) {
                std::ostringstream out; out<<"START "<<f.id<<' '<<r<<' '<<s<<' '<<air::encode(f)<<'\n'; enqueue(out.str());
              },[enqueue](const AirFrame& f,bool ok){ enqueue("END "+std::to_string(f.id)+" "+(ok?"1\n":"0\n")); });
            } else if(kind=="TX") {
              AirFrame f;
              if(!air::decode(line,f)) {c->dead=true; break;}
              medium.transmit(c->name,std::move(f),air::nowUs());
            } else c->dead=true;
          }
        }
      }
      for(auto i=clients.begin();i!=clients.end();) {
        if((*i)->dead) { if(!(*i)->name.empty()) medium.remove((*i)->name); close((*i)->fd); i=clients.erase(i); }
        else ++i;
      }
      if(polls[0].revents&POLLIN) {
        int fd;
        while((fd=accept4(listener,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC))>=0) {
          if(clients.size()>=128) close(fd);
          else {
            int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
            auto c=std::make_shared<Client>(); c->fd=fd; clients.push_back(c);
          }
        }
      }
    }
    for(auto& c:clients) close(c->fd);
    close(listener);
    return 0;
  } catch(const std::exception& e) { std::cerr<<"emu-air: "<<e.what()<<'\n'; return 1; }
}
