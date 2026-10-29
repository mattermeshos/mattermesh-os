#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include "ui/navigation_catalog.h"
#include "mesh/radio_parameters.h"
#include "mesh/radio_presets.h"
#include "services/keyboard_modifiers.h"
// Plus colour frame: ink = pixels that differ from the status bar background of the current theme.
static int ink(mm::FastCanvas16 &canvas,mm::Rect rect){int n=0;for(int y=rect.y;y<rect.y+rect.h;y++)for(int x=rect.x;x<rect.x+rect.w;x++)n+=canvas.getPixel(x,y)!=mm::StatusBg;return n;}
int main(){
 assert(mm::SettingsGpsIndex<mm::SettingsGroupFirst[2]);assert(mm::SettingsItems[mm::SettingsGpsIndex].section==3);assert(std::string(mm::SettingsItems[0].label)=="LoRa");
 assert(matter::RADIO_PRESET_COUNT==26);std::set<int> visited;// 23 official 868 / 900 presets + 3 for 433 MHz units
 for(int band:{868,900,433}){int index=-1,first=-1;for(int i=0;i<26;i++){index=matter::nextRadioPreset(index,band,1);assert(index>=0);if(first<0)first=index;else if(index==first)break;visited.insert(index);assert(matter::radioBand(matter::RADIO_PRESETS[index].frequency)==band);}assert(matter::nextRadioPreset(matter::nextRadioPreset(first,band,-1),band,1)==first);}
 assert(visited.size()==26);
 {int n433=0;for(auto preset:matter::RADIO_PRESETS)n433+=matter::radioBand(preset.frequency)==433;assert(n433==3);}
 assert(matter::radioBand(869.618f)==868&&matter::radioBand(863)==868&&matter::radioBand(902)==900&&matter::radioBand(928)==900&&matter::radioBand(433.65f)==433);
 assert(matter::nextBand(868)==900&&matter::nextBand(900)==433&&matter::nextBand(433)==868);
 assert(matter::bandDefaultPower(433,22)==10&&matter::bandDefaultPower(868,22)==22&&matter::bandDefaultPower(900,22)==22);for(auto preset:matter::RADIO_PRESETS)assert(matter::validRadioParameters(preset.frequency,preset.bandwidth,preset.sf,preset.cr,22));
 float value;
 for(auto text:{"869.618","863","870","902","927.875","928","433.65","433.375","433.05","434.79"})assert(matter::parseRadioField(matter::Frequency,text,value));
 for(auto text:{"433.0","434.8","500","900","870.1","928.1","nan","inf","869.6oops","","..","1e3","9999999999999999999999999999999999999999999"})assert(!matter::parseRadioField(matter::Frequency,text,value));
 for(auto text:{"31.25","62.50","125","500"})assert(matter::parseRadioField(matter::Bandwidth,text,value));assert(!matter::parseRadioField(matter::Bandwidth,"63",value));
 assert(matter::parseRadioField(matter::SpreadFactor,"12",value));assert(!matter::parseRadioField(matter::SpreadFactor,"13",value));assert(!matter::parseRadioField(matter::SpreadFactor,"8.0",value));
 assert(matter::parseRadioField(matter::CodingRate,"5",value));assert(!matter::parseRadioField(matter::CodingRate,"9",value));
 assert(matter::parseRadioField(matter::TxPower,"-9",value));assert(matter::parseRadioField(matter::TxPower,"22",value));assert(!matter::parseRadioField(matter::TxPower,"23",value));
 device::KeyboardModifiers keys;assert(keys.update(30,true)&&keys.alt&&keys.symbols()&&!keys.shift());keys.update(30,false);assert(!keys.symbols());
 keys.update(35,true);keys.update(31,true);assert(keys.shift()&&!keys.symbols());keys.update(35,false);assert(keys.shift());keys.update(31,false);assert(!keys.shift());
 keys.update(32,true);keys.update(30,true);keys.update(32,false);assert(keys.symbols());keys.update(30,false);assert(!keys.symbols());keys.update(35,true);keys.reset();assert(!keys.shift()&&!keys.symbols());
 mm::FastCanvas16 canvas(mm::W,mm::H);mm::Components c(canvas);mm::Renderer r(canvas);mm::State s;
 // All active service icons pack to the right, with no abandoned fixed slots.
 for(int count=0;count<=4;count++){
  s.status=mm::Status{};s.status.unread=count>=1;s.status.gps=count>=2;s.status.wifi=count>=3;s.status.bt=count>=4;s.status.mesh=true;c.statusBar(s.status);
  int first=c.statusLayout(s.status).iconsRight-(count?(count-1)*mm::StatusIconStep+mm::IconStatus:0);
  assert(first>mm::StatusAntennaX+mm::StatusAntennaW);assert(!ink(canvas,{mm::StatusIconsX,mm::StatusIconY,first-mm::StatusIconsX,mm::IconStatus}));
  for(int i=0;i<count;i++)assert(ink(canvas,{first+i*mm::StatusIconStep,mm::StatusIconY,mm::IconStatus,mm::IconStatus})>0);
 }
 for(auto rect:{mm::LoRaBand868,mm::LoRaBand900,mm::LoRaPreset,mm::LoRaApply,mm::LoRaAdvert,mm::LoRaName,mm::LoRaDeviceApply,mm::loraBytesRect(1),mm::loraBytesRect(2),mm::loraBytesRect(3)})assert(rect.x>=mm::Margin&&rect.x+rect.w<=mm::W-mm::Margin&&rect.y>=mm::Top&&rect.y+rect.h<=mm::Bottom);
 for(auto tab:{mm::LoRaRadio,mm::LoRaDevice})assert(tab.y>=mm::StatusH&&tab.y+tab.h==mm::LoRaPanel.y+1&&tab.x>=100&&tab.x+tab.w<=mm::W-mm::Margin);assert(mm::LoRaRadio.x+mm::LoRaRadio.w<mm::LoRaDevice.x);
 // Landscape: the five fields stack below the preset in the left column, APPLY / ADVERTISE on the right, all above the footer status line.
 for(int i=0;i<5;i++){auto row=mm::loraFieldRect(i);assert(row.y>=mm::LoRaPreset.y+mm::LoRaPreset.h&&row.x+row.w<=mm::LoRaApply.x&&row.y+row.h<=mm::FooterY&&row.h>=mm::LoRaFieldStep-1);if(i)assert(mm::loraFieldRect(i-1).y+mm::loraFieldRect(i-1).h<=row.y);}
 assert(mm::LoRaApply.y+mm::LoRaApply.h<=mm::LoRaAdvert.y&&mm::LoRaApply.x==mm::LoRaAdvert.x&&mm::LoRaAdvert.y+mm::LoRaAdvert.h<=mm::FooterY&&mm::LoRaApply.y>=mm::LoRaPreset.y+mm::LoRaPreset.h);
 s.page=mm::Page::Radio;s.radioReady=true;for(int i=0;i<26;i++){auto p=matter::RADIO_PRESETS[i];s.radioPreset=i;s.radioBand=matter::radioBand(p.frequency);s.radioFrequency=p.frequency;s.radioBW=p.bandwidth;s.radioSF=p.sf;s.radioCR=p.cr;r.draw(s);}
 for(int i=0;i<5;i++){s.radioPowerEditing=true;s.radioEditField=i;s.radioPowerInput="22";r.draw(s);}
 s.radioPowerEditing=false;s.radioDevicePage=true;s.radioName=std::string(31,'W');for(int bytes=1;bytes<=3;bytes++){s.radioPathBytes=bytes;s.radioDeviceFocus=bytes;r.draw(s);}
 s.radioPowerEditing=true;s.radioEditField=5;s.radioPowerInput=s.radioName;r.draw(s);assert(c.width(c.inputTail(s.radioName,mm::RadioPowerInput.w-20,mm::Font::ChatBody),mm::Font::ChatBody)<=mm::RadioPowerInput.w-20);
 std::cout<<"PASS: all 23 official 868/900 presets and carousel reachability, strict LoRa ranges/invalid values, physical ALT/dual SHIFT/SYM modifiers, right-aligned active-only icons, GPS Connectivity/LoRa name, all five field layouts/editors\n";
}
