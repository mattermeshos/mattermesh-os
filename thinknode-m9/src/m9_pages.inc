// M9 D-pad pages (ui/row_list.h): Wi-Fi, Bluetooth, GPS, Sound, Power, Lights and LoRa as one column of rows, built
// from the same state the T-Deck pages show. Each row says what left / right and OK do; the page keeps one selection.
struct PageRow{mm::ListRow look;std::function<void(int)> change;std::function<void()> ok;std::function<void()> focus;};// focus: run while the row is selected (a Field: which text the keys type into)
static std::vector<PageRow> shownRows;static int rowFocus=-1,rowScroll=0,rowPage=-1,rowRevealed=-2;// rowRevealed: the focus the scroll last followed

// Sections drawn as rows (their text editors - Wi-Fi password, BLE name, a LoRa value - stay the T-Deck forms).
// Chat dialogs drawn as rows: chat options, a message's actions, the clear / position confirmations, scopes, channel
// options and the new-chat menu. Text editors (scope name, search) stay the T-Deck forms.
static bool rowDialog(){
 if(radioLinkAsking())return true;// download the companion's messages?
 if(channelOptions)return !channelKeyView;
 if(screen==Screen::Chats)return messagesMenu;
 if(screen==Screen::Chat)return !emojiPicker&&!messagePath&&(chatDetails||messageActions||clearChatConfirm||positionShare||scopePicker||scopeEditing);
 return false;}
bool rowPageOn(){
 if(deviceLocked)return false;
 if(rowDialog())return true;
 if(screen==Screen::NewChat||screen==Screen::PickContact)return true;
 if(screen!=Screen::Section)return false;
 switch(section){
  case 4:case 14:case 2:case 1:case mm::WebAppSection:case mm::RadioLinkSection:case mm::RadioLinkSettingsSection:return true;
  case 9:return mapMenu;case 10:return weatherTab!=2;// the radar stays a picture
  case 3:case 21:case 20:case 6:case 22:case 31:case mm::HomeWidgetsSection:case 16:case 24:case 5:case 17:case 11:case 8:case 7:case mm::QuickSettingsSection:return true;
  default:return false;}}
static int rowPageId(){
 int dialog=channelOptions?1+deleteChannelConfirm:messagesMenu?3:chatDetails?4:messageActions?5:clearChatConfirm?6:positionShare?7:scopeEditing?9:scopePicker?8:section==4&&wifiEditing?11:section==14&&bleNameEditing?12:section==2&&radioPowerEditing?13:section==1&&contactSearching?14:screen==Screen::NewChat?15:0;
 return (radioLinkAsking()?7919:0)+rlEditing*4099+(int(screen)*64+section)*16+dialog+(section==2&&radioDevicePage?10:0)+(section==10?weatherTab*11:0);}

static std::string clipText(const std::string &b0){std::string b=b0;return b.size()>60?b.substr(0,57)+"...":b;}
// Long lists (contacts) read again at most every 2 s or when what selects them changes: moving the
// selection redraws the page, and reading 40 entries each time cost 20-60 ms on a real M9 (2026-11-04).
template<class T> struct ListCache{std::string key;uint32_t at=0;T value;bool ok=false;
 template<class F> const T &get(const std::string &k,F read){if(!ok||k!=key||uint32_t(millis()-at)>=2000){value=read();key=k;at=millis();ok=true;}return value;}};
static ListCache<matter::ContactPage> contactsCache;
static mm::ListRow look(mm::RowKind kind,const std::string &label,const std::string &detail="",const std::string &value=""){mm::ListRow r;r.kind=kind;r.label=label;r.detail=detail;r.value=value;return r;}
static mm::ListRow withIcon(mm::ListRow r,mm::Icon icon){r.icon=icon;r.hasIcon=true;return r;}
// The buzzer volume as three levels (services/audio.cpp buzz): Off 0, Low 30, High 100.
static const char *volumeName(int v){return v<=0?"Off":v<60?"Low":"High";}
static int volumeStep(int v,int d){int level=v<=0?0:v<60?1:2;level=constrain(level+d,0,2);return level==0?0:level==1?30:100;}
static std::string timeoutLabel(uint32_t s){return s==0?"Never":s<60?std::to_string(s)+" s":std::to_string(s/60)+" min";}
static uint32_t timeoutStep(uint32_t now,int d){int i=0;while(i<6&&device::TIMEOUT_VALUES[i]!=now)i++;if(i>=6)i=1;return device::TIMEOUT_VALUES[constrain(i+d,0,5)];}

static std::vector<PageRow> buildRows(const mm::State &v,std::string &title,std::string &sub){
 using K=mm::RowKind;std::vector<PageRow> rows;
 auto add=[&](mm::ListRow l,std::function<void(int)> change=nullptr,std::function<void()> ok=nullptr){rows.push_back({l,change,ok,nullptr});};
 // OK on a Field: `done` (the form's main action) when given, else the next row.
 auto finish=[&]{if(!notice.isEmpty())add(look(K::Info,notice.c_str()));return rows;};// the page notice, last
 auto field=[&](const std::string &label,const std::string &text,const std::string &hint,std::function<void()> focus,std::function<void()> done=nullptr){auto l=look(K::Field,label,hint,text);rows.push_back({l,nullptr,done,focus});};
 auto toggle=[&](mm::Icon icon,const std::string &label,const std::string &detail,bool on,std::function<void(bool)> set){auto l=withIcon(look(K::Toggle,label,detail),icon);l.on=on;add(l,[=](int d){if((d>0)!=on)set(d>0);},[=]{set(!on);});};
 // Companion link: the download question, then the PIN / name / region form.
 if(radioLinkAsking()){title="Companion connected";sub="";
  add(look(K::Info,"Download the waiting messages?"));
  add(look(K::Action,"Yes, download them"),nullptr,[]{radioLinkAnswer(true);});add(look(K::Action,"No, leave them there"),nullptr,[]{radioLinkAnswer(false);});
  add(look(K::Info,"No: they stay on it, new ones too."));return rows;}
 if(screen==Screen::Section&&(section==mm::RadioLinkSection||section==mm::RadioLinkSettingsSection)&&rlEditing){const char *labels[]={"","Pairing PIN (6 digits)","Companion name","Region (empty: none)"};const char *hints[]={"","default 123456","1 to 31 characters","e.g. it-lig"};
  title="Companion link";sub="edit";field(labels[rlEditing],rlInput.c_str(),hints[rlEditing],[]{},[]{rlFinishEdit();});
  add(look(K::Action,"Save"),nullptr,[]{rlFinishEdit();});add(look(K::Action,"Cancel"),nullptr,[]{rlEditing=0;rlInput="";});
  return finish();}
 // Text forms: a Field row the keys type into (the T-Deck form's own key handler adds the letters), then the actions.
 if(screen==Screen::NewChat){const bool hashtag=newForm==int(mm::ChannelForm::JoinHashtag),create=newForm==int(mm::ChannelForm::CreatePrivate);
  title=hashtag?"Join hashtag channel":create?"Create private channel":"Join private channel";
  field(hashtag?"Hashtag (#name)":"Channel name",newName.c_str(),hashtag?"e.g. meteo":"type a name",[]{formField=0;});
  if(create){add(look(K::Info,"Secret key","",std::string(newKey.c_str()).substr(0,16)+"..."));add(withIcon(look(K::Action,"New random key"),mm::Icon::Lock),nullptr,[]{newKey=randomChannelSecret();notice="New key generated";});}
  else if(hashtag)add(look(K::Info,"Key","","from the #name"));
  else field("Secret key (32 hex)",newKey.c_str(),"0-9 a-f",[]{formField=1;});
  field("Region scope (optional)",newScope.c_str(),"e.g. it-lig",[]{formField=2;},[]{saveNewChannel();});
  add(look(K::Action,create?"Create":"Join"),nullptr,[]{saveNewChannel();});add(look(K::Action,"Cancel"),nullptr,[]{back();});
  return finish();}
 if(screen==Screen::PickContact){title=pickerMention?"Mention":"New direct chat";sub=std::to_string(pickerContacts.size())+" contacts";
  field("Search",pickerQuery.c_str(),"type a name",[]{pickerSearching=true;});
  for(int i=0;i<int(pickerContacts.size())&&i<40;i++){auto &n=pickerContacts[i];add(withIcon(look(K::Nav,mm::senderDisplayName(n.name.c_str()),n.type==2?"Repeater":"Companion"),mm::Icon::Contacts),nullptr,[i]{pickContact(i);});rows.back().focus=[]{pickerSearching=false;};}
  if(pickerContacts.empty())add(look(K::Info,pickerQuery.length()?"No contact matches":"No contacts yet"));
  return rows;}
 if(screen==Screen::Chat&&scopeEditing){title="Add region scope";sub=activeChat>=0?chats[activeChat].name.c_str():"";
  field("Scope name",scopeInput.c_str(),"e.g. it-lig",[]{},[]{saveNewScope();});add(look(K::Action,"Save"),nullptr,[]{saveNewScope();});add(look(K::Action,"Cancel"),nullptr,[]{back();});
  return finish();}
 if(screen==Screen::Section&&section==4&&wifiEditing){title="Wi-Fi";sub=wifiHidden?"hidden network":"connect";
  if(wifiHidden)field("Network name (SSID)",wifiTarget.c_str(),"type the name",[]{wifiEditField=0;});else add(withIcon(look(K::Info,"Network","",wifiTarget.c_str()),mm::Icon::Wifi));
  field("Password",wifiShowPassword?wifiPassword.c_str():v.wifiPasswordMask,"empty if the network is open",[]{wifiEditField=1;},[]{connectWifi();});
  {auto l=withIcon(look(K::Toggle,"Show password"),mm::Icon::Lock);l.on=wifiShowPassword;add(l,[](int d){wifiShowPassword=d>0;},[]{wifiShowPassword=!wifiShowPassword;});}
  add(withIcon(look(K::Action,"Connect"),mm::Icon::Wifi),nullptr,[]{connectWifi();});add(look(K::Action,"Cancel"),nullptr,[]{wifiPassword="";wifiEditing=false;wifiShowPassword=false;});
  return finish();}
 if(screen==Screen::Section&&section==14&&bleNameEditing){title="Bluetooth name";sub="MeshCore-...";
  field("Name after MeshCore-",bleNameInput.c_str(),"empty = the node name",[]{},[]{saveBleName();});add(look(K::Action,"Save"),nullptr,[]{saveBleName();});add(look(K::Action,"Cancel"),nullptr,[]{bleNameEditing=false;});
  return finish();}
 if(screen==Screen::Section&&section==2&&radioPowerEditing){const char *names[]={"Frequency (MHz)","Bandwidth (kHz)","Spreading factor","Coding rate (5-8)","TX power (dBm)","Companion name"};
  const char *hints[]={"863-870 / 902-928","7.8 ... 500","5 to 12","4/5 to 4/8","-9 to 22","1 to 31 bytes"};int f=constrain(radioEditField,0,5);
  title="LoRa";sub="edit value";field(names[f],radioPowerInput.c_str(),hints[f],[]{},[]{saveRadioPower();});
  add(look(K::Action,"Save"),nullptr,[]{saveRadioPower();});add(look(K::Action,"Cancel"),nullptr,[]{radioPowerEditing=false;notice="";});add(look(K::Info,"Then Apply changes on the LoRa page"));
  return finish();}
 if(screen==Screen::Section&&section==1&&contactSearching){title="Contacts";sub="search";
  field("Search contacts",contactQuery.c_str(),"type a name",[]{},[]{contactSearching=false;});add(look(K::Action,"Done"),nullptr,[]{contactSearching=false;});
  auto page=matter::contacts(discoverFilter,contactQuery.c_str(),0,20);for(int i=0;i<int(page.rows.size());i++){add(withIcon(look(K::Nav,mm::senderDisplayName(page.rows[i].name.c_str())),mm::Icon::Contacts),nullptr,[i]{contactSearching=false;contactFocus=i;openContact(i);});}
  return rows;}
 switch(section){
 case mm::RadioLinkSection:{title="Companion link";sub=!v.rlSaved?"not paired":!v.rlOn?"off":v.rlReady?"in use":"connecting";
  if(v.rlSaved){
   toggle(mm::Icon::Antenna,"Use companion radio",v.rlOn?"M9 radio off":"M9 radio on",v.rlOn,[](bool on){rlSetOn(on);});
   add(withIcon(look(K::Info,v.rlSavedName,v.rlStatus),mm::Icon::Bluetooth));
   add(withIcon(look(K::Nav,"Companion settings"),mm::Icon::Settings),nullptr,[]{openSection(mm::RadioLinkSettingsSection);});
   add(look(K::Action,v.rlOn?"Disconnect":"Connect"),nullptr,[]{rlSetOn(!rlOn);});
   add(look(K::Action,"Forget companion (OK twice)"),nullptr,[]{rlForget();});
   if(v.rlReady){add(look(K::Section,"COMPANION"));add(look(K::Info,"Contacts / channels","",std::to_string(v.rlContacts)+" / "+std::to_string(v.rlChannels)));if(!v.rlBattery.empty())add(look(K::Info,"Battery","",v.rlBattery));}
   else add(look(K::Info,v.rlOn?"Companion on, phone app closed.":"Link off: own radio in use."));
   break;}
  add(look(K::Info,"Uses a MeshCore companion's radio."));/* one row each: a sentence split over two cards read as two lines */
  add(withIcon(look(K::Nav,"Pairing PIN","",v.rlPin),mm::Icon::Lock),nullptr,[]{rlStartEdit(1);});
  add(withIcon(look(K::Action,v.rlScanning?"Searching...":"Search companions"),mm::Icon::Search),nullptr,[]{rlScan();});
  for(int i=0;i<int(v.rlPeers.size());i++)add(withIcon(look(K::Nav,v.rlPeers[i].name,"OK pairs it",std::to_string(v.rlPeers[i].rssi)+" dBm"),mm::Icon::Antenna),nullptr,[i]{rlPair(i);});
  if(!v.rlStatus.empty())add(look(K::Info,v.rlStatus));
  break;}
 case mm::RadioLinkSettingsSection:{title="Companion settings";sub=v.rlReady?v.rlName:"not connected";
  add(withIcon(look(K::Nav,"Name","",v.rlName),mm::Icon::Compose),nullptr,[]{rlStartEdit(2);});
  {auto l=look(K::Choice,"Path bytes","",v.rlPathBytes?std::to_string(v.rlPathBytes):"--");add(l,[](int d){int now=rlClient.info.pathModeKnown?rlClient.info.pathMode+1:1;int to=constrain(now+d,1,3);notice=rlClient.setPathBytes(to,millis())?"Path bytes sent":"Companion not ready";});}
  add(withIcon(look(K::Nav,"Region","",v.rlRegion),mm::Icon::Mesh),nullptr,[]{rlStartEdit(3);});
  add(withIcon(look(K::Action,"Send advert"),mm::Icon::Radio),nullptr,[]{notice=rlClient.advertise(millis())?"Advert sent by the companion":"Companion not ready";});
  add(look(K::Section,"COMPANION"));add(look(K::Info,"Version","",v.rlVersion.empty()?"--":v.rlVersion));
  if(!v.rlRadio.empty())add(look(K::Info,v.rlRadio));
  add(look(K::Info,"Contacts / channels","",std::to_string(v.rlContacts)+" / "+std::to_string(v.rlChannels)));
  break;}
 case mm::WebAppSection:{title="MatterMesh Web";sub=v.webOn?(v.webLinked?"browser connected":"on"):"off";
  toggle(mm::Icon::Desktop,"MatterMesh Web",v.webOn?"Serving your chats on Wi-Fi":"Off",v.webOn,[](bool){webToggleUser();});
  if(v.webOn){add(look(K::Section,"OPEN IN A BROWSER"));
   add(look(K::Info,"Address","",v.webAddress.empty()?"No Wi-Fi: join a network":v.webAddress));
   add(look(K::Info,v.webLinked?"Browser":"Code","",v.webLinked?v.webPeer:v.webCode));
   add(look(K::Action,v.webLinked?"Close session":"New code"),nullptr,[]{webNewCode();});}
  add(look(K::Section,"HOW IT WORKS"));
  add(look(K::Info,"Browser on this Wi-Fi + the code."));add(look(K::Info,"One browser at a time."));
  break;}
 case 4:{title="Wi-Fi";sub=v.wifiConnected?"connected":v.wifiConnecting?"connecting...":v.wifiEnabled?"not connected":"off";
  toggle(mm::Icon::Wifi,"Wi-Fi",v.wifiConnected?"Connected to "+v.wifiSSID:v.wifiEnabled?"On, not connected":"Off",v.wifiEnabled,[](bool on){device::wifiEnabled(on);});
  if(!v.wifiEnabled)break;
  add(look(K::Section,"NETWORKS","",v.wifiScanning?"searching...":std::to_string(v.networks.size())+" found"));
  for(int i=0;i<int(v.networks.size())&&i<12;i++){auto &n=v.networks[i];bool current=v.wifiConnected&&n.ssid==v.wifiSSID;
   const char *signal=n.rssi>=-55?"strong":n.rssi>=-67?"good":n.rssi>=-78?"fair":"weak";
   add(withIcon(look(K::Nav,n.ssid.empty()?"(hidden)":n.ssid,std::string(current?"Connected":n.saved?"Saved":n.secured?"Secured":"Open")+" / "+signal),current?mm::Icon::Wifi:n.secured?mm::Icon::Lock:mm::Icon::Wifi),nullptr,[i]{selectWifi(i);});}
  add(withIcon(look(K::Action,v.wifiScanning?"Searching...":v.networks.empty()?"Search networks":"Search again"),mm::Icon::Search),nullptr,[]{device::scan();render();});
  add(withIcon(look(K::Action,"Hidden network"),mm::Icon::Plus),nullptr,[]{hiddenWifi();});
  if(v.wifiConnected||v.wifiConnecting)add(look(K::Action,"Disconnect"),nullptr,[]{device::disconnect();render();});
  else add(look(K::Action,"Reconnect"),nullptr,[]{device::reconnect();render();});
  break;}
 case 14:{title="Bluetooth";sub=v.appBleLinked?"app connected":v.bleEnabled?"visible":"off";
  toggle(mm::Icon::Bluetooth,"Bluetooth",!v.bleEnabled?"Off":!v.bleReady?"Starting...":v.appBleLinked?"MeshCore app connected":"Visible to the MeshCore app",v.bleEnabled,[](bool on){device::bleEnabled(on);});
  add(withIcon(look(K::Nav,"Bluetooth name",v.appBleName),mm::Icon::Compose),nullptr,[]{startBleNameEdit();});
  add(look(K::Info,"Pairing PIN","",v.appPin));
  add(look(K::Section,"MESHCORE APP"));
  toggle(mm::Icon::Wifi,"App over Wi-Fi",!v.appWifi?"Off":v.appWifiLinked?"MeshCore app connected":v.appWifiDetail,v.appWifi,[](bool){appWifiToggle();});
  add(look(K::Info,"USB cable","",v.appUsbLinked?"app connected":"always available"));
  break;}
 case 3:{title="GPS";sub=!v.gpsEnabled?"off":v.gpsFix?"fix":"searching";
  toggle(mm::Icon::GPS,"GPS receiver",!v.gpsAvailable?"Unavailable":v.gpsTemporary?"On for time sync":v.gpsEnabled?"Enabled":"Off, saved position kept",v.gpsEnabled&&!v.gpsTemporary,[](bool){notice=device::locationToggleUser()?(device::locationState().enabled?"GPS enabled":"GPS disabled"):"GPS switch failed";});
  add(look(K::Section,"STATUS"));
  add(look(K::Info,"Fix","",!v.gpsAvailable?"Unavailable":!v.gpsEnabled?"Off":v.gpsFix?"Position fix":"Searching..."));
  add(look(K::Info,"Satellites","",v.gpsEnabled?std::to_string(v.gpsUsed)+" used / "+std::to_string(v.gpsVisible):"--"));
  add(look(K::Info,"Receiver","",!v.gpsEnabled?"Off":v.gpsNmeaFresh?"NMEA "+std::to_string(v.gpsBaud):v.gpsData?"No fresh NMEA":"No data"));
  add(look(K::Section,v.gpsFix?"POSITION":"SAVED POSITION"));
  char pos[40]="-- / --";if(v.locationValid)snprintf(pos,sizeof(pos),"%.5f / %.5f",v.latitude,v.longitude);
  add(look(K::Info,"Lat / lon","",pos));add(look(K::Info,"Altitude","",v.gpsFix?std::to_string(int(v.altitude))+" m":"--"));
  break;}
 case 21:{title="Sound";sub=v.audioReady?"buzzer ready":"no buzzer";
  toggle(mm::Icon::Messages,"Notification sound",v.audioNotifications?"Messages and mentions":"Silent",v.audioNotifications,[](bool on){device::audioNotifications(on);});
  add(withIcon(look(K::Choice,"Volume","the buzzer",volumeName(v.audioVolume)),mm::Icon::Messages),[](int d){device::audioVolume(volumeStep(device::audioState().volume,d));device::audioNotify(true);});
  add(withIcon(look(K::Choice,"New message","",v.messageTone),mm::Icon::Messages),[](int d){cycleRingtone(false,d);},[]{cycleRingtone(false,0);});
  add(withIcon(look(K::Choice,"Mention @you","",v.mentionTone),mm::Icon::Contacts),[](int d){cycleRingtone(true,d);},[]{cycleRingtone(true,0);});
  add(look(K::Action,"Play the sound"),nullptr,[]{device::audioNotify(true);});
  break;}
 case 20:{title="Power / timeouts";
  add(withIcon(look(K::Choice,"Screen light off","after no key",timeoutLabel(v.displayTimeout)),mm::Icon::Power),[](int d){auto p=device::powerState();notice=device::timeout(false,timeoutStep(p.displaySeconds,d))?"":"Save failed";});
  add(withIcon(look(K::Choice,"Keyboard light off","after no key",timeoutLabel(v.keyboardTimeout)),mm::Icon::Keyboard),[](int d){auto p=device::powerState();notice=device::timeout(true,timeoutStep(p.keyboardSeconds,d))?"":"Save failed";});
  add(look(K::Info,"Lock","",v.displayTimeout?"with the screen":"never"));
  break;}
 case 6:{title="Lights";
  auto light=[&](bool keyboard){const char *name=keyboard?"Keyboard light":"Screen light";bool enabled=keyboard?v.keyboardLightEnabled:v.displayLightEnabled;
   toggle(keyboard?mm::Icon::Keyboard:mm::Icon::Flashlight,name,enabled?"Wakes on a key":"Always off",enabled,[keyboard](bool on){if(!keyboard){notice="The screen light stays on";return;}notice=device::lightEnabled(keyboard,on)?"":"Cannot save light preference";});
   auto l=withIcon(look(K::Level,"Brightness"),mm::Icon::Flashlight);l.percent=((keyboard?v.keyboardLight:v.displayLight)*100+127)/255;
   add(l,[keyboard](int d){uint8_t &b=keyboard?keyboardBrightness:displayBrightness;b=uint8_t(constrain(int(b)+d*26,keyboard?0:13,255));saveLights();device::brightness(displayBrightness,keyboardBrightness);});};
  add(look(K::Section,"SCREEN"));light(false);add(look(K::Section,"KEYBOARD"));light(true);
  break;}
 case 2:{title="LoRa";sub=v.radioReady?(v.radioTX?"TX active":"RX ready"):"radio error "+std::to_string(v.radioError);if(loraLeaveWarning)sub="BACK again: discard";
  add(look(K::Choice,"View","",radioDevicePage?"Device":"Radio"),[](int){selectLoRaTab(!radioDevicePage);});
  if(radioDevicePage){
   add(withIcon(look(K::Nav,"Companion name","",v.radioName),mm::Icon::Compose),nullptr,[]{radioDeviceFocus=0;editRadioField(CompanionNameField);});
   add(withIcon(look(K::Choice,"Path hash","bytes per hop in paths",std::to_string(v.radioPathBytes)+(v.radioPathBytes==1?" byte":" bytes")),mm::Icon::Radio),[](int d){radioDraft.pathBytes=constrain(int(radioDraft.pathBytes)+d,1,3);notice="Changes ready: Apply";});
  }else{
   add(withIcon(look(K::Choice,"Band","",std::to_string(v.radioBand)+" MHz"),mm::Icon::Radio),[b=v.radioBand](int d){const int bands[]={433,868,915};int i=b==433?0:b==915?2:1;selectRadioBand(bands[(i+d+3)%3]);});
   add(withIcon(look(K::Choice,"Preset","",v.radioPreset>=0&&v.radioPreset<matter::RADIO_PRESET_COUNT?matter::RADIO_PRESETS[v.radioPreset].name:"Custom"),mm::Icon::Signal),[](int d){cycleRadioPreset(d);});
   add(look(K::Section,"VALUES"));
   char f[24],bw[24];snprintf(f,sizeof(f),"%.3f MHz",v.radioFrequency);snprintf(bw,sizeof(bw),"%.2f kHz",v.radioBW);
   const std::string values[]={f,bw,"SF"+std::to_string(v.radioSF),"4/"+std::to_string(v.radioCR),std::to_string(v.radioPower)+" dBm"};
   const char *labels[]={"Frequency","Bandwidth","Spreading factor","Coding rate","TX power"};
   for(int i=0;i<matter::RadioFieldCount&&i<5;i++)add(look(K::Nav,labels[i],"",values[i]),nullptr,[i]{radioFocus=i+1;editRadioField(i);});
   add(withIcon(look(K::Action,"Advertise now"),mm::Icon::Signal),nullptr,[]{matter::advertise(notice);});
  }
  add(look(K::Action,v.radioDraftChanged?"Apply changes *":"Apply changes"),nullptr,[]{applyRadioDraft();});
  break;}
 case 22:{title="Themes";sub=mm::theme(v.wallpaper).name;
  for(int i=0;i<WALLPAPER_COUNT&&i<mm::ThemeCount;i++)add(withIcon(look(K::Nav,mm::theme(i).name,mm::theme(i).detail,i==v.wallpaper?"In use":""),mm::Icon::Mountain),nullptr,[i]{wallpaperFocus=i;notice=setWallpaper(i)?"Theme applied":"Save failed";});
  break;}
 case 31:{title="Text size";
  add(withIcon(look(K::Choice,"Chat text","bubbles, composer, chat dialogs",chatTextSize?"Large":"Compact"),mm::Icon::Compose),[](int d){bool ok=setTextSize(d>0?1:0);notice=ok?"":"Save failed";textSizeFocus=chatTextSize;});
  add(look(K::Info,"Lists and settings","","keep their size"));
  break;}
 case mm::HomeWidgetsSection:{title="Home widgets";
  add(withIcon(look(K::Choice,"Beside the clock",mm::HomeWidgets[v.homeWidget].description,mm::HomeWidgets[v.homeWidget].name),mm::Icon::Clock),[](int d){setHomeWidget((homeWidget+d+mm::HomeWidgetCount)%mm::HomeWidgetCount);homeWidgetFocus=homeWidget;});
  if(*mm::HomeWidgets[v.homeWidget].needs)add(look(K::Info,mm::HomeWidgets[v.homeWidget].needs));
  add(withIcon(look(K::Choice,"Main area",mm::MainWidgets[v.homeMain].description,mm::MainWidgets[v.homeMain].name),mm::Icon::Messages),[](int d){setHomeMain((homeMain+d+mm::MainWidgetCount)%mm::MainWidgetCount);homeMainFocus=homeMain;});
  if(*mm::MainWidgets[v.homeMain].needs)add(look(K::Info,mm::MainWidgets[v.homeMain].needs));
  break;}
 case 16:{title="Date / time";sub=v.timeSource;
  add(look(K::Info,"Now","",v.status.clock+"  "+v.date));
  add(withIcon(look(K::Choice,"Time zone","",v.timeZone),mm::Icon::Clock),[](int d){device::setTimeZone(device::timeZone()+d);dtLoad();lockScreenDirty=true;});
  add(look(K::Section,"SET BY HAND"));
  const char *names[]={"Day","Month","Year","Hour","Minute"};
  for(int i=0;i<5;i++){char val[8];snprintf(val,sizeof(val),i==2?"%04d":"%02d",v.dtField[i]);add(look(K::Choice,names[i],"",val),[i](int d){dtFocus=i;dtStep(i,d);});}
  add(look(K::Action,"Set date and time"),nullptr,[]{String reason;notice=dtApply(reason)?"Date and time set":reason;});
  break;}
 case 24:{title="Storage";
  auto gb=[](uint64_t b){char x[16];if(b<1073741824ull)snprintf(x,sizeof(x),"%u MB",unsigned(b/1048576));else snprintf(x,sizeof(x),"%.1f GB",b/1073741824.0);return std::string(x);};
  add(look(K::Info,"SD card","",v.sd?"Mounted":"Not found"));
  if(v.sd){add(look(K::Info,"Used","",gb(v.storageUsed)));add(look(K::Info,"Free","",gb(v.storageTotal-v.storageUsed)));add(look(K::Info,"Total","",gb(v.storageTotal)));}
  break;}
 case 5:{title="Battery";
  add(look(K::Info,"Charge","",v.status.battery<0?"--":std::to_string(v.status.battery)+"%"));
  add(look(K::Info,"Voltage","",v.voltage<0?"--":std::to_string(v.voltage)+" mV"));
  const bool usb=device::batteryPowerSource()==device::PowerSource::USB;
  add(look(K::Info,"State","",v.batteryCharging||v.status.charging?"Charging (USB)":usb?(v.batteryFull||v.status.battery>=100?"Full, on USB":"On USB power"):"On battery"));
  add(look(K::Info,"Source","",v.batteryEstimated?"Voltage estimate":"Fuel gauge"));
  break;}
 case 17:{title="OTA update";sub=v.otaStatus;
  add(look(K::Info,"Installed","",v.version));add(look(K::Info,"Server","",v.otaStatus));
  if(v.otaProgress>=0){auto l=look(K::Level,"Download");l.percent=v.otaProgress;rows.push_back({l,nullptr,nullptr});rows.back().look.kind=K::Info;rows.back().look.value=std::to_string(v.otaProgress)+"%";}
  if(otaConfirm){add(look(K::Action,"Install now"),nullptr,[]{otaAction();});add(look(K::Action,"Not now"),nullptr,[]{otaConfirm=false;});}
  else if(!v.otaAction.empty())add(look(K::Action,v.otaAction),nullptr,[]{otaAction();});
  if(!v.otaNotes.empty()){add(look(K::Section,v.otaNotesTitle.empty()?"WHAT'S NEW":v.otaNotesTitle));for(auto &n:v.otaNotes)add(look(K::Info,n));}
  break;}
 case 11:{title="Diagnostics";
  add(look(K::Info,"Free memory","",std::to_string(v.heapKB)+" KB"));add(look(K::Info,"Loop stack free","",std::to_string(v.stackFree)+" B"));
  add(look(K::Info,"CPU","",std::to_string(v.cpuMHz)+" MHz"));add(look(K::Info,"Mesh RX / TX","",std::to_string(v.radioPacketsRX)+" / "+std::to_string(v.radioPacketsTX)));
  add(look(K::Info,"UI / LCD time","",std::to_string(v.rasterMs)+" / "+std::to_string(v.paintMs)+" ms"));add(look(K::Info,"USB serial","","ui stats / status"));
  break;}
 case 8:{title="About";sub="by Kinta";
  add(look(K::Info,"MatterMesh OS","",v.version));add(look(K::Info,"MeshCore","",v.meshCoreVersion));add(look(K::Info,"Device","",mm::DeviceLabel));
  add(look(K::Info,"Radio","",v.radioReady?"LR1110 / MeshCore":"LR1110 / offline"));add(look(K::Info,"Build","",v.build));add(look(K::Info,"Commit","",v.commit));
  add(look(K::Info,"Memory","",std::to_string(v.flashMB)+" MB / "+std::to_string(v.heapKB)+" KB free"));add(look(K::Info,"Keyboard","",v.keyboard?"OK":"missing"));
  break;}
 case 7:{title="Files";sub=v.sd?"SD card, read only":"no SD card";
  add(look(K::Section,"SD CARD / ROOT"));
  for(auto &f:v.files){bool dir=f.rfind("[DIR] ",0)==0;add(withIcon(look(K::Info,dir?f.substr(6):f,"",dir?"folder":""),dir?mm::Icon::Files:mm::Icon::Notes));}
  if(v.files.empty())add(look(K::Info,"No files available"));
  break;}
 case 1:{title="Contacts";
  const char *filters[]={"All","Companions","Repeaters"};
  add(withIcon(look(K::Choice,"Show","",filters[discoverFilter%3]),mm::Icon::Contacts),[](int d){discoverFilter=(discoverFilter+d+3)%3;contactFocus=nodeScroll=0;});
  add(withIcon(look(K::Action,"Search contacts"),mm::Icon::Search),nullptr,[]{contactSearching=true;contactQuery="";});
  if(contactQuery.length())add(look(K::Action,("Clear search \""+std::string(contactQuery.c_str())+"\"")),nullptr,[]{contactQuery="";});
  auto &page=contactsCache.get(std::to_string(discoverFilter)+"|"+contactQuery.c_str(),[]{return matter::contacts(discoverFilter,contactQuery.c_str(),0,40);});sub=std::to_string(page.total)+" / "+std::to_string(page.live)+" live";
  add(look(K::Section,"CONTACTS","",std::to_string(page.total)));
  for(int i=0;i<int(page.rows.size());i++){auto &n=page.rows[i];std::string kind=n.type==2?"Repeater":n.type==1?"Companion":"Node";
   // seen is millis() of the last advert heard since boot (live); a saved contact not heard yet has no age
   uint32_t s=n.live?(millis()-n.seen)/1000:0;std::string age=!n.live?"saved":s<60?"now":s<3600?std::to_string(s/60)+" min":s<86400?std::to_string(s/3600)+" h":std::to_string(s/86400)+" d";
   add(withIcon(look(K::Nav,mm::senderDisplayName(n.name.c_str()),kind+(n.hops>=0?" / "+std::to_string(n.hops)+" hops":""),age),n.type==2?mm::Icon::Signal:mm::Icon::Contacts),nullptr,[i]{contactFocus=i;openContact(i);});}
  if(page.rows.empty())add(look(K::Info,contactQuery.length()?"No contact matches":"No contacts yet - waiting for adverts"));
  break;}
 case 10:{title="Weather";sub=v.weatherPlace;
  const char *tabs[]={"Now","Storms","Radar"};
  add(withIcon(look(K::Choice,"View","",tabs[weatherTab]),mm::Icon::Weather),[](int d){weatherSelectTab((weatherTab+d+3)%3);});
  auto phase=device::weatherState().phase;bool wifiOff=phase==device::WeatherPhase::WifiOff,noNet=phase==device::WeatherPhase::NoNetwork;
  if(weatherTab==0){
   if(v.weatherValid){char now[40];snprintf(now,sizeof(now),"%.0f\xC2\xB0",v.temperature);add(look(K::Info,v.weatherText,"",now));
    char hl[32];snprintf(hl,sizeof(hl),"%.0f\xC2\xB0 / %.0f\xC2\xB0",v.low,v.high);add(look(K::Info,"Today min / max","",hl));
    char hum[16];snprintf(hum,sizeof(hum),"%.0f%%",v.humidity);add(look(K::Info,"Humidity","",hum));
    char wind[24];snprintf(wind,sizeof(wind),"%.0f km/h",v.wind);add(look(K::Info,"Wind","",wind));
    char pr[24];snprintf(pr,sizeof(pr),"%.0f hPa",v.pressure);add(look(K::Info,"Pressure","",pr));
    if(!v.weatherUpdated.empty())add(look(K::Info,"Updated","",v.weatherUpdated));}
   else add(look(K::Info,v.weatherMessage.empty()?"No data yet":v.weatherMessage));
   if(wifiOff)add(withIcon(look(K::Action,"Turn on Wi-Fi"),mm::Icon::Wifi),nullptr,[]{device::weatherEnableWifi();});
   else if(noNet)add(withIcon(look(K::Action,"Wi-Fi settings"),mm::Icon::Wifi),nullptr,[]{openSection(4);});
   else add(look(K::Action,v.weatherBusy?"Updating...":"Refresh"),nullptr,[]{device::weatherRefresh();});
  }else{
   if(!v.stormHead.empty())add(look(K::Info,v.stormHead,v.stormSub));
   for(auto &r:v.stormRows)add(withIcon(look(K::Info,r.title,r.detail,r.when),r.thunder?mm::Icon::Weather:mm::Icon::Info));
   toggle(mm::Icon::Info,"Storm alerts","notice when one is near",v.stormAlerts,[](bool){stormToggleAlerts();});
   add(look(K::Action,v.stormBusy?"Checking...":"Refresh"),nullptr,[]{device::stormRefresh();});
  }
  break;}
 case 9:{title="Map";sub="BACK: the map";
  add(withIcon(look(K::Choice,"Zoom","",std::to_string(mapView.zoom)),mm::Icon::Search),[](int d){mapZoom(d);});
  add(withIcon(look(K::Action,"Centre on my position"),mm::Icon::GPS),nullptr,[]{mapCenter();mapMenu=false;});
  add(look(K::Action,"Back to the map"),nullptr,[]{mapMenu=false;});
  break;}
 case mm::QuickSettingsSection:{title="Quick settings";
  {auto l=withIcon(look(K::Level,"Screen brightness"),mm::Icon::Flashlight);l.percent=(displayBrightness*100+127)/255;add(l,[](int d){displayBrightness=uint8_t(constrain(int(displayBrightness)+d*16,16,255));saveLights();device::brightness(displayBrightness,keyboardBrightness);});}
  add(withIcon(look(K::Choice,"Volume","",volumeName(device::audioState().volume)),mm::Icon::Messages),[](int d){device::audioVolume(volumeStep(device::audioState().volume,d));});
  auto c=device::connectivityStatus();
  toggle(mm::Icon::Wifi,"Wi-Fi",c.connected?"Connected":"",c.wifi,[](bool on){device::wifiEnabled(on);});
  toggle(mm::Icon::Bluetooth,"Bluetooth","",c.ble,[](bool on){device::bleEnabled(on);});
  toggle(mm::Icon::GPS,"GPS","",v.gpsEnabled&&!v.gpsTemporary,[](bool){notice=device::locationToggleUser()?"":"GPS switch failed";});
  toggle(mm::Icon::Messages,"Notification sound","",device::audioState().notifications,[](bool on){device::audioNotifications(on);});
  break;}
 }
 if(rowDialog()){
  rows.clear();const bool direct=activeChat>=0&&activeChat<chatCount&&chats[activeChat].direct;title=activeChat>=0&&activeChat<chatCount?chats[activeChat].name.c_str():"Chat";
  if(channelOptions){int id=channelOptionTarget;bool dm=chats[id].direct;title=chats[id].name.c_str();
   if(deleteChannelConfirm){sub=dm?"delete chat":"delete channel";add(look(K::Info,dm?"Delete this chat? Its messages go too.":"Delete this channel? Its messages go too."));add(look(K::Action,"Cancel"),nullptr,[]{back();});add(look(K::Action,"Delete"),nullptr,[]{deleteSelectedChannel();});}/* Cancel first: OK twice must not delete */
   else{sub=dm?"chat options":"channel options";bool muted=mutedChannels&(uint32_t(1)<<id);
    toggle(mm::Icon::Messages,"Notifications",muted?"Muted":"Sound and banner",!muted,[](bool){channelOption(0);});
    if(!dm)add(withIcon(look(K::Nav,"Key and QR code","share the channel"),mm::Icon::Lock),nullptr,[]{channelOption(1);});
    add(look(K::Action,dm?"Delete chat":"Delete channel"),nullptr,[]{channelOption(2);});}}
  else if(messagesMenu){title="New";sub="chat or channel";
   const char *t2[]={"New direct chat","Create private channel","Join private channel","Join hashtag channel"},*d2[]={"Message a saved contact","Random secret key to share","Name + 32-char secret key","Public, key from the #name"};
   const mm::Icon ic[]={mm::Icon::Contacts,mm::Icon::Plus,mm::Icon::Lock,mm::Icon::Messages};
   for(int i=0;i<mm::MessagesMenuRows;i++)add(withIcon(look(K::Nav,t2[i],d2[i]),ic[i]),nullptr,[i]{messagesMenuChoose(i);});}
  else if(messageActions){const bool mine=contextMessage.mine;const bool muted=mutedChannels&(uint32_t(1)<<activeChat);
   sub=mine?"your message":mm::senderDisplayName(contextMessage.sender);
   add(look(K::Info,clipText(contextMessage.body)));
   if(mine){add(look(K::Nav,"Quote"),nullptr,[]{messageAction(4);});add(look(K::Nav,"Path"),nullptr,[]{messageAction(2);});add(withIcon(look(K::Action,"Resend"),mm::Icon::Messages),nullptr,[]{messageAction(0);});// Resend not first: OK right after the triangle must not send again
    for(auto &peer:repeaterRows(contextSnapshot()))add(look(K::Info,peer.c_str(),"heard by"));}
   else{add(withIcon(look(K::Action,"Reply / mention"),mm::Icon::Compose),nullptr,[]{chatSelecting=false;messageAction(mm::receivedMessageAction(0));});
    add(look(K::Nav,"Read the whole message"),nullptr,[]{messageAction(mm::receivedMessageAction(2));});
    add(look(K::Nav,"View full path"),nullptr,[]{messageAction(mm::receivedMessageAction(1));});
    add(look(K::Nav,"Path on the map"),nullptr,[]{messageAction(mm::MessageMapAction);});
    if(!device::linkFind(contextMessage.body).empty())add(look(K::Nav,"Open the link"),nullptr,[]{linkOpenFromChat(contextMessage.body);});}
   toggle(mm::Icon::Messages,"Notifications",muted?"Muted":"On",!muted,[mine](bool){messageAction(mine?1:mm::receivedMessageAction(3));});}
  else if(clearChatConfirm){sub="clear chat";add(look(K::Info,"Clear every message of this chat?"));add(look(K::Action,"Clear chat"),nullptr,[]{clearActiveChat();});add(look(K::Action,"Cancel"),nullptr,[]{back();});}
  else if(positionShare){sub="send position";add(look(K::Info,"Send your GPS position in this chat?"));add(withIcon(look(K::Action,"Send position"),mm::Icon::GPS),nullptr,[]{shareGpsPosition();});add(look(K::Action,"Cancel"),nullptr,[]{back();});}
  else if(scopePicker){sub="region scope";
   for(int i=0;i<int(regionScopes.size());i++){auto name=regionScopes[i]=="*"?std::string("Global (*)"):regionScopes[i];add(look(K::Nav,name,"",i==scopeFocus?"selected":""),nullptr,[i]{scopeFocus=i;applySelectedScope();});}
   add(withIcon(look(K::Action,"Add a region scope"),mm::Icon::Plus),nullptr,[]{openScopePicker(true);});}
  else if(chatDetails){sub=direct?"this chat":"this channel";/* not "channel options": that is the Chats list menu (notifications, key, delete) */
   const char *labels[]={"Region scope","Add region scope","Clear chat","Send GPS position",v.chatOpenUnread?"Open at: unread":"Open at: latest","Path"};
   for(int i=0;i<mm::chatOptionCount(direct);i++)add(look(K::Nav,labels[i]),nullptr,[i]{chatOption(i);});}
 }
 if(!notice.isEmpty())add(look(K::Info,notice.c_str()));
 return rows;
}
// The next selectable row from `from` that way, or -1.
static bool rowFocusShown=true;// the selected row is on screen (an info page scrolled by hand can hide it)
static int rowNext(int from,int d){for(int i=from+d;i>=0&&i<int(shownRows.size());i+=d)if(mm::rowSelectable(shownRows[i].look))return i;return -1;}
// Arrow up / down: the next selectable row; none left that way, the page scrolls one row (information rows).
static void rowSkip(int d){int i=rowNext(rowFocus,d);if(i>=0)rowFocus=i;else rowScroll+=d*mm::ListPitch;}
// Called by renderSection: the rows for this frame, the selection kept on a selectable row and in view.
void renderRowPage(const mm::State &v){
 std::string title,sub;shownRows=buildRows(v,title,sub);
 if(rowPage!=rowPageId()){rowPage=rowPageId();rowFocus=-1;rowScroll=0;rowRevealed=-2;}
 const int n=int(shownRows.size());if(rowFocus>=n)rowFocus=n-1;
 if(rowFocus<0||!mm::rowSelectable(shownRows[rowFocus].look)){int at=rowNext(rowFocus<0?-1:rowFocus,1);if(at<0)at=rowNext(n,-1);rowFocus=at;}// the nearest selectable row (or none)
 std::vector<mm::ListRow> looks;for(auto &r:shownRows)looks.push_back(r.look);
 if(rowFocus!=rowRevealed){rowScroll=mm::rowReveal(looks,rowFocus,rowScroll);rowRevealed=rowFocus;}
 else rowScroll=std::max(0,std::min(rowScroll,mm::rowsHeight(looks)-(mm::Bottom-mm::ListTop)));// scrolled by hand past the selectable rows
 rowFocusShown=rowFocus>=0&&mm::rowReveal(looks,rowFocus,rowScroll)==rowScroll;
 if(rowFocusShown&&shownRows[rowFocus].focus)shownRows[rowFocus].focus();/* a Field selected: the keys type into its text */
 mm::Renderer(frame).rowList(v,title,sub,looks,rowFocusShown?rowFocus:-1,rowScroll);
}
// Keys on a row page: up / down select, left / right change, OK (or Space) opens / runs. BACK goes to the caller.
// The D-pad on a row page (also with a Field selected, where the letters are text): up / down rows, left / right value.
bool rowArrow(int dx,int dy){
 if(!rowPageOn())return false;
 if(dy){rowSkip(dy<0?-1:1);render();return true;}
 if(dx&&rowFocusShown&&rowFocus>=0&&rowFocus<int(shownRows.size())&&shownRows[rowFocus].change){notice="";shownRows[rowFocus].change(dx<0?-1:1);render();}
 return true;}
bool rowPageKey(char c){
 if(!rowPageOn())return false;
 if((c==3||c=='\b')&&screen==Screen::Section&&section==9&&mapMenu){mapMenu=false;render();return true;}// back to the map
 if(c==3||c=='\b')return false;
 if(!rowFocusShown||rowFocus<0||rowFocus>=int(shownRows.size()))return true;auto row=shownRows[rowFocus];
 if(row.look.kind==mm::RowKind::Field){if(c=='\n'){notice="";if(row.ok)row.ok();else rowSkip(1);render();return true;}return false;}// letters and DEL: the form's own handler types them
 // Letters are not shortcuts here (a stray 'a' on the keyboard turned Wi-Fi off): the D-pad moves (rowArrow).
 if(c=='\n'||c==' '){notice="";if(row.ok)row.ok();else if(row.change)row.change(1);render();return true;}
 return true;// letters do nothing here: every function is a row
}
