#include "air_client.h"
#include <atomic>
#include <cerrno>
#include <deque>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

void AirPort::setReceiver(Start s, End e) {
  std::lock_guard<std::mutex> lock(callbackMutex_); start_ = std::move(s); end_ = std::move(e);
}
void AirPort::rxStart(const AirFrame& f, double r, double s) {
  std::lock_guard<std::mutex> lock(callbackMutex_); if (start_) start_(f,r,s);
}
void AirPort::rxEnd(const AirFrame& f, bool ok) {
  std::lock_guard<std::mutex> lock(callbackMutex_); if (end_) end_(f,ok);
}
LocalAir::LocalAir(std::string n, std::shared_ptr<Network> net) : name_(std::move(n)), network_(std::move(net)) {
  if (!network_ || !air::validName(name_)) throw std::invalid_argument("invalid LocalAir");
  network_->add(name_,[this](const AirFrame& f,double r,double s){rxStart(f,r,s);},
                       [this](const AirFrame& f,bool ok){rxEnd(f,ok);});
}
LocalAir::~LocalAir() { network_->remove(name_); }
void LocalAir::transmit(const AirFrame& f) { network_->transmit(name_,f,air::nowUs()); }
void LocalAir::poll() { network_->pump(air::nowUs()); }

struct NetAir::Impl {
  NetAir* owner;
  std::string name, host;
  uint16_t port;
  std::atomic<bool> stop{false}, online{false};
  std::mutex mutex;
  std::deque<std::string> outgoing;
  std::thread thread;
  Impl(NetAir* o,std::string n,uint16_t p,std::string h) : owner(o),name(std::move(n)),host(std::move(h)),port(p) {
    in_addr address{};
    if (!air::validName(name) || inet_pton(AF_INET,host.c_str(),&address) != 1)
      throw std::invalid_argument("NetAir requires a valid name and numeric IPv4 address");
    thread = std::thread([this]{run();});
  }
  ~Impl() { stop = true; if (thread.joinable()) thread.join(); }
  void run() {
    while (!stop) {
      int fd = socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
      if (fd < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
      int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
      sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
      inet_pton(AF_INET,host.c_str(),&addr.sin_addr);
      int rc = connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
      pollfd p{fd,POLLOUT,0};
      if (rc < 0 && errno == EINPROGRESS) rc = ::poll(&p,1,50) > 0 ? 0 : -1;
      int error = 0; socklen_t size = sizeof(error);
      if (rc == 0 && (getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size) < 0 || error)) rc = -1;
      if (rc == 0) session(fd);
      online = false; close(fd);
      { std::lock_guard<std::mutex> lock(mutex); outgoing.clear(); }
      for (int i = 0; i < 10 && !stop; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  void session(int fd) {
    std::string output = "HELLO 1 " + name + "\n", input;
    size_t sent = 0;
    std::map<uint64_t,AirFrame> frames;
    while (!stop) {
      if (sent == output.size()) {
        output.clear(); sent = 0;
        std::lock_guard<std::mutex> lock(mutex);
        if (!outgoing.empty()) { output = std::move(outgoing.front()); outgoing.pop_front(); }
      }
      pollfd p{fd,short(POLLIN | (sent < output.size() ? POLLOUT : 0)),0};
      int rc = ::poll(&p,1,10);
      if (rc < 0 && errno == EINTR) continue;
      if (rc < 0 || (p.revents & (POLLERR|POLLHUP|POLLNVAL))) break;
      if (p.revents & POLLOUT) {
        auto n = send(fd,output.data()+sent,output.size()-sent,MSG_NOSIGNAL);
        if (n > 0) sent += size_t(n);
        else if (n == 0 || (errno != EAGAIN && errno != EINTR)) break;
      }
      if (p.revents & POLLIN) {
        char b[4096]; auto n = recv(fd,b,sizeof(b),0);
        if (n <= 0) { if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue; break; }
        input.append(b,size_t(n));
        if (input.size() > 65536) break;
        size_t end;
        while ((end = input.find('\n')) != std::string::npos) {
          std::istringstream line(input.substr(0,end)); input.erase(0,end+1);
          std::string kind; line >> kind;
          if (kind == "READY") online = true;
          else if (kind == "START") {
            AirFrame f; double r,s;
            if ((line >> f.id >> r >> s) && std::isfinite(r) && std::isfinite(s) &&
                air::decode(line,f) && frames.size() < 1024) {
              frames[f.id] = f; owner->rxStart(f,r,s);
            }
          } else if (kind == "END") {
            uint64_t id; unsigned ok;
            if ((line >> id >> ok) && frames.count(id)) {
              owner->rxEnd(frames.at(id),ok == 1); frames.erase(id);
            }
          }
        }
      }
    }
    // Release in-progress receptions on disconnect; never fabricate a successful frame.
    for (auto& f : frames) owner->rxEnd(f.second,false);
  }
};
NetAir::NetAir(std::string n,uint16_t p,std::string h) : impl_(new Impl(this,std::move(n),p,std::move(h))) {}
NetAir::~NetAir() { impl_.reset(); }
bool NetAir::connected() const { return impl_->online; }
void NetAir::transmit(const AirFrame& f) {
  if (!impl_->online) return;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->outgoing.size() < 128) impl_->outgoing.push_back("TX " + air::encode(f) + "\n");
}
