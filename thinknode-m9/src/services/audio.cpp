#include "audio.h"
#include "ringtones.h"
#include <Preferences.h>
#include "../board/board.h"
#include <atomic>
namespace device {
// ThinkNode M9: a piezo buzzer on GPIO9, no codec. Each ringtone note is played as a square wave (buzz()).
static Preferences prefs;static AudioState state;static std::atomic<bool> done{false},cancel{false};static int toneId=0;static uint32_t lastTone=0;
// Volume levels of the M9 settings: 0 Off, 1..59 Low, 60..100 High. Low drives the piezo with short pulses (about
// 5% duty instead of 50%): clearly quieter, same pitch. The emulator only logs the pitch (tone()).
static void buzz(uint32_t hz){
#ifdef MM_EMULATOR
 if(hz)tone(BOARD_BUZZER,hz);else noTone(BOARD_BUZZER);
#else
 constexpr uint8_t Channel=0;static bool attached=false;if(!attached){ledcSetup(Channel,2000,10);ledcAttachPin(BOARD_BUZZER,Channel);attached=true;}
 if(!hz){ledcWrite(Channel,0);return;}
 ledcWriteTone(Channel,hz);ledcWrite(Channel,state.volume>=60?512:48);
#endif
}
static void hold(uint32_t ms){uint32_t until=millis()+ms;while(!cancel.load()&&int32_t(until-millis())>0)vTaskDelay(pdMS_TO_TICKS(5));}
static void worker(void*){
 const Ringtone &r=RINGTONES[toneId];
 for(int i=0;i<r.count&&!cancel.load();i++){const RingNote &n=r.notes[i];
  if(n.toHz){constexpr uint32_t Step=10;for(uint32_t at=0;at<n.ms&&!cancel.load();at+=Step){buzz(n.hz+(int(n.toHz)-int(n.hz))*int(at)/int(n.ms));hold(Step);}}
  else{buzz(n.hz);hold(n.ms);}
  buzz(0);hold(n.gapMs);}
 buzz(0);done=true;vTaskDelete(nullptr);
}
void audioBegin(){prefs.begin("mmaudio",false);state.notifications=prefs.getBool("notify",true);state.volume=constrain(prefs.getInt("volume",20),0,100);state.messageTone=ringtoneIndex(prefs.getInt("tone",RINGTONE_MESSAGE_DEFAULT));state.mentionTone=ringtoneIndex(prefs.getInt("mtone",RINGTONE_MENTION_DEFAULT));pinMode(BOARD_BUZZER,OUTPUT);digitalWrite(BOARD_BUZZER,LOW);state.ready=true;state.message="Buzzer ready";state.revision++;}
static bool start(int tone){if(!state.ready||state.busy)return false;state.lastError=0;toneId=ringtoneIndex(tone);done=false;cancel=false;state.busy=true;state.message="Notification tone";state.revision++;if(xTaskCreate(worker,"mmaudio",4096,nullptr,1,nullptr)!=pdPASS){state.busy=false;state.message="Audio task failed";state.revision++;return false;}return true;}
bool audioNotify(bool test,bool mention){if(!test&&(!state.notifications||state.volume==0||millis()-lastTone<2000))return false;if(!start(mention?state.mentionTone:state.messageTone))return false;lastTone=millis();return true;}
bool audioPreview(int tone){return start(tone);}
void audioRingtone(bool mention,int tone){tone=ringtoneIndex(tone);(mention?state.mentionTone:state.messageTone)=tone;prefs.putInt(mention?"mtone":"tone",tone);state.revision++;}
void audioCancel(){cancel=true;}
void audioNotifications(bool enabled){state.notifications=enabled;prefs.putBool("notify",enabled);state.revision++;}
void audioVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);state.revision++;}
uint32_t audioRevision(){return state.revision;}
bool audioBusy(){return state.busy;}
AudioState audioState(){return state;}
void audioLoop(){
 if(!state.busy||!done.load())return;
 state.playbacks++;
 state.busy=false;state.message="Playback complete";done=false;state.revision++;
}
}
