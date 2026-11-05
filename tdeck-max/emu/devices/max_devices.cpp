#include "max_devices.h"
#include "sx1262.h"
#include "air/air_client.h"
#include <Arduino.h>
#include <emu_hal.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <memory>

namespace maxdev {
namespace {
constexpr int W=240, H=320, Row=W/8, Bytes=Row*H;
int frontlight=0, keylight=0;
bool usb=false;
std::unique_ptr<NetAir> air;
Sx1262Model radio;

// UC8253 as used by GxEPD2_310_GDEQ031T10 and TypingPanel. BUSY is active LOW.
struct Panel final : emu::SpiDevice {
 std::array<uint8_t,Bytes> current{}, previous{};
 std::array<uint16_t,W*H> visible{}, pending{};
 uint8_t cmd=0, panelSetting=0x1f, interval=0x97, pll=9, frames=48, repeat=1;
 std::array<uint8_t,8> args{};
 size_t offset=0, position=0;
 int x0=0,x1=W-1,y0=0,y1=H-1;
 bool partial=false, sleeping=false, refreshing=false, dirty=true;
 uint64_t deadline=0;
 Panel(){current.fill(255);previous.fill(255);visible.fill(65535);pending=visible;}
 void reset(){cmd=0;panelSetting=0x1f;interval=0x97;pll=9;partial=false;sleeping=false;deadline=0;refreshing=false;window();emu::gpioDrive(37,HIGH);}
 void window(){x0=0;x1=W-1;y0=0;y1=H-1;}
 void busy(unsigned ms){deadline=emu::micros64()+uint64_t(ms)*1000;emu::gpioDrive(37,LOW);}
 unsigned typingMs() const {
  // TypingPanel's documented PLL 0x13 = 100 Hz; default 0x09 = 50 Hz.
  unsigned hz=pll==0x13?100:50;
  return std::max(1u,unsigned(frames)*std::max(1u,unsigned(repeat))*1000/hz);
 }
 void refresh(){
  if(deadline||sleeping)return;
  bool typing=panelSetting&0x20;
  unsigned ms=typing?typingMs():(interval==0xd7?650:1015);
  pending=visible;
  int left=partial?x0:0,right=partial?x1:W-1,top=partial?y0:0,bottom=partial?y1:H-1;
  for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++)
   pending[y*W+x]=(current[y*Row+x/8]&(0x80>>(x&7)))?65535:0;
  refreshing=true;busy(ms);
  printf("EMU_EPD_BEGIN mode=%s ms=%u area=%d,%d,%d,%d\n",typing?"typing":interval==0xd7?"partial":"full",ms,left,top,right-left+1,bottom-top+1);
 }
 void tick(){
  if(!deadline||emu::micros64()<deadline)return;
  deadline=0;
  if(refreshing){visible=pending;refreshing=false;dirty=true;printf("EMU_EPD_DONE at=%u\n",emu::millis32());}
  emu::gpioDrive(37,HIGH);
 }
 uint8_t transfer(uint8_t data) override {
  if(!emu::gpioLevel(35)){
   if(sleeping)return 0;
   cmd=data;offset=position=0;args.fill(0);
   if(cmd==0x91)partial=true;
   if(cmd==0x92){partial=false;window();}
   if(cmd==0x04||cmd==0x02)busy(45);
   if(cmd==0x12)refresh();
   return 0;
  }
  if(sleeping)return 0;
  if(cmd==0x10||cmd==0x13){
   int rowBytes=(x1/8-x0/8+1);
   int y=y0+int(position/rowBytes),x=x0/8+int(position%rowBytes);
   if(y>=0&&y<=y1&&y<H&&x>=0&&x<Row)(cmd==0x10?previous:current)[y*Row+x]=data;
   ++position;return 0;
  }
  if(offset<args.size())args[offset]=data;
  if(cmd==0x00&&offset==0)panelSetting=data;
  if(cmd==0x50&&offset==0)interval=data;
  if(cmd==0x30&&offset==0)pll=data;
  if(cmd==0x23&&offset==0)repeat=data;
  if(cmd==0x23&&offset==1)frames=data&63;
  if(cmd==0x07&&data==0xa5)sleeping=true;
  if(cmd==0x90&&offset==6){
   x0=std::min(W-1,int(args[0]&0xf8));x1=std::clamp(int(args[1]|7),x0,W-1);
   y0=std::min(H-1,int(args[2])*256+args[3]);y1=std::clamp(int(args[4])*256+args[5],y0,H-1);
  }
  ++offset;return 0;
 }
} panel;

// Ordinary auto-incrementing 8-bit register device. Overrides provide status/FIFO behavior.
struct Registers : emu::I2cDevice {
 std::array<uint8_t,256> r{};uint8_t pointer=0;
 virtual uint8_t get(uint8_t a){return r[a];}
 virtual void put(uint8_t a,uint8_t v){r[a]=v;}
 bool write(const uint8_t *d,size_t n) override {if(!n)return true;pointer=d[0];for(size_t i=1;i<n;i++)put(pointer++,d[i]);return true;}
 size_t read(uint8_t *d,size_t n) override {for(size_t i=0;i<n;i++)d[i]=get(pointer++);return n;}
};
struct Keyboard final : Registers {
 std::deque<uint8_t> fifo;std::array<bool,81> held{};bool enabled=true;
 void irq(){emu::gpioDrive(15,(r[1]&r[2]&0x0f)?LOW:HIGH);}
 void reset(){r.fill(0);fifo.clear();held.fill(false);pointer=0;irq();}
 uint8_t get(uint8_t a) override {
  if(a==3)return uint8_t((r[3]&0xf0)|fifo.size());
  if(a==4){if(fifo.empty())return 0;auto v=fifo.front();fifo.pop_front();return v;}
  return r[a];
 }
 void put(uint8_t a,uint8_t v) override {if(a==2)r[a]&=~v;else r[a]=v;irq();}
 void event(int code,bool down){
  if(!enabled||code<1||code>80||held[code]==down)return;
  // TCA8418 row/column configured as keypad pins by matrix(4,10).
  int row=(code-1)/10,col=(code-1)%10;
  if(!(r[0x1d]&(1<<row))||!(r[0x1e + col/8]&(1<<(col%8))))return;
  held[code]=down;
  if(fifo.size()==10){r[2]|=8;if(r[1]&0x20)fifo.pop_front();else{irq();return;}}
  fifo.push_back(uint8_t(code|(down?128:0)));r[2]|=1;irq();
 }
} keyboard;
struct Touch final : emu::I2cDevice {
 uint16_t pointer=0;bool enabled=true,down=false,sleep=false;int x=0,y=0;
 bool write(const uint8_t *d,size_t n) override {
  if(!enabled)return false;
  if(!n)return true;
  // CST328 is the second HynTouch probe. Reject CST66xx's 4-byte register protocol.
  if(n<2||n>3)return false;
  pointer=uint16_t(d[0]<<8|d[1]);
  if(pointer==0xd105)sleep=true;
  if(pointer==0xd109)sleep=false;
  if(pointer==0xd000&&n==3&&d[2]==0xab)emu::gpioDrive(12,HIGH);
  return true;
 }
 size_t read(uint8_t *d,size_t n) override {
  if(!enabled)return 0;
  std::array<uint8_t,28> info{};info[0]=10;info[2]=16;info[4]=240;info[6]=64;info[7]=1;info[18]=0x28;info[19]=3;info[20]=1;
  uint8_t report[7]={uint8_t(down?6:0),uint8_t(x>>4),uint8_t(y>>4),uint8_t((x&15)<<4|(y&15)),80,uint8_t(down?1:0),0xab};
  for(size_t i=0;i<n;i++){
   unsigned a=pointer+i;
   d[i]=a>=0xd1f4&&a<0xd210?info[a-0xd1f4]:a>=0xd000&&a<0xd007?report[a-0xd000]:0;
  }
  pointer+=n;return n;
 }
 void set(int nx,int ny,bool nd){if(!enabled||sleep)return;x=std::clamp(nx,0,W-1);y=std::clamp(ny,0,H-1);down=nd;emu::gpioDrive(12,HIGH);emu::gpioDrive(12,LOW);}
 void reset(bool on){enabled=on;down=false;sleep=false;emu::gpioDrive(12,HIGH);}
} touchChip;
struct Expander final : Registers {
 Expander(){r[2]=r[3]=r[6]=r[7]=255;}
 bool level(int p)const{int port=p/8,bit=p%8;return (r[6+port]&(1<<bit))||bool(r[2+port]&(1<<bit));}
 uint8_t get(uint8_t a)override{if(a<2){uint8_t v=0;for(int b=0;b<8;b++)if(level(a*8+b))v|=1<<b;return v^r[4+a];}return r[a&7];}
 void put(uint8_t a,uint8_t v)override{
  a&=7;if(a<2)return;
  bool oldTouch=level(7),oldKey=level(9),oldRadio=level(1);r[a]=v;
  if(oldRadio!=level(1))radio.setPowered(level(1));
  if(oldTouch!=level(7))touchChip.reset(level(7));
  if(oldKey!=level(9)){keyboard.enabled=level(9);keyboard.reset();}
 }
} io;
struct Gauge final : Registers {
 void word(int reg,int v){r[reg]=v;r[reg+1]=v>>8;}
 Gauge(){word(0x06,2981);word(0x08,3920);word(0x28,2981);word(0x10,2460);word(0x12,3000);word(0x2c,82);power(false);}
 void power(bool on){word(0x0a,0x28|(on?0:1));word(0x0c,on?450:-120);word(0x14,on?450:-120);word(0x16,on?65535:1230);word(0x18,on?72:65535);}
} gauge;
struct Haptic final : Registers {
 Haptic(){r[0]=0xe0;}
 void put(uint8_t a,uint8_t v)override{r[a]=v;if((a==0x0c&&v&1)||(a==2&&v&&((r[1]&7)==5))){printf("EMU_VIBRATE mode=%u effect=%u rtp=%u powered=%d\n",r[1]&7,r[4],r[2],io.level(5));}if(a==0x0c)r[a]=0;}
} haptic;
struct Charger final : Registers {
 Charger(){r[0x14]=0x38;r[0x0e]=uint8_t((3920-2304)/20);}
 void power(bool on){r[0x0b]=on?0x74:0;r[0x11]=on?0x98:0;}
} charger;
struct Codec final : Registers {Codec(){r[0xfd]=0x83;r[0xfe]=0x11;} } codec;
}
void attach(const char *airName,int airPort){
 for(int pin:{3,34,48})emu::gpioDrive(pin,HIGH);
 if(airPort>0)air=std::make_unique<NetAir>(airName,uint16_t(airPort));
 radio.attach(3,6,4,5,air.get());
 emu::spiAttach(34,&panel);emu::gpioDrive(37,HIGH);emu::gpioDrive(12,HIGH);emu::gpioDrive(15,HIGH);emu::gpioDrive(0,HIGH);
 emu::onGpioWrite(9,[](int level){if(!level)panel.reset();});
 emu::onAnalogWrite(41,[](int v){frontlight=std::clamp(v,0,255);});
 emu::onAnalogWrite(42,[](int v){keylight=std::clamp(v,0,255);});
 emu::i2cAttach(0x20,&io);emu::i2cAttach(0x34,&keyboard);emu::i2cAttach(0x1a,&touchChip);
 emu::i2cAttach(0x55,&gauge);emu::i2cAttach(0x5a,&haptic);emu::i2cAttach(0x6a,&charger);emu::i2cAttach(0x18,&codec);
 emu::addPumpHook([]{panel.tick();});usbPower(false);
}
const uint16_t *frame(){return panel.visible.data();}
int frameWidth(){return W;}int frameHeight(){return H;}
bool frameDirty(bool clear){bool v=panel.dirty;if(clear)panel.dirty=false;return v;}
// E-paper remains readable with power and frontlight off.
bool displayOn(){return true;}
int backlight(){return 12+frontlight*4/255;}
int keyboardLight(){return keylight;}
void key(int code,bool pressed){keyboard.event(code,pressed);}
void touch(int x,int y,bool down){touchChip.set(x,y,down);}
void roll(int,int){}
void press(bool down){emu::gpioDrive(0,down?LOW:HIGH);}
void usbPower(bool on){usb=on;gauge.power(on);charger.power(on);}
bool usbPowered(){return usb;}
bool ioLevel(int pin){return pin>=0&&pin<16&&io.level(pin);}
}
