#include "sx1262.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <random>

namespace {
constexpr uint16_t TxDone=1, RxDone=2, Preamble=4, Header=16, CrcError=64,
                   CadDone=128, CadDetected=256, Timeout=512;
uint32_t be(const std::vector<uint8_t>& v,size_t offset,size_t n) {
  uint32_t x=0; for(size_t i=0;i<n;++i) x=(x<<8)|v.at(offset+i); return x;
}
uint32_t bandwidth(uint8_t code) {
  switch(code) {
    case 0: return 7800; case 8: return 10400; case 1: return 15600;
    case 9: return 20800; case 2: return 31250; case 10: return 41700;
    case 3: return 62500; case 4: return 125000; case 5: return 250000;
    case 6: return 500000; default: return 125000;
  }
}
uint64_t symbolUs(const AirFrame& f) { return (uint64_t(1000000)<<f.sf)/f.bandwidth; }
uint64_t airtime(const AirFrame& f) {
  unsigned divisor=4*(f.sf-(f.ldro?2:0));
  int bits=int(8*f.payload.size())+16*f.crc-4*int(f.sf)+(f.sf<=6?0:8)+(f.implicit?0:20);
  unsigned symbols=(std::max(0,bits)+divisor-1)/divisor;
  return symbolUs(f)*((f.preamble+8)*4+(f.sf<=6?25:17)+symbols*f.cr*4)/4;
}
uint8_t rssiByte(double rssi) { return uint8_t(std::clamp(std::lround(-2*rssi),0L,255L)); }
}
struct Sx1262Model::Impl {
  int cs=-1,busy=-1,rst=-1,dio=-1,lastReset=1;
  AirPort* air=nullptr;
  std::array<uint8_t,65536> regs{};
  std::array<uint8_t,256> buffer{};
  std::vector<uint8_t> command;
  uint8_t mode=0x20,commandStatus=4,packetType=1,fallback=0x20;
  uint8_t txBase=0,rxBase=0,rxLength=0,rxOffset=0,payloadLength=0;
  uint16_t irq=0,irqMask=0,dioMask=0;
  AirFrame settings;
  uint64_t txEnd=0,txTimeout=0,rxTimeout=0,cadEnd=0,rxSince=0;
  bool continuous=false,cadBusy=false,stopTimerOnPreamble=false;
  unsigned symbolTimeout=0;
  uint64_t symbolDeadline=0;
  unsigned cadSymbols=4,cadExit=0;
  double lastRssi=-115,lastSnr=0;
  std::mt19937 rng{std::random_device{}()};
  struct Signal { AirFrame frame; uint64_t start; double rssi,snr; };
  std::map<uint64_t,Signal> signals;
  uint64_t receiving=0,headerAt=0;
  struct Event { AirFrame frame; uint64_t at; double rssi=0,snr=0; bool end=false,ok=true; };
  std::mutex mutex;
  std::deque<Event> events;
  void drive() { if(dio>=0) emu::gpioDrive(dio,(irq & dioMask)!=0); }
  void flag(uint16_t bits) { irq |= bits & irqMask; drive(); }
  void stop() { receiving=0; headerAt=txEnd=txTimeout=rxTimeout=cadEnd=symbolDeadline=0; }
  void reset() {
    stop(); regs.fill(0); buffer.fill(0); settings=AirFrame{};
    // MM_EMU_SX1268: the 433 MHz T-Deck chip (its version string); MM_EMU_NO_RADIO: a chip that answers nothing known.
    const char *version=getenv("MM_EMU_NO_RADIO")?"XXXXXX":getenv("MM_EMU_SX1268")?"SX1268 V1.1":"SX1261 V1.1";
    std::copy(version,version+strlen(version)+1,regs.begin()+0x0320);
    regs[0x0740]=0x14; regs[0x0741]=0x24; regs[0x08e7]=0x38;
    regs[0x08ac]=0x94; regs[0x0736]=0x01; regs[0x08d8]=0x1e;
    regs[0x0889]=0x04; regs[0x08b8]=0x80;
    irq=irqMask=dioMask=0; mode=0x20; commandStatus=4; fallback=0x20;
    txBase=rxBase=rxLength=rxOffset=payloadLength=0; packetType=1;
    continuous=false; symbolTimeout=0; stopTimerOnPreamble=false; cadSymbols=4; cadExit=0; command.clear();
    // RF energy belongs to the air, not to chip state: reset must not erase an ongoing signal.
    if(busy>=0) emu::gpioDrive(busy,0);
    drive();
  }
  uint8_t status() const { return mode | commandStatus; }
  AirFrame frame() const {
    AirFrame f=settings; f.syncWord=(uint16_t(regs[0x0740])<<8)|regs[0x0741]; return f;
  }
  bool matches(const AirFrame& f) const {
    auto own=frame();
    return packetType==1 && air::channel(own,f) && own.syncWord==f.syncWord &&
      own.invertIQ==f.invertIQ && own.implicit==f.implicit &&
      (!own.implicit || (payloadLength==f.payload.size() && own.cr==f.cr && own.crc==f.crc));
  }
  void enqueue(const AirFrame& f,double r,double s,bool end,bool ok) {
    std::lock_guard<std::mutex> lock(mutex);
    if(events.size()<4096) events.push_back({f,emu::micros64(),r,s,end,ok});
  }
  void timers(uint64_t now) {
    if(headerAt && now>=headerAt && (!rxTimeout || headerAt<=rxTimeout) &&
       (!symbolDeadline || headerAt<=symbolDeadline)) {
      headerAt=0;
      if(mode==0x50 && receiving) { rxTimeout=0; flag(Header); }
    }
    if(txTimeout && now>=txTimeout && txTimeout<txEnd) {
      txTimeout=txEnd=0; mode=fallback; commandStatus=4; flag(Timeout);
    }
    if(txEnd && now>=txEnd) { txEnd=txTimeout=0; mode=fallback; commandStatus=12; flag(TxDone); }
    if((rxTimeout && now>=rxTimeout) || (symbolDeadline && now>=symbolDeadline)) { stop(); mode=0x20; flag(Timeout); }
    if(cadEnd) {
      for(auto& s:signals) if(air::channel(frame(),s.second.frame)) cadBusy=true;
      if(now>=cadEnd) {
        cadEnd=0; mode=0x20; flag(CadDone | (cadBusy?CadDetected:0));
        if(cadBusy && cadExit==1) { mode=0x50; rxSince=now; continuous=true; }
      }
    }
    // A lost transport END cannot leave a ghost signal or a stuck preamble forever.
    for(auto i=signals.begin();i!=signals.end();) {
      if(now>i->second.start+i->second.frame.durationUs+250000) {
        if(receiving==i->first) { receiving=0; headerAt=0; flag(32); }
        i=signals.erase(i);
      } else ++i;
    }
  }
  void pump() {
    if(air) air->poll();
    std::deque<Event> queue;
    { std::lock_guard<std::mutex> lock(mutex); queue.swap(events); }
    for(auto& e:queue) {
      timers(e.at);
      if(!e.end) {
        signals[e.frame.id]={e.frame,e.at,e.rssi,e.snr};
        if(cadEnd && air::channel(frame(),e.frame)) cadBusy=true;
        if(mode==0x50 && !cadEnd && e.at>=rxSince && matches(e.frame)) {
          // One demodulator: retain the strongest acquisition, including late capture.
          if(!receiving || e.rssi>signals.at(receiving).rssi) {
            receiving=e.frame.id; headerAt=e.at+e.frame.headerUs;
            symbolDeadline=0;
            if(stopTimerOnPreamble) rxTimeout=0;
            flag(Preamble);
          }
        }
      } else {
        auto it=signals.find(e.frame.id);
        if(it!=signals.end() && receiving==e.frame.id && mode==0x50) {
          auto& s=it->second;
          rxLength=uint8_t(s.frame.payload.size()); rxOffset=rxBase;
          for(size_t j=0;j<s.frame.payload.size();++j) buffer[uint8_t(rxBase+j)]=s.frame.payload[j];
          lastRssi=s.rssi; lastSnr=s.snr;
          regs[0x0702]=uint8_t((s.frame.cr-4)<<4);
          regs[0x076b]=s.frame.crc?0x10:0;
          receiving=0; headerAt=0; rxTimeout=0;
          if(!continuous) mode=fallback;
          commandStatus=4; flag(RxDone | (e.ok?0:CrcError));
        }
        signals.erase(e.frame.id);
      }
    }
    timers(emu::micros64());
  }
  double instantRssi() {
    double r=-115+std::uniform_int_distribution<int>(-4,4)(rng)*0.5;
    for(auto& s:signals) if(std::abs(int64_t(s.second.frame.frequency)-settings.frequency)<=
                            int64_t(settings.bandwidth)/2) r=std::max(r,s.second.rssi);
    return r;
  }
  uint8_t reply(size_t pos) {
    uint8_t op=command[0];
    if(op==0xc0) return status();
    size_t prefix=op==0x1d?4:op==0x1e?3:2;
    if(pos<prefix) return status();
    size_t i=pos-prefix;
    switch(op) {
      case 0x1d: {
        uint16_t addr=uint16_t(be(command,1,2)+i);
        return addr>=0x0819 && addr<=0x081c?uint8_t(rng()):regs[addr];
      }
      case 0x1e: return buffer[uint8_t(command[1]+i)];
      case 0x11: return packetType;
      case 0x12: return i==0?uint8_t(irq>>8):uint8_t(irq);
      case 0x13: return i==0?rxLength:rxOffset;
      case 0x14:
        if(i==1) return uint8_t(int8_t(std::clamp(std::lround(lastSnr*4),-128L,127L)));
        return rssiByte(lastRssi);
      case 0x15: return rssiByte(instantRssi());
      case 0x17: return 0;
      case 0x10: return 0; // GetStats (all counters zero)
      default: return status();
    }
  }
  void execute() {
    if(command.empty()) return;
    uint8_t op=command[0]; size_t n=command.size()-1;
    auto need=[&](size_t length){ if(n<length) {commandStatus=8; return false;} return true; };
    if(op==0xc0 || op==0x11 || op==0x12 || op==0x13 || op==0x14 || op==0x15 ||
       op==0x17 || op==0x1d || op==0x1e || op==0x10) return;
    commandStatus=4;
    uint64_t now=emu::micros64();
    switch(op) {
      case 0x00: if(mode==0) mode=0x20; break; // NSS wake/NOP
      case 0x80: if(need(1)){stop(); mode=command[1]?0x30:0x20;} break;
      case 0x84: if(need(1)){ bool cold=!(command[1]&4); if(cold) reset(); else stop(); mode=0; } break;
      case 0xc1: stop(); mode=0x40; break;
      case 0x8a: if(need(1)) packetType=command[1]; break;
      case 0x86: if(need(4)) settings.frequency=uint32_t(std::llround(be(command,1,4)*(32000000.0/33554432.0))); break;
      case 0x8b: if(need(4)) {
        if(command[1]<5 || command[1]>12 || command[3]>7) {commandStatus=8; break;}
        settings.sf=command[1]; settings.bandwidth=bandwidth(command[2]);
        settings.cr=command[3]<5?command[3]+4:(command[3]==7?8:command[3]); settings.ldro=command[4]!=0;
      } break;
      case 0x8c: if(need(6)) {
        settings.preamble=be(command,1,2); settings.implicit=command[3]!=0;
        payloadLength=command[4]; settings.crc=command[5]!=0; settings.invertIQ=command[6]!=0;
      } break;
      case 0x8f: if(need(2)){txBase=command[1]; rxBase=command[2];} break;
      case 0x08: if(need(8)){irqMask=uint16_t(be(command,1,2)); dioMask=uint16_t(be(command,3,2)); drive();} break;
      case 0x02: if(need(2)){irq &= ~be(command,1,2); drive();} break;
      case 0x0d: if(need(2)) for(size_t i=3;i<command.size();++i) regs[uint16_t(be(command,1,2)+i-3)]=command[i]; break;
      case 0x0e: if(need(1)) for(size_t i=2;i<command.size();++i) buffer[uint8_t(command[1]+i-2)]=command[i]; break;
      case 0x82: if(need(3)) {
        stop(); mode=0x50; rxSince=now; uint32_t timeout=be(command,1,3);
        continuous=timeout==0xffffff;
        if(timeout && !continuous) rxTimeout=now+(uint64_t(timeout)*125+7)/8;
        if(symbolTimeout) symbolDeadline=now+symbolTimeout*symbolUs(settings);
      } break;
      case 0x83: if(need(3)) {
        stop(); mode=0x60; AirFrame f=frame();
        for(unsigned i=0;i<payloadLength;++i) f.payload.push_back(buffer[uint8_t(txBase+i)]);
        f.durationUs=airtime(f); f.headerUs=std::min(f.durationUs,symbolUs(f)*((f.preamble+8)*4+(f.sf<=6?25:17))/4);
        txEnd=now+f.durationUs;
        uint32_t timeout=be(command,1,3); if(timeout) txTimeout=now+(uint64_t(timeout)*125+7)/8;
        if(air) air->transmit(f);
      } break;
      case 0x88: if(need(7)){cadSymbols=1U<<std::min<unsigned>(command[1],4); cadExit=command[4];} break;
      case 0xc5:
        stop(); mode=0x50; cadBusy=false;
        cadEnd=now+((uint64_t(1)<<settings.sf)+32)*cadSymbols*1000000/settings.bandwidth;
        for(auto& s:signals) if(air::channel(frame(),s.second.frame)) cadBusy=true;
        break;
      case 0x93: if(need(1)) {
        if(command[1]==0x40) fallback=0x40;
        else if(command[1]==0x30) fallback=0x30;
        else if(command[1]==0x20) fallback=0x20;
        else commandStatus=8;
      } break;
      // Analog calibration, power/ramp, regulator, TCXO/RF-switch and timer controls have no latency.
      case 0x95: need(4); break;
      case 0x8e: need(2); break;
      case 0x89: need(1); break;
      case 0x98: need(2); break;
      case 0x96: need(1); break;
      case 0x97: need(4); break;
      case 0x9d: need(1); break;
      case 0xa0: if(need(1)) symbolTimeout=command[1]; break;
      case 0x9f: if(need(1)) stopTimerOnPreamble=command[1]!=0; break;
      case 0x07: need(2); break;
      case 0xc0: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
      case 0x17: case 0x1d: case 0x1e: case 0x10: break;
      default: commandStatus=8; break;
    }
  }
};
Sx1262Model::Sx1262Model() : impl_(std::make_shared<Impl>()) { impl_->reset(); }
Sx1262Model::~Sx1262Model() {
  if(impl_->air) impl_->air->setReceiver({},{});
  if(impl_->cs>=0) emu::spiAttach(impl_->cs,nullptr);
}
void Sx1262Model::attach(int cs,int busy,int rst,int dio,AirPort* air) {
  if(impl_->cs>=0) throw std::logic_error("Sx1262Model already attached");
  auto p=impl_; p->cs=cs; p->busy=busy; p->rst=rst; p->dio=dio; p->air=air; p->reset();
  std::weak_ptr<Impl> weak=p;
  emu::onGpioWrite(rst,[weak](int level){if(auto s=weak.lock()) {
    if(!s->lastReset && level) s->reset();
    s->lastReset=level;
  }});
  if(air) air->setReceiver([weak](const AirFrame& f,double r,double s){if(auto p=weak.lock()) p->enqueue(f,r,s,false,true);},
                           [weak](const AirFrame& f,bool ok){if(auto p=weak.lock()) p->enqueue(f,0,0,true,ok);});
  emu::addPumpHook([weak]{if(auto s=weak.lock()) s->pump();});
  emu::spiAttach(cs,this);
}
void Sx1262Model::select(bool selected) {
  if(selected) {
    impl_->pump();
    if(impl_->mode==0) impl_->mode=0x20; // NSS wakes the oscillator from sleep.
    impl_->command.clear();
  }
  else {impl_->execute(); impl_->command.clear();}
}
uint8_t Sx1262Model::transfer(uint8_t out) {
  size_t pos=impl_->command.size();
  if(pos>=1024) return impl_->status();
  impl_->command.push_back(out);
  return pos?impl_->reply(pos):impl_->status();
}
