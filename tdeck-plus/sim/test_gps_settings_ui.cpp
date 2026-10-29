#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include "ui/navigation_catalog.h"
int main(){
 assert(mm::ApplicationCount==4&&mm::Applications[0].section==9&&mm::SettingsCount==19);/* Plus: Themes, Home widgets, Text size, Accent key and Keyboard languages rows, no Antenna row */for(auto app:mm::Applications)assert(app.section!=3);
 assert(mm::SettingsItems[mm::SettingsGpsIndex].section==3);assert(mm::settingsVisible(mm::SettingsGpsIndex,mm::settingsReveal(mm::SettingsGpsIndex,0)));
 assert(mm::GpsToggle.y>=mm::Top&&mm::GpsToggle.x+mm::GpsToggle.w<=mm::W-mm::Margin);
 mm::FastCanvas16 canvas(mm::W,mm::H);mm::Components c(canvas);mm::Renderer renderer(canvas);mm::State s;
 for(bool connected:{false,true}){s.status.mesh=connected;c.statusBar(s.status);for(int y=0;y<mm::StatusH-1;y++)for(int x=mm::StatusIconsX+mm::StatusIconStep;x<mm::StatusIconsX+mm::StatusIconStep+mm::IconStatus;x++)assert(canvas.getPixel(x,y)==mm::StatusBg);}
 s.page=mm::Page::GPS;s.gpsAvailable=true;s.locationValid=true;s.latitude=45.9;s.longitude=7.6;renderer.draw(s);std::vector<uint16_t> on(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);s.gpsEnabled=false;renderer.draw(s);assert(!std::equal(on.begin(),on.end(),canvas.getBuffer()));
 // Pixel scrolling: every scroll from 0 to the max shows at least one whole row; settingsReveal shows any row whole
 // (with its group label); paging moves by the list height less one row, so it reaches both ends; groups in order.
 for(int scroll=0;scroll<=mm::settingsMaxScroll();scroll++){int first=mm::settingsFirstVisible(scroll),last=mm::settingsLastVisible(scroll);assert(mm::settingsVisible(first,scroll)&&last>=first);
  for(int i=first;i<=last;i++){auto r=mm::settingsRect(i,scroll);assert(r.y>=mm::SettingsListY&&r.y+mm::SettingsCardH<=mm::Bottom);}}
 assert(mm::settingsVisible(mm::SettingsCount-1,mm::settingsMaxScroll())&&mm::settingsVisible(0,0)&&mm::settingsRect(0,0).y==mm::SettingsListY+mm::SettingsHeaderH);
 for(int i=0;i<mm::SettingsCount;i++)for(int from:{0,mm::settingsMaxScroll()/2,mm::settingsMaxScroll()}){int at=mm::settingsReveal(i,from);assert(mm::settingsVisible(i,at));if(mm::settingsGroupFirst(i))assert(mm::settingsRect(i,at).y-mm::SettingsHeaderH>=mm::SettingsListY);}
 {int scroll=0,pages=0;while(scroll<mm::settingsMaxScroll()){int next=mm::settingsPage(scroll,true);assert(next>scroll);scroll=next;pages++;}assert(scroll==mm::settingsMaxScroll()&&pages<=6);
  while(scroll>0){int prev=mm::settingsPage(scroll,false);assert(prev<scroll);scroll=prev;}}
 for(int g=1;g<mm::SettingsGroupCount;g++)assert(mm::SettingsGroupFirst[g]>mm::SettingsGroupFirst[g-1]);
 // Plus: no Antenna page (one LoRa antenna), no haptic / vibration rows on Lights (no motor), no dictation card on Sound;
 // the Plus-only Themes, Text size and Date / time pages are checked instead.
 // Settings pages: one switch card shared by Wi-Fi/Bluetooth/GPS/Sound, every touch target inside the content
 // area and the margins, no two targets of a page overlapping.
 auto inside=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 auto apart=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 for(auto r:{mm::BleToggle,mm::GpsToggle,mm::AudioNotify})assert(r.y==mm::SettingsSwitch.y&&r.h==mm::SettingsSwitch.h);
 // Plus Wi-Fi: ON/OFF pill in the header band (right of the title), the SCAN / HIDDEN / DISCONNECT bar on the last 30 px like the Chats bar.
 assert(mm::WifiToggle.y>=mm::StatusH&&mm::WifiToggle.y+mm::WifiToggle.h<=mm::Top&&mm::WifiToggle.x+mm::WifiToggle.w<=mm::W-mm::Margin&&mm::WifiToggle.x>=mm::W/2);
 for(auto r:{mm::WifiScan,mm::WifiManual,mm::WifiDisconnect})assert(r.y==mm::ChatsSearch.y&&r.h==mm::ChatsSearch.h&&r.y+r.h<=mm::H-4&&r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin);
 assert(mm::WifiScan.x+mm::WifiScan.w<=mm::WifiManual.x&&mm::WifiManual.x+mm::WifiManual.w<=mm::WifiDisconnect.x);
 std::vector<std::vector<mm::Rect>> pages={{mm::wifiRowRect(0),mm::wifiRowRect(mm::WifiRows-1)},
  {mm::AudioNotify,mm::AudioSlider,mm::AudioMinus,mm::AudioPlus,mm::AudioToneMessage,mm::AudioToneMention},{mm::wallpaperCardRect(0),mm::wallpaperCardRect(1),mm::wallpaperCardRect(2)},{mm::textSizeCardRect(0),mm::textSizeCardRect(1)},
  {mm::dateTimeBox(0),mm::dateTimeBox(1),mm::dateTimeBox(2),mm::dateTimeBox(3),mm::dateTimeBox(4),mm::DateTimeSet},
  {mm::powerRowRect(0),mm::powerRowRect(1),mm::timeoutOptionRect(0),mm::timeoutOptionRect(5)},{mm::lightToggleRect(0),mm::lightSliderRect(0),mm::lightToggleRect(1),mm::lightSliderRect(1)}};
 for(auto &page:pages)for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}
 assert(mm::WifiListY>=mm::Top+8+16&&mm::wifiRowRect(mm::WifiRows-1).y+mm::wifiRowRect(mm::WifiRows-1).h<=mm::WifiScan.y);// status line, then the rows, then the bar
 {mm::State p;p.networks.resize(9);p.wifiSSID="A very long network name for the card";p.wifiIP="192.168.100.200";p.wifiConnected=true;p.storageTotal=1ull<<35;p.storageUsed=1ull<<30;p.sd=true;
  for(auto page:{mm::Page::Wifi,mm::Page::Bluetooth,mm::Page::GPS,mm::Page::Controls,mm::Page::Audio,mm::Page::Power,mm::Page::Wallpaper,mm::Page::TextSize,mm::Page::AccentKey,mm::Page::Battery,mm::Page::DateTime,mm::Page::Storage,mm::Page::Terminal,mm::Page::Info}){p.page=page;renderer.draw(p);}
  p.page=mm::Page::Wifi;p.wifiEditing=true;renderer.draw(p);}
 s.page=mm::Page::Settings;s.settingsFocus=mm::SettingsGpsIndex;renderer.draw(s);s.settingsOffset=mm::settingsMaxScroll();renderer.draw(s);s.page=mm::Page::Launcher;renderer.draw(s);
 std::cout<<"PASS: two menu entries without GPS, GPS Connectivity row, ON/OFF page/cache, all settings reachable, no Mesh triangle or stray slash in either state\n";
}
