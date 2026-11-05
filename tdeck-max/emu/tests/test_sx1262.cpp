#include "devices/sx1262.h"
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <array>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if(!(condition)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #condition); } while(false)
namespace {
void pass(const char* s) { std::cout<<"PASS "<<s<<std::endl; }
template<class Predicate> void waitFor(Predicate predicate,uint64_t timeout=3000000) {
  uint64_t start=emu::micros64();
  while(!predicate()) { CHECK(emu::micros64()-start<timeout); delay(1); }
}
struct Board : mesh::MainBoard {
  uint16_t getBattMilliVolts() override {return 4000;}
  const char* getManufacturerName() const override {return "test";}
  void reboot() override {}
  uint8_t getStartupReason() const override {return BD_STARTUP_NORMAL;}
};
struct Node {
  Sx1262Model chip;
  Module module;
  CustomSX1262 radio;
  int cs,busy,rst,dio;
  Node(int base,AirPort* air):module(base,base+3,base+2,base+1,SPI),radio(&module),
      cs(base),busy(base+1),rst(base+2),dio(base+3) {
    digitalWrite(cs,HIGH); chip.attach(cs,busy,rst,dio,air);
    CHECK(radio.begin(869.618,62.5,8,8,RADIOLIB_SX126X_SYNC_WORD_PRIVATE,22,16,1.8,false)==0);
    CHECK(radio.setCRC(1)==0);
    CHECK(radio.setDio2AsRfSwitch(false)==0);
    CHECK(radio.setRxBoostedGainMode(true)==0);
  }
};
const std::vector<uint8_t> payload{0x00,0xff,0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,1,2,3,4,5,6,7,8,9,10,11,12,13,14};
void send(Node& n,const std::vector<uint8_t>& bytes=payload) {CHECK(n.radio.startTransmit(bytes.data(),bytes.size())==0);}
void txDone(Node& n) {waitFor([&]{return n.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TX_DONE;}); CHECK(n.radio.finishTransmit()==0);}
void recv(Node& n,bool ok=true,const std::vector<uint8_t>& bytes=payload) {
  waitFor([&]{return n.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_RX_DONE;});
  CHECK(n.radio.getPacketLength()==bytes.size());
  std::array<uint8_t,256> data{};
  CHECK(n.radio.readData(data.data(),bytes.size())==(ok?0:RADIOLIB_ERR_CRC_MISMATCH));
  CHECK(std::equal(bytes.begin(),bytes.end(),data.begin()));
}
void noPacket(Node& sender,Node& receiver) {
  send(sender); txDone(sender); CHECK(!(receiver.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_RX_DONE));
}
void protocolTests() {
  AirFrame frame; frame.payload=payload; frame.durationUs=100000; frame.headerUs=50000;
  auto encoded=air::encode(frame); std::istringstream input(encoded); AirFrame copy;
  CHECK(air::decode(input,copy)); CHECK(copy.payload==payload);
  for(auto bad : {"1 1 1 1 99 8 1 16 0 1 0 0 00", "1 1 1 1 8 8 1 16 0 1 0 0 zz", "1 1 1 1 8 8 1 16 0 1 0 0 0"}) {
    std::istringstream s(bad); CHECK(!air::decode(s,copy));
  }
  air::Topology t;
  std::istringstream text("# comment\ndefault none\nA B -81 -2.25\nA > C -70 10 25\n"); t.read(text);
  CHECK(t.get("A","B").rssi==-81); CHECK(t.get("B","A").enabled);
  CHECK(t.get("A","C").loss==25); CHECK(!t.get("C","A").enabled);
  bool threw=false; try {std::istringstream bad("A B -50 2 101");t.read(bad);} catch(...) {threw=true;} CHECK(threw);
  pass("protocol round-trip, malformed input, topology symmetry/direction/defaults");
}
void localTests() {
  auto medium=std::make_shared<LocalAir::Network>();
  medium->topology.fallback={-83.5,-2.25,0,true};
  LocalAir airA("A",medium),airB("B",medium),airC("C",medium);
  Node a(60,&airA),b(70,&airB),c(80,&airC);
  Board board; CustomSX1262Wrapper wrapper(b.radio,board);
  wrapper.begin(); wrapper.setParams(869.618,62.5,8,8);
  CHECK(wrapper.getRxBoostedGainMode());
  std::array<uint8_t,256> data{};
  CHECK(wrapper.recvRaw(data.data(),data.size())==0);
  pass("real CustomSX1262 begin at firmware settings, TCXO, boosted gain and wrapper begin");
  uint64_t expected=a.radio.getTimeOnAir(payload.size()),start=emu::micros64();
  send(a); delay(15);
  CHECK(wrapper.isReceivingPacket());
  CHECK(b.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
  CHECK(!(b.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_HEADER_VALID));
  CHECK(emu::gpioLevel(b.dio)==LOW);
  CHECK(wrapper.getCurrentRSSI()==-83.5);
  delay(180); CHECK(wrapper.isReceivingPacket());
  CHECK(b.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_HEADER_VALID);
  CHECK(!(a.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TX_DONE));
  waitFor([&]{return a.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TX_DONE;});
  uint64_t actual=emu::micros64()-start;
  CHECK(actual>=expected && actual<expected+50000);
  CHECK(emu::gpioLevel(b.dio)==HIGH);
  delay(5); CHECK(emu::gpioLevel(b.dio)==HIGH);
  CHECK(wrapper.recvRaw(data.data(),data.size())==int(payload.size()));
  CHECK(std::equal(payload.begin(),payload.end(),data.begin()));
  CHECK(wrapper.getLastRSSI()==-83.5 && wrapper.getLastSNR()==-2.25);
  CHECK(emu::gpioLevel(b.dio)==LOW);
  CHECK((b.radio.getStatus()&0x70)==0x50);
  CHECK(a.radio.finishTransmit()==0);
  std::cout<<"TX timing: expected="<<expected<<"us observed="<<actual<<"us\n";
  pass("LocalAir wrapper recvRaw bytes/RSSI/SNR, timed preamble/header/TxDone, latched DIO1, continuous RX");
  // LocalAir can sustain consecutive continuous receptions without another SetRx.
  CHECK(b.radio.startReceive()==0);
  send(a); recv(b); txDone(a); send(a); recv(b); txDone(a);
  pass("continuous RX receives consecutive frames");
  CHECK(b.radio.scanChannel()==RADIOLIB_CHANNEL_FREE);
  send(a); delay(10);
  CHECK(b.radio.scanChannel()==RADIOLIB_LORA_DETECTED);
  txDone(a);
  CHECK(b.radio.scanChannel()==RADIOLIB_CHANNEL_FREE);
  CHECK(b.radio.startReceive()==0);
  pass("real RadioLib blocking CAD busy and free");
  CHECK(b.radio.setSpreadingFactor(9)==0); CHECK(b.radio.startReceive()==0); noPacket(a,b);
  CHECK(b.radio.setSpreadingFactor(8)==0); CHECK(b.radio.setFrequency(870.0)==0); CHECK(b.radio.startReceive()==0); noPacket(a,b);
  CHECK(b.radio.setFrequency(869.618)==0); CHECK(b.radio.setSyncWord(RADIOLIB_SX126X_SYNC_WORD_PUBLIC)==0);
  CHECK(b.radio.startReceive()==0); noPacket(a,b);
  CHECK(b.radio.setSyncWord(RADIOLIB_SX126X_SYNC_WORD_PRIVATE)==0);
  CHECK(b.radio.standby()==0); send(a); delay(30); CHECK(b.radio.startReceive()==0); txDone(a);
  CHECK(!(b.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_RX_DONE));
  CHECK(b.radio.sleep()==0); noPacket(a,b); CHECK(b.radio.standby()==0);
  CHECK(b.radio.setFrequency(869.6185)==0); CHECK(b.radio.startReceive()==0); send(a); recv(b); txDone(a);
  CHECK(b.radio.setFrequency(869.618)==0);
  pass("SF/frequency/sync mismatch, standby/sleep/late RX rejection, frequency tolerance");
  auto collision=[&](double rA,double rC,bool ok,bool cFirst) {
    medium->topology.links[{"A","B"}]={rA,10,0,true};
    medium->topology.links[{"C","B"}]={rC,10,0,true};
    CHECK(b.radio.startReceive()==0);
    if(cFirst) {send(c);delay(15);send(a);} else {send(a);delay(15);send(c);}
    recv(b,ok); txDone(a); txDone(c);
    CHECK(b.radio.getRSSI()==std::max(rA,rC));
  };
  collision(-80,-80,false,false);
  collision(-80,-75,false,false);
  collision(-80,-74,true,false);
  collision(-74,-80,true,true);
  collision(-74,-80,true,false);
  pass("collisions: equal/5dB CRC errors, exact 6dB capture, stronger first and late capture");
  medium->topology.links[{"A","B"}].loss=100;
  CHECK(b.radio.startReceive()==0); noPacket(a,b);
  pass("100 percent topology loss");
  wrapper.begin(); wrapper.recvRaw(data.data(),data.size());
  wrapper.triggerNoiseFloorCalibrate(10);
  for(int i=0;i<80;++i) wrapper.loop();
  CHECK(wrapper.getNoiseFloor()>=-117 && wrapper.getNoiseFloor()<=-113);
  std::set<long> values; for(int i=0;i<8;++i) values.insert(b.radio.random(0x7fffffff)); CHECK(values.size()>1);
  std::cout<<"Noise floor: "<<wrapper.getNoiseFloor()<<" dBm; RNG distinct="<<values.size()<<"/8\n";
  pass("MeshCore noise-floor calibration and RadioLib RNG variation");
  // Buffer offsets and wrap follow the chip's eight-bit buffer addressing.
  CHECK(b.radio.startReceive()==0);
  CHECK(b.radio.setBufferBaseAddress(0,250)==0);
  CHECK(a.radio.setBufferBaseAddress(245,0)==0);
  medium->topology.links[{"A","B"}].loss=0;
  std::vector<uint8_t> largest(255); for(size_t i=0;i<largest.size();++i) largest[i]=uint8_t(i);
  send(a,largest); recv(b,true,largest); txDone(a);
  pass("255-byte packet and RX buffer offset wrap");
  // Finite hardware timeout, fallback modes, reset/warm/cold sleep, and register read/write.
  CHECK(b.radio.setDioIrqParams(0x3ff,0x3ff)==0); CHECK(b.radio.clearIrqStatus()==0);
  CHECK(b.radio.setRx(640)==0); delay(20);
  CHECK(b.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TIMEOUT);
  CHECK((b.radio.getStatus()&0x70)==0x20);
  CHECK(b.radio.clearIrqStatus()==0); CHECK(emu::gpioLevel(b.dio)==LOW);
  uint8_t written=0xa5,read=0; CHECK(b.radio.writeRegister(0xffff,&written,1)==0);
  CHECK(b.radio.readRegister(0xffff,&read,1)==0 && read==written);
  CHECK(b.radio.sleep(true)==0); CHECK(b.radio.standby()==0);
  CHECK(b.radio.readRegister(0xffff,&read,1)==0 && read==written);
  CHECK(b.radio.sleep(false)==0); CHECK(b.radio.standby()==0);
  CHECK(b.radio.readRegister(0xffff,&read,1)==0 && read==0);
  digitalWrite(b.rst,LOW); digitalWrite(b.rst,HIGH);
  CHECK(b.radio.readRegister(0x08ac,&read,1)==0 && read==0x94);
  CHECK(emu::gpioLevel(b.busy)==LOW && emu::gpioLevel(b.dio)==LOW);
  pass("finite RX timeout, register space, warm/cold sleep and GPIO reset defaults");
  // Wrapper TX lifecycle is tested separately: MeshCore has one static ISR state per process.
  CustomSX1262Wrapper txWrapper(a.radio,board); txWrapper.begin();
  CHECK(txWrapper.startSendRaw(payload.data(),payload.size()));
  waitFor([&]{return txWrapper.isSendComplete();}); txWrapper.onSendFinished();
  CHECK(txWrapper.getPacketsSent()==1);
  pass("real MeshCore wrapper TX completion lifecycle");
}
struct RecordingAir : AirPort {
  AirFrame sent;
  void transmit(const AirFrame& f) override { sent=f; }
};
void airtimeTests() {
  RecordingAir air; Node a(120,&air);
  unsigned count=0;
  for(unsigned sf : {5U,6U,8U,10U,12U}) for(float bw : {62.5f,125.0f,500.0f}) {
    CHECK(a.radio.setSpreadingFactor(sf)==0); CHECK(a.radio.setBandwidth(bw)==0);
    CHECK(a.radio.setCodingRate(sf%2?5:8)==0);
    CHECK(a.radio.setPreambleLength(sf%2?16:32)==0);
    CHECK(a.radio.setCRC(sf%2)==0);
    CHECK(a.radio.explicitHeader()==0);
    send(a);
    CHECK(air.sent.durationUs==a.radio.getTimeOnAir(payload.size()));
    CHECK(air.sent.payload==payload);
    CHECK(air.sent.sf==sf && air.sent.bandwidth==uint32_t(bw*1000));
    CHECK(a.radio.standby()==0); ++count;
  }
  CHECK(a.radio.setSpreadingFactor(8)==0);
  CHECK(a.radio.implicitHeader(payload.size())==0); send(a);
  CHECK(air.sent.implicit && air.sent.durationUs==a.radio.getTimeOnAir(payload.size()));
  CHECK(a.radio.standby()==0);
  // Exercise actual fallback/status values and TX's finite hardware timeout through SPI commands.
  uint8_t fs=0x40;
  CHECK(a.module.SPIwriteStream(0x93,&fs,1)==0);
  send(a);
  waitFor([&]{return a.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TX_DONE;});
  CHECK((a.radio.getStatus()&0x70)==0x40);
  CHECK((a.radio.getStatus()&0x0e)==12);
  CHECK(a.radio.finishTransmit()==0);
  CHECK(a.radio.setDioIrqParams(0x3ff,0x3ff)==0);
  CHECK(a.radio.setPacketParams(16,1,payload.size(),0,0)==0);
  CHECK(a.radio.writeBuffer(payload.data(),payload.size(),245)==0);
  CHECK(a.radio.setBufferBaseAddress(245,0)==0);
  CHECK(a.radio.setTx(640)==0);delay(20);
  CHECK(a.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TIMEOUT);
  CHECK((a.radio.getStatus()&0x70)==0x40);
  CHECK(air.sent.payload==payload);
  CHECK(a.radio.clearIrqStatus()==0);
  uint8_t symbols=3; CHECK(a.module.SPIwriteStream(0xa0,&symbols,1)==0);
  CHECK(a.radio.setRx(0xffffff)==0);delay(10);
  CHECK(a.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_TIMEOUT);
  std::cout<<"Airtime formula: "<<count+1<<" SF/BW/CR/preamble/CRC/header/LDRO configurations match RadioLib exactly\n";
  pass("airtime parameter matrix, implicit header, buffer wrap, TX timeout and FS fallback");
}
void offlineTests() {
  NetAir air("offline",1); Node a(130,&air);
  uint64_t start=emu::micros64(); send(a);
  CHECK(emu::micros64()-start<50000); txDone(a);
  pass("NetAir with unavailable broker never stalls firmware or TX completion");
}
struct ThreadAir : AirPort {
  void transmit(const AirFrame&) override {}
  void start(const AirFrame& f) {rxStart(f,-90,5);}
  void end(const AirFrame& f) {rxEnd(f,true);}
};
void threadTests() {
  ThreadAir air; Node b(140,&air);
  CHECK(b.radio.startReceive()==0);
  AirFrame f; f.id=1; f.durationUs=100000; f.headerUs=20000; f.payload=payload;
  // No pumps or SPI between join and inspecting the GPIO: worker callbacks only enqueue.
  std::thread start([&]{air.start(f);}); start.join();
  CHECK(emu::gpioLevel(b.dio)==LOW); emu::pump(); CHECK(b.radio.isReceiving());
  delay(25); CHECK(b.radio.getIrqFlags()&RADIOLIB_SX126X_IRQ_HEADER_VALID);
  std::thread end([&]{air.end(f);}); end.join();
  CHECK(emu::gpioLevel(b.dio)==LOW); emu::pump(); CHECK(emu::gpioLevel(b.dio)==HIGH);
  recv(b);
  pass("worker callbacks queue events; GPIO and IRQ delivery occur on main-thread pump");
}
struct Broker {
  pid_t pid=-1;
  ~Broker(){ if(pid>0) { kill(pid,SIGTERM); int status; waitpid(pid,&status,0); } }
  void stop() {if(pid>0){kill(pid,SIGTERM);int status;CHECK(waitpid(pid,&status,0)==pid);pid=-1;CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);}}
  void start(const char* executable,unsigned port,const std::string& topology,const std::string& log) {
    CHECK(pid<0); pid=fork(); CHECK(pid>=0);
    if(pid==0) {auto p=std::to_string(port);execl(executable,executable,"--port",p.c_str(),"--topology",topology.c_str(),"--log",log.c_str(),nullptr);_exit(127);}
  }
};
unsigned freePort() {
  int fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd<0) throw std::runtime_error(std::string("localhost TCP socket unavailable: ")+std::strerror(errno));
  sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  CHECK(bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0);
  socklen_t n=sizeof(a);CHECK(getsockname(fd,reinterpret_cast<sockaddr*>(&a),&n)==0);close(fd);return ntohs(a.sin_port);
}
void netTests(const char* executable) {
  char temp[]="/tmp/emu-air-test-XXXXXX";CHECK(mkdtemp(temp));
  std::string root=temp,topology=root+"/topology",log=root+"/air.log";
  {std::ofstream file(topology);file<<"default none\nNA NB -76.5 7.25\n";}
  unsigned port=freePort();
  NetAir airA("NA",uint16_t(port)),airB("NB",uint16_t(port));
  Node a(100,&airA),b(110,&airB);
  // Disconnected transmit must complete with no firmware wait for the broker.
  uint64_t start=emu::micros64(); send(a); CHECK(emu::micros64()-start<50000); txDone(a);
  Broker broker; broker.start(executable,port,topology,log);
  waitFor([&]{return airA.connected()&&airB.connected();});
  Board board;CustomSX1262Wrapper wrapper(b.radio,board);wrapper.begin();
  std::array<uint8_t,256> data{};wrapper.recvRaw(data.data(),data.size());
  send(a);waitFor([&]{return wrapper.isReceivingPacket();});
  int len=0;waitFor([&]{len=wrapper.recvRaw(data.data(),data.size());return len>0;});
  CHECK(len==int(payload.size()) && std::equal(payload.begin(),payload.end(),data.begin()));
  CHECK(wrapper.getLastRSSI()==-76.5 && wrapper.getLastSNR()==7.25); txDone(a);
  pass("localhost broker + two NetAir clients, real driver/wrapper packet and RSSI/SNR");
  broker.stop();waitFor([&]{return !airA.connected()&&!airB.connected();});
  broker.start(executable,port,topology,log);
  waitFor([&]{return airA.connected()&&airB.connected();});
  wrapper.recvRaw(data.data(),data.size());send(a);
  len=0;waitFor([&]{len=wrapper.recvRaw(data.data(),data.size());return len>0;});
  CHECK(len==int(payload.size()));txDone(a);broker.stop();
  std::ifstream file(log);std::string contents((std::istreambuf_iterator<char>(file)),{});
  CHECK(contents.find("sender=NA")!=std::string::npos);
  CHECK(contents.find("NB:-76.5/7.25/ok")!=std::string::npos);
  CHECK(contents.find(air::hex(payload))!=std::string::npos);
  std::filesystem::remove_all(root);
  pass("no-broker TX, automatic reconnect after restart, topology and frame log");
}
}
int main(int argc,char** argv) {
  try {CHECK(argc==2);protocolTests();localTests();airtimeTests();offlineTests();threadTests();
    if(std::getenv("SX1262_TEST_LOCAL_ONLY")) {
      pass("all local SX1262/air tests (ASan + UBSan)");
      std::cout<<"SKIP localhost NetAir tests: SX1262_TEST_LOCAL_ONLY was explicitly set\n";
    } else {netTests(argv[1]);pass("all SX1262/air tests (ASan + UBSan)");}return 0;}
  catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
