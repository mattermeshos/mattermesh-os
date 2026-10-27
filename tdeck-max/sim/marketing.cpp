// Website screenshots rendered with the firmware renderer and fictional demo
// content (no real callsigns or messages). Output: PBM frames in argv[1].
#include <fstream>
#include <filesystem>
#include "ui/renderer.h"
int main(int argc,char **argv){
 std::filesystem::path dir=argc>1?argv[1]:"site/screenshots/raw";std::filesystem::create_directories(dir);
 GFXcanvas1 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);
 auto save=[&](const std::string &name){std::ofstream f(dir/(name+".pbm"),std::ios::binary);f<<"P4\n"<<mm::W<<" "<<mm::H<<"\n";f.write((char*)canvas.getBuffer(),((mm::W+7)/8)*mm::H);};
 mm::State s;
 s.sd=s.touch=s.keyboard=s.hapticReady=true;s.radioReady=true;s.status.mesh=true;s.status.clock="09:41";s.status.battery=86;s.status.gps=true;s.status.unread=4;s.unread=4;
 s.date="Sat, Oct 18, 2026";s.menuDate="Sat, Oct 18";s.gpsAvailable=s.gpsEnabled=true;s.nodeCount=42;
 struct Chat {const char *name,*preview,*sender,*time,*age;int unread;};
 const Chat chats[]={{"Trail Team","Rifugio at 12:30, all good?","Marta","09:38","3m",3},{"Public","Repeater on the ridge is back up","Paolo","09:21","20m",1},{"Cervinia","Clear skies over the coast","Elena","08:55","46m",0},{"Weather","Wind 12 km/h, gusts 25","WX-Station","08:30","1h",0}};
 for(int i=0;i<4;i++){mm::Conversation c;c.name=chats[i].name;c.preview=chats[i].preview;c.sender=chats[i].sender;c.time=chats[i].time;c.age=chats[i].age;c.unread=chats[i].unread;c.scope="it-vda";c.id=i;c.activity=10-i;s.conversations.push_back(c);}
 s.focus=-1;
 mm::splashScreen(canvas,80,"Initializing mesh services...");save("01-splash");
 s.receivedChannel="Trail Team";s.receivedName="Marta";s.receivedPreview="Rifugio at 12:30, all good? Bring the spare battery.";s.receivedTime="09:38";s.receivedHops=2;s.lockHasMessage=true;s.lockRepeater="Received via: Ridge-RPT";
 renderer.lock(s);save("02-lock");
 renderer.home(s);save("03-home");
 s.page=mm::Page::Messages;renderer.draw(s);save("04-messages");
 // Conversation: received bubbles with hops/scope, own messages with Heard xN.
 s.page=mm::Page::Chat;s.chatName="Trail Team";s.scope="it-vda";s.active=0;s.chatRadio=true;
 auto rx=[&](const char *who,const char *t,const char *body,int hops){mm::Message m{who,t,body,std::to_string(hops)+" hop | it-vda",false};m.hops=hops;m.scope="it-vda";s.messages.push_back(m);};
 auto tx=[&](const char *t,const char *body,int heard){mm::Message m{"YOU",t,body,"Sent",true};m.repeaters=heard;m.heard=heard>1?"a1,b2":"a1";s.messages.push_back(m);};
 rx("Marta","09:30","Starting from the car park now",2);tx("09:32","On my way, 20 min behind you",2);rx("Luca","09:35","Fog lifting on the ridge",3);rx("Marta","09:38","Rifugio at 12:30, all good?",2);tx("09:40","Perfect, see you there",3);
 s.latestOwnMessage=int(s.messages.size())-1;s.firstRepeater="Received via: Ridge-RPT";
 renderer.draw(s);save("05-chat");
 // Offline dictation in progress.
 s.firstRepeater="";s.audioRecording=true;s.audioSeconds=3;renderer.draw(s);save("06-dictation");s.audioRecording=false;
 // Notification for another chat while reading this one.
 renderer.draw(s);renderer.notificationBanner(mm::NotificationBanner,mm::Top-1,mm::Icon::Messages,"Paolo","Public","Repeater on the ridge is back up");save("07-notification");
 // Quick settings.
 mm::State q=s;q.page=mm::Page::QuickSettings;q.displayLight=96;q.displayLightEnabled=true;q.audioVolume=60;q.audioNotifications=true;q.wifiEnabled=false;q.bleEnabled=false;q.gpsEnabled=true;q.powerSourceKnown=true;
 renderer.draw(q);save("08-quick-settings");
 // Weather and LoRa radio pages.
 mm::State w=s;w.page=mm::Page::Weather;w.weatherValid=true;w.weatherCode=1;w.temperature=17;w.humidity=58;w.pressure=1019;w.wind=12;w.high=21;w.low=11;w.weatherText="Mainly clear";w.weatherPlace="Portofino";w.weatherUpdated="2026-10-18T09:30";
 renderer.draw(w);save("09-weather");
 mm::State r=s;r.page=mm::Page::Radio;r.radioPreset=8;r.radioBand=868;r.radioFrequency=869.618f;r.radioBW=62.5f;r.radioSF=8;r.radioCR=8;r.radioPower=22;r.radioName="Trail-Alpine";r.radioPathBytes=2;r.radioRX=true;r.radioPacketsRX=1284;r.radioPacketsTX=96;r.radioRSSI=-88;r.radioSNR=6;
 renderer.draw(r);save("10-lora");
}
