#include "lr1110.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <random>
namespace {
constexpr uint32_t TxDone=1<<2,RxDone=1<<3,Preamble=1<<4,Header=1<<5,CrcError=1<<7,CadDone=1<<8,CadDetected=1<<9,Timeout=1<<10;
constexpr uint8_t Ok=2<<1,Data=3<<1,Failed=0;
enum Mode:uint8_t{Sleep,Standby,Rx,Tx,Cad};
uint32_t be(const std::vector<uint8_t>& v,size_t offset,size_t n){uint32_t x=0;for(size_t i=0;i<n;++i)x=(x<<8)|v.at(offset+i);return x;}
uint32_t bandwidth(uint8_t code){switch(code){case 3:return 62500;case 4:return 125000;case 5:return 250000;case 6:return 500000;default:return 125000;}}
uint64_t symbolUs(const AirFrame& f){return (uint64_t(1000000)<<f.sf)/f.bandwidth;}
uint64_t airtime(const AirFrame& f){
  unsigned divisor=4*(f.sf-(f.ldro?2:0));
  int bits=int(8*f.payload.size())+16*f.crc-4*int(f.sf)+(f.sf<=6?0:8)+(f.implicit?0:20);
  unsigned symbols=(std::max(0,bits)+divisor-1)/divisor;
  return symbolUs(f)*((f.preamble+8)*4+(f.sf<=6?25:17)+symbols*f.cr*4)/4;
}
uint8_t rssiByte(double rssi){return uint8_t(std::clamp(std::lround(-2*rssi),0L,255L));}
}
struct Lr1110Model::Impl {
  int cs=-1,busy=-1,rst=-1,dio=-1,lastReset=1;AirPort* air=nullptr;
  std::array<uint8_t,256> rxBuffer{},txBuffer{};
  std::vector<uint8_t> command,out,pending;size_t pos=0;bool response=false;
  uint8_t commandStatus=Ok,packetType=2,payloadLength=0,rxLength=0,txLength=0,syncByte=0x12;
  uint32_t irq=0,irqMask=0;Mode mode=Standby;bool continuous=false,cadBusy=false;
  AirFrame settings;uint64_t txEnd=0,cadEnd=0,rxDeadline=0,rxSince=0;unsigned cadSymbols=4;
  double lastRssi=-115,lastSnr=0;std::mt19937 rng{std::random_device{}()};
  struct Signal{AirFrame frame;uint64_t start;double rssi,snr;};std::map<uint64_t,Signal> signals;
  uint64_t receiving=0,headerAt=0;
  struct Event{AirFrame frame;uint64_t at;double rssi=0,snr=0;bool end=false,ok=true;};
  std::mutex mutex;std::deque<Event> events;
  void drive(){if(dio>=0)emu::gpioDrive(dio,(irq&irqMask)!=0);}
  void flag(uint32_t bits){irq|=bits&irqMask;drive();}
  void stop(){receiving=0;headerAt=txEnd=cadEnd=rxDeadline=0;}
  void reset(){stop();irq=irqMask=0;mode=Standby;commandStatus=Ok;packetType=2;settings=AirFrame{};syncByte=0x12;pending.clear();if(busy>=0)emu::gpioDrive(busy,0);drive();}
  AirFrame frame()const{AirFrame f=settings;f.syncWord=(unsigned(syncByte&0xF0)|4)<<8|((syncByte&0x0F)<<4|4);return f;}// on the air as an SX126x register pair
  bool matches(const AirFrame& f)const{auto own=frame();return packetType==2&&air::channel(own,f)&&own.syncWord==f.syncWord&&own.invertIQ==f.invertIQ&&own.implicit==f.implicit;}
  void enqueue(const AirFrame& f,double r,double s,bool end,bool ok){std::lock_guard<std::mutex> lock(mutex);if(events.size()<4096)events.push_back({f,emu::micros64(),r,s,end,ok});}
  void timers(uint64_t now){
    if(headerAt&&now>=headerAt){headerAt=0;if(mode==Rx&&receiving){rxDeadline=0;flag(Header);}}
    if(txEnd&&now>=txEnd){txEnd=0;mode=Standby;flag(TxDone);}
    if(rxDeadline&&now>=rxDeadline){stop();mode=Standby;flag(Timeout);}
    if(cadEnd){for(auto& s:signals)if(air::channel(frame(),s.second.frame))cadBusy=true;
      if(now>=cadEnd){cadEnd=0;mode=Standby;flag(CadDone|(cadBusy?CadDetected:0));}}
    for(auto i=signals.begin();i!=signals.end();){
      if(now>i->second.start+i->second.frame.durationUs+250000){if(receiving==i->first){receiving=0;headerAt=0;}i=signals.erase(i);}else ++i;}
  }
  void pump(){
    if(air)air->poll();
    std::deque<Event> queue;{std::lock_guard<std::mutex> lock(mutex);queue.swap(events);}
    for(auto& e:queue){
      timers(e.at);
      if(!e.end){
        signals[e.frame.id]={e.frame,e.at,e.rssi,e.snr};
        if(cadEnd&&air::channel(frame(),e.frame))cadBusy=true;
        if(mode==Rx&&e.at>=rxSince&&matches(e.frame)&&(!receiving||e.rssi>signals.at(receiving).rssi)){receiving=e.frame.id;headerAt=e.at+e.frame.headerUs;flag(Preamble);}
      }else{
        auto it=signals.find(e.frame.id);
        if(it!=signals.end()&&receiving==e.frame.id&&mode==Rx){
          auto& s=it->second;rxLength=uint8_t(s.frame.payload.size());
          for(size_t j=0;j<s.frame.payload.size();++j)rxBuffer[j]=s.frame.payload[j];
          lastRssi=s.rssi;lastSnr=s.snr;receiving=0;headerAt=0;rxDeadline=0;if(!continuous)mode=Standby;
          flag(RxDone|(e.ok?0:CrcError));
        }
        signals.erase(e.frame.id);
      }
    }
    timers(emu::micros64());
  }
  double instantRssi(){double r=-115+std::uniform_int_distribution<int>(-4,4)(rng)*0.5;
    for(auto& s:signals)if(std::abs(int64_t(s.second.frame.frequency)-settings.frequency)<=int64_t(settings.bandwidth)/2)r=std::max(r,s.second.rssi);return r;}
  uint8_t stat1(uint8_t status)const{return uint8_t(status|((irq&irqMask)?1:0));}
  void answer(std::initializer_list<uint8_t> bytes){pending.assign(bytes);}
  void execute(){
    if(command.size()<2)return;uint16_t op=uint16_t(command[0]<<8|command[1]);if(!op)return;
    size_t n=command.size()-2;uint64_t now=emu::micros64();commandStatus=Ok;
    auto need=[&](size_t length){if(n<length){if(getenv("MM_EMU_LR_TRACE"))fprintf(stderr,"LR1110 short command 0x%04X (%zu bytes)\n",op,n);commandStatus=Failed;return false;}return true;};
    switch(op){
      case 0x0101:answer({0x22,0x01,0x04,0x02});break;// hardware, LR1110, firmware 4.2 (the latest RadioLib knows)
      case 0x0320:answer({0x02,0x01});break;// Wi-Fi scanner firmware
      case 0x0406:answer({0x04,0x01});break;// GNSS firmware, almanac
      case 0x010D:answer({0,0});break;
      case 0x0120:answer({uint8_t(rng()),uint8_t(rng()),uint8_t(rng()),uint8_t(rng())});break;
      case 0x0202:answer({packetType});break;
      case 0x0203:answer({rxLength,0});break;
      case 0x0204:answer({rssiByte(lastRssi),uint8_t(int8_t(std::clamp(std::lround(lastSnr*4),-128L,127L))),rssiByte(lastRssi)});break;
      case 0x0205:answer({rssiByte(instantRssi())});break;
      case 0x0106:if(need(5))pending.assign(size_t(command[6])*4,0);break;
      case 0x010A:if(need(2)){pending.resize(command[3]);for(size_t i=0;i<pending.size();++i)pending[i]=rxBuffer[uint8_t(command[2]+i)];}break;
      case 0x0109:txLength=uint8_t(std::min<size_t>(n,255));for(size_t i=0;i<txLength;++i)txBuffer[i]=command[2+i];break;
      case 0x010B:rxLength=0;break;
      case 0x0113:if(need(4)){irqMask=be(command,2,4);drive();}break;
      case 0x0114:if(need(4)){irq&=~be(command,2,4);drive();}break;
      case 0x011B:stop();mode=Sleep;break;
      case 0x011C:stop();mode=Standby;break;
      case 0x020B:if(need(4))settings.frequency=be(command,2,4);break;
      case 0x020E:if(need(1))packetType=command[2];break;
      case 0x020F:if(need(4)){if(command[2]>12||command[4]>4){commandStatus=Failed;break;}// begin() sends these one at a time, from zeros
        if(command[2]>=5)settings.sf=command[2];settings.bandwidth=bandwidth(command[3]);if(command[4])settings.cr=4+command[4];settings.ldro=command[5]!=0;}break;
      case 0x0210:if(need(6)){settings.preamble=be(command,2,2);settings.implicit=command[4]!=0;payloadLength=command[5];settings.crc=command[6]!=0;settings.invertIQ=command[7]!=0;}break;
      case 0x022B:if(need(1))syncByte=command[2];break;
      case 0x0209:if(need(3)){stop();mode=Rx;rxSince=now;uint32_t t=be(command,2,3);continuous=t==0xFFFFFF;if(t&&!continuous)rxDeadline=now+uint64_t(t)*1000000/32768;}break;
      case 0x020A:if(need(3)){stop();mode=Tx;AirFrame f=frame();f.payload.assign(txBuffer.begin(),txBuffer.begin()+std::min<size_t>(payloadLength,txLength));
        f.durationUs=airtime(f);f.headerUs=std::min(f.durationUs,symbolUs(f)*((f.preamble+8)*4+(f.sf<=6?25:17))/4);txEnd=now+f.durationUs;if(air)air->transmit(f);}break;
      case 0x020D:if(need(1))cadSymbols=std::max<unsigned>(1,command[2]);break;
      case 0x0218:stop();mode=Cad;cadBusy=false;cadEnd=now+((uint64_t(1)<<settings.sf)+32)*cadSymbols*1000000/settings.bandwidth;
        for(auto& s:signals)if(air::channel(frame(),s.second.frame))cadBusy=true;break;
      // Calibration, regulator, TCXO, RF switch, PA and fallback settings have no latency and no effect on the model.
      case 0x0105:case 0x010C:case 0x010E:case 0x010F:case 0x0110:case 0x0111:case 0x0112:case 0x0116:case 0x0117:case 0x012A:
      case 0x0208:case 0x0211:case 0x0212:case 0x0213:case 0x0215:case 0x0217:case 0x021B:case 0x0227:break;
      default:if(getenv("MM_EMU_LR_TRACE"))fprintf(stderr,"LR1110 unknown command 0x%04X (%zu bytes)\n",op,n);commandStatus=Failed;break;
    }
  }
};
Lr1110Model::Lr1110Model():impl_(std::make_shared<Impl>()){impl_->reset();}
void Lr1110Model::attach(int cs,int busy,int rst,int dio,AirPort* air){
  auto p=impl_;p->cs=cs;p->busy=busy;p->rst=rst;p->dio=dio;p->air=air;p->reset();
  std::weak_ptr<Impl> weak=p;
  emu::onGpioWrite(rst,[weak](int level){if(auto s=weak.lock()){if(!s->lastReset&&level)s->reset();s->lastReset=level;}});
  if(air)air->setReceiver([weak](const AirFrame& f,double r,double s){if(auto p=weak.lock())p->enqueue(f,r,s,false,true);},
                          [weak](const AirFrame& f,bool ok){if(auto p=weak.lock())p->enqueue(f,0,0,true,ok);});
  emu::addPumpHook([weak]{if(auto s=weak.lock())s->pump();});
  emu::spiAttach(cs,this);
}
void Lr1110Model::select(bool selected){
  auto& s=*impl_;
  if(!selected){if(!s.response)s.execute();s.command.clear();return;}
  s.pump();if(s.mode==Sleep)s.mode=Standby;s.command.clear();s.pos=0;
  if(!s.pending.empty()){s.response=true;s.out.assign(1,s.stat1(Data));s.out.insert(s.out.end(),s.pending.begin(),s.pending.end());s.pending.clear();return;}
  s.response=false;s.out={s.stat1(s.commandStatus),0,uint8_t(s.irq>>24),uint8_t(s.irq>>16),uint8_t(s.irq>>8),uint8_t(s.irq)};
}
uint8_t Lr1110Model::transfer(uint8_t out){auto& s=*impl_;if(s.command.size()<1024)s.command.push_back(out);return s.pos<s.out.size()?s.out[s.pos++]:0;}
