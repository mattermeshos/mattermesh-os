#include "audio.h"
#include "ringtones.h"
#include "audio_output.h"
#include "music.h"
#include <Preferences.h>
#include <atomic>
// Notification tones for the T-Deck Pro: plays the same synthesised waveforms as the T-Deck Max
// (services/ringtones.h) through the PCM5102A I2S DAC instead of an ES7210/ES8311 codec. Recording/dictation
// needs the PDM mic, not ported here, so those calls stay stubbed (see audio.h).
namespace device {
static Preferences prefs;static AudioState state;static std::atomic<bool> toneBusy{false};
static void toneWorker(void *arg){
 int tone=int(reinterpret_cast<intptr_t>(arg));
 auto &out=i2sOutput();out.SetGain(1.0f)/* ringtoneSample already scales by state.volume below */;out.begin();
 int frames=ringtoneFrames(tone);
 for(int i=0;i<frames;i++){int16_t s=ringtoneSample(tone,i,state.volume);int16_t pair[2]={s,s};while(!out.ConsumeSample(pair))vTaskDelay(1);}
 out.flush();out.stop();toneBusy=false;state.busy=false;state.revision++;vTaskDelete(nullptr);
}
static bool start(int tone){
 if(!state.ready||toneBusy.load()||musicState().playing)return false;
 toneBusy=true;state.busy=true;state.message="Playing tone...";state.revision++;
 if(xTaskCreate(toneWorker,"mmtone",3072,reinterpret_cast<void*>(intptr_t(ringtoneIndex(tone))),1,nullptr)!=pdPASS){toneBusy=false;state.busy=false;state.message="Tone task failed";state.revision++;return false;}
 return true;
}
void audioBegin(){prefs.begin("mmaudio",false);state.dictationConfigured=false;state.notifications=prefs.getBool("notify",true);state.volume=constrain(prefs.getInt("volume",20),0,100);state.messageTone=ringtoneIndex(prefs.getInt("tone",RINGTONE_MESSAGE_DEFAULT));state.mentionTone=ringtoneIndex(prefs.getInt("mtone",RINGTONE_MENTION_DEFAULT));state.ready=true;state.message="PCM5102A ready";}
static uint32_t lastTone=0;
bool audioNotify(bool test,bool mention){if(!test&&(!state.notifications||state.volume==0||millis()-lastTone<2000))return false;if(!start(mention?state.mentionTone:state.messageTone))return false;lastTone=millis();return true;}
bool audioPreview(int tone){return start(tone);}
void audioRingtone(bool mention,int tone){tone=ringtoneIndex(tone);(mention?state.mentionTone:state.messageTone)=tone;prefs.putInt(mention?"mtone":"tone",tone);state.revision++;}
bool audioRecord(){state.message="Recording not supported on this board";state.revision++;return false;}
bool audioProbeRecord(){return false;}
void audioStop(){}
void audioCancel(){}
void audioNotifications(bool enabled){state.notifications=enabled;prefs.putBool("notify",enabled);state.revision++;}
void audioVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);state.revision++;}
bool audioDiagnosticPcm(const uint8_t *&data,size_t &bytes){data=nullptr;bytes=0;return false;}
String audioDiagnosticResult(){return String("");}
uint32_t audioRevision(){return state.revision;}
bool audioBusy(){return state.busy;}
AudioState audioState(){return state;}
bool takeTranscript(String &text){(void)text;return false;}
void audioLoop(){if(state.busy&&!toneBusy.load()){state.busy=false;state.revision++;}}
}
