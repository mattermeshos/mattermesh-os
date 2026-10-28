// CLI app (section 30): local command line for this T-Deck (MeshCore-style commands). Included by main.cpp.
#include "services/cli_line.h"
static void cliCompose(mm::State &v);// CLI taps and scrolling need only the CLI part of the state
static std::string rxClock(const matter::RxPacket &p,bool year){
 char text[24];if(p.epoch){time_t t=p.epoch;struct tm local;localtime_r(&t,&local);strftime(text,sizeof(text),year?"%d/%m/%Y %H:%M:%S":"%d/%m %H:%M:%S",&local);}
 else{uint32_t s=p.uptimeMs/1000;snprintf(text,sizeof(text),"up %lu:%02lu:%02lu",(unsigned long)(s/3600),(unsigned long)(s/60%60),(unsigned long)(s%60));}
 return text;
}

// ---- CLI ----------------------------------------------------------------
static std::vector<std::string> cliOutput;static device::CliLine cliLine;static device::CliHistory cliHistory;static String cliMirror;
static int cliScrollLines=0;static bool cliLogging=false;static uint32_t cliLogSeq=0;static std::string cliClipboard;
constexpr int CLI_OUTPUT_MAX=200,CLI_OUTPUT_KEEP=40;
static const std::vector<std::string> &cliCommands(){static const std::vector<std::string> words={"help","ver","board","clock","uptime","get name","get radio","get freq","get bw","get sf","get cr","get tx","get public.key","get role","get lat","get lon","set name","set radio","set tx","advert","neighbors","stats-radio","stats-packets","stats-core","gps","gps on","gps off","log start","log stop","clear","reboot"};return words;}
static std::string cliName(){return matter::snapshot().config.name.c_str();}
static std::string cliPrompt(){return cliName()+"> ";}
static void cliPrint(const std::string &line){cliOutput.push_back(line);if(int(cliOutput.size())>CLI_OUTPUT_MAX)cliOutput.erase(cliOutput.begin(),cliOutput.begin()+(cliOutput.size()-CLI_OUTPUT_MAX));cliScrollLines=0;}
static void cliSync(){cliMirror=cliLine.text.c_str();}
static void cliOpen(){if(cliOutput.empty()){cliPrint("MatterMesh CLI");cliPrint("Connected to "+cliName());cliPrint("Type 'help' for available commands.");cliPrint("");}cliScrollLines=0;cliSync();}
static std::string cliDuration(uint32_t seconds){char text[32];if(seconds>=86400)snprintf(text,sizeof(text),"%lud %luh %02lum",(unsigned long)(seconds/86400),(unsigned long)(seconds/3600%24),(unsigned long)(seconds/60%60));else snprintf(text,sizeof(text),"%luh %02lum %02lus",(unsigned long)(seconds/3600),(unsigned long)(seconds/60%60),(unsigned long)(seconds%60));return text;}
static std::string cliPacketLine(const matter::RxPacket &p){
 char text[96];std::string clock=rxClock(p,false);size_t space=clock.find(' ');if(space!=std::string::npos&&p.epoch)clock=clock.substr(space+1);
 snprintf(text,sizeof(text),"%s %s %s %uB %.1fdB ",clock.c_str(),matter::rxRouteName(p.route()),matter::rxTypeName(p.type()),p.length,p.snr());
 std::string path=matter::rxPathSummary(p,1,1);for(size_t at;(at=path.find(" > "))!=std::string::npos;)path.replace(at,3,">");return text+(path.empty()?std::string("direct"):path);
}
static bool cliApply(matter::Config config,const char *done){String reason;if(matter::configure(config,reason)){cliPrint(done);return true;}cliPrint(std::string("Error: ")+reason.c_str());return false;}
static void cliRun(const std::string &command){
 cliPrint(cliPrompt()+command);cliHistory.add(command);auto w=device::cliWords(command);if(w.empty())return;
 auto config=matter::snapshot().config;char text[96];const std::string &verb=w[0];std::string arg=w.size()>1?w[1]:"";
 auto rest=[&](size_t words){size_t at=0;for(size_t i=0;i<words;i++){at=command.find_first_not_of(' ',at);at=command.find(' ',at);if(at==std::string::npos)return std::string();}at=command.find_first_not_of(' ',at);return at==std::string::npos?std::string():command.substr(at);};
 if(verb=="help"){
  cliPrint("ver, board, clock, uptime");cliPrint("get name|radio|freq|bw|sf|cr|tx|public.key|role|lat|lon");cliPrint("set name <text>, set radio <freq,bw,sf,cr>, set tx <dBm>");
  cliPrint("advert, neighbors, gps [on|off]");cliPrint("stats-radio, stats-packets, stats-core");cliPrint("log start | log stop (live RX packets), clear, reboot");
  cliPrint("Keys: Alt+Q tab, Alt+W/S history, Alt+A/D cursor. Tap a line to copy it.");
 }
 else if(verb=="ver"){cliPrint("MatterMesh OS " MM_RELEASE_LABEL " / MeshCore v" MM_MESHCORE_VERSION);cliPrint("Build " __DATE__ " / " MM_GIT_COMMIT);}
 else if(verb=="board")cliPrint("LilyGO " MM_DEVICE_MODEL " / ESP32-S3 / SX1262");
 else if(verb=="clock"){time_t now=time(nullptr);if(now<MM_EPOCH_MIN)cliPrint("Clock not set / acquiring");else{struct tm local,utc;localtime_r(&now,&local);gmtime_r(&now,&utc);strftime(text,sizeof(text),"%H:%M:%S - %d/%m/%Y",&local);cliPrint(text);strftime(text,sizeof(text),"UTC %H:%M:%S",&utc);auto ts=device::timeSyncState();cliPrint(std::string(text)+" / source "+(ts.source==device::TimeSource::GPS?"GPS":ts.source==device::TimeSource::WiFi?"Wi-Fi":"none"));}}
 else if(verb=="uptime")cliPrint("Up "+cliDuration(millis()/1000));
 else if(verb=="get"){
  if(arg=="name")cliPrint(cliName());
  else if(arg=="radio"){snprintf(text,sizeof(text),"%.3f,%g,%u,%u",config.frequency,config.bandwidth,config.sf,config.cr);cliPrint(text);}
  else if(arg=="freq"){snprintf(text,sizeof(text),"%.3f MHz",config.frequency);cliPrint(text);}
  else if(arg=="bw"){snprintf(text,sizeof(text),"%g kHz",config.bandwidth);cliPrint(text);}
  else if(arg=="sf")cliPrint(std::to_string(config.sf));else if(arg=="cr")cliPrint(std::to_string(config.cr));
  else if(arg=="tx")cliPrint(std::to_string(config.power)+" dBm");
  else if(arg=="public.key"){std::string key=matter::ownKey().c_str();cliPrint(key.empty()?"Identity not ready":key);}
  else if(arg=="role")cliPrint("companion");
  else if(arg=="lat"||arg=="lon"){auto gps=device::locationState();if(!gps.configured)cliPrint("Position unknown");else{snprintf(text,sizeof(text),"%.6f",arg=="lat"?gps.latitude:gps.longitude);cliPrint(text);}}
  else cliPrint("Usage: get name|radio|freq|bw|sf|cr|tx|public.key|role|lat|lon");
 }
 else if(verb=="set"){
  if(arg=="name"){std::string name=rest(2);if(name.empty()||name.size()>31)cliPrint("Usage: set name <1..31 bytes>");else{config.name=name.c_str();cliApply(config,"OK - name saved");}}
  else if(arg=="radio"){std::string value=rest(2);float f,b,sf,cr;char a[16]={},bb[16]={},c[16]={},d[16]={};
   if(sscanf(value.c_str(),"%15[^,],%15[^,],%15[^,],%15s",a,bb,c,d)!=4||!matter::parseRadioField(matter::Frequency,a,f)||!matter::parseRadioField(matter::Bandwidth,bb,b)||!matter::parseRadioField(matter::SpreadFactor,c,sf)||!matter::parseRadioField(matter::CodingRate,d,cr))cliPrint("Usage: set radio <freq,bw,sf,cr> e.g. 869.618,62.5,8,8");
   else{config.frequency=f;config.bandwidth=b;config.sf=uint8_t(sf);config.cr=uint8_t(cr);cliApply(config,"OK - radio restarted");}}
  else if(arg=="tx"){float p;if(w.size()<3||!matter::parseRadioField(matter::TxPower,w[2].c_str(),p)){snprintf(text,sizeof(text),"Usage: set tx <%d..%d dBm>",matter::MIN_TX_POWER,matter::MAX_TX_POWER);cliPrint(text);}else{config.power=int8_t(p);cliApply(config,"OK - TX power saved");}}
  else cliPrint("Usage: set name|radio|tx <value>");
 }
 else if(verb=="advert"){String reason;cliPrint(matter::advertise(reason)?"OK - advert sent":std::string("Error: ")+reason.c_str());}
 else if(verb=="neighbors"){
  auto nodes=matter::nodes(0);int shown=0;uint32_t now=millis();
  for(auto &n:nodes){if(n.hops!=0||!n.seen)continue;uint32_t age=(now-n.seen)/1000;snprintf(text,sizeof(text),"%s %s %s SNR %.1f",n.key.substring(0,8).c_str(),n.name.length()?n.name.c_str():"?",age<60?(std::to_string(age)+"s").c_str():age<3600?(std::to_string(age/60)+"m").c_str():(std::to_string(age/3600)+"h").c_str(),n.snr);cliPrint(text);if(++shown>=20)break;}
  if(!shown)cliPrint("No direct neighbors heard yet");
 }
 else if(verb=="stats-radio"){auto d=matter::radioDiag();snprintf(text,sizeof(text),"Noise floor %d dBm",d.noiseFloor);cliPrint(text);auto l=matter::radioLevels();if(l.received){snprintf(text,sizeof(text),"Last RSSI %.0f dBm / SNR %.1f dB",l.rssi,l.snr);cliPrint(text);}
  snprintf(text,sizeof(text),"TX %d dBm / TCXO %.1f V / boosted RX %s",config.power,d.tcxo,d.boosted?"on":"off");cliPrint(text);snprintf(text,sizeof(text),"Device errors 0x%04X",d.deviceErrors);cliPrint(text);}
 else if(verb=="stats-packets"){auto s=matter::snapshot();snprintf(text,sizeof(text),"RX %lu / TX %lu / RX errors %lu",(unsigned long)s.received,(unsigned long)s.sent,(unsigned long)s.errors);cliPrint(text);auto &log=matter::rxLog();snprintf(text,sizeof(text),"RX log %d stored / %lu since boot / queued %d",log.count(),(unsigned long)log.total(),s.queued);cliPrint(text);}
 else if(verb=="stats-core"){cliPrint("Up "+cliDuration(millis()/1000));auto b=device::batteryState();snprintf(text,sizeof(text),"Battery %d mV / %d%%%s",b.voltage,b.percent,b.charging?" / charging":"");cliPrint(text);snprintf(text,sizeof(text),"Heap %lu KB free / PSRAM %lu KB free",(unsigned long)(ESP.getFreeHeap()/1024),(unsigned long)(ESP.getFreePsram()/1024));cliPrint(text);}
 else if(verb=="gps"){auto gps=device::locationState();
  if(arg=="on"||arg=="off"){bool want=arg=="on";if(gps.enabled!=want&&!device::locationToggleUser())cliPrint("Error: GPS switch failed");else cliPrint(want?"OK - GPS on":"OK - GPS off");}
  else if(!arg.empty())cliPrint("Usage: gps [on|off]");
  else{snprintf(text,sizeof(text),"GPS %s / %s / sats %lu of %lu",gps.enabled?"on":"off",gps.fix?"fix":"no fix",(unsigned long)gps.satellitesUsed,(unsigned long)gps.satellitesVisible);cliPrint(text);if(gps.configured){snprintf(text,sizeof(text),"%.6f, %.6f",gps.latitude,gps.longitude);cliPrint(text);}}}
 else if(verb=="log"){if(arg=="start"){cliLogging=true;cliLogSeq=matter::rxLog().total();cliPrint("Logging RX packets / STOP or 'log stop' to end");}else if(arg=="stop"){cliLogging=false;cliPrint("Log stopped");}else cliPrint("Usage: log start | log stop");}
 else if(verb=="clear"){cliOutput.clear();}
 else if(verb=="reboot"){cliPrint("Rebooting...");render();delay(300);ESP.restart();}
 else cliPrint("Unknown command: "+verb+" (type 'help')");
}
static void cliComplete(){std::vector<std::string> matches;bool changed=device::cliComplete(cliLine,cliCommands(),matches);if(matches.size()>1){std::string list;for(auto &m:matches)list+=(list.empty()?"":"  ")+m;cliPrint(list);}else if(!changed&&matches.empty())notice="";}
// Toolbar: 0 TAB, 1 up, 2 down, 3 left, 4 right, 5 PASTE, 6 CLEAR, 7 STOP.
static void cliTool(int tool){
 switch(tool){
 case 0:cliComplete();break;case 1:cliHistory.older(cliLine);break;case 2:cliHistory.newer(cliLine);break;case 3:cliLine.left();break;case 4:cliLine.right();break;
 case 5:if(!cliClipboard.empty())cliLine.insert(cliClipboard.substr(0,device::CLI_INPUT_MAX-std::min(cliLine.text.size(),device::CLI_INPUT_MAX)));break;
 case 6:cliOutput.clear();cliScrollLines=0;break;
 case 7:if(cliLogging){cliLogging=false;cliPrint("^C Log stopped");}else{cliPrint(cliPrompt()+cliLine.text+"^C");cliLine.clear();cliHistory.reset();}break;
 }
 cliSync();
}
static bool cliKey(char c){
 if(c=='\n'){std::string command=cliLine.text;cliLine.clear();cliSync();if(!cliLogging||!command.empty())cliRun(command);cliSync();render();return true;}
 if(c=='\b'){if(cliLine.text.empty()){back();return true;}cliLine.erase();cliSync();repaintSettled();return true;}
 if(c=='\t'){cliTool(0);render();return true;}
 if(uint8_t(c)>=32&&uint8_t(c)<=126){cliLine.insert(std::string(1,c));cliHistory.reset();cliSync();repaintSettled();return true;}
 return true;
}
// Alt shortcuts from the key event: W/S history, A/D cursor, Q completion (Tab sits left of Q).
static bool cliAltKey(char ch){int tool=ch=='q'?0:ch=='w'?1:ch=='s'?2:ch=='a'?3:ch=='d'?4:-1;if(tool<0)return false;cliTool(tool);render();return true;}
static bool cliTap(int x,int y){
 for(int i=0;i<mm::CliToolCount;i++)if(mm::cliToolRect(i).contains(x,y)){cliTool(i);render();return true;}
 if(mm::CliOutput.contains(x,y)){mm::State v;cliCompose(v);mm::Renderer r(frame);int line=r.cliLineAt(v,y);if(line>=0&&line<int(cliOutput.size())&&!cliOutput[line].empty()){std::string text=cliOutput[line],prompt=cliPrompt();if(text.compare(0,prompt.size(),prompt)==0)text=text.substr(prompt.size());cliClipboard=text;notice="Copied";render();}return true;}
 return false;
}
static void cliTick(){
 if(!cliLogging)return;auto &log=matter::rxLog();if(log.total()==cliLogSeq)return;
 std::vector<const matter::RxPacket*> fresh;for(int i=0;i<log.count();i++){auto p=log.newest(i);if(p->sequence<=cliLogSeq)break;fresh.push_back(p);}
 for(auto it=fresh.rbegin();it!=fresh.rend();++it)cliPrint(cliPacketLine(**it));cliLogSeq=log.total();
 if(screen==Screen::Section&&section==mm::CliSection){markRadioDirty();}
}
static void cliCompose(mm::State &v){v.cliLines=cliOutput;v.cliPrompt=cliPrompt();v.cliInput=cliLine.text;v.cliCursor=cliLine.cursor;v.cliScroll=cliScrollLines;v.cliStreaming=cliLogging;v.cliCanPaste=!cliClipboard.empty();}
// Leaving the CLI (Back, Home, lock, serial): it stops streaming RX packets and keeps only its last lines.
static void cliClose(){if(cliLogging){cliLogging=false;cliPrint("Log stopped / CLI closed");}if(int(cliOutput.size())>CLI_OUTPUT_KEEP)cliOutput.erase(cliOutput.begin(),cliOutput.begin()+(cliOutput.size()-CLI_OUTPUT_KEEP));cliOutput.shrink_to_fit();cliScrollLines=0;}
static void cliScroll(int dy){mm::State v;cliCompose(v);mm::Renderer r(frame);cliScrollLines=std::max(0,std::min(r.cliMaxScroll(v),cliScrollLines+(dy<0?-6:6)));}
