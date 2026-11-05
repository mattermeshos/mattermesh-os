#include "audio.h"
#include "audio_pcm.h"
#include "ringtones.h"
#include "offline_it.h"
#include "esp_codec.h"
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_reg.h>
#include <soc/gpio_sig_map.h>
#include <Preferences.h>
#include <TDeckMaxBoard.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <atomic>
namespace device {
static EspCodec codec;static Preferences prefs;static ExtensionIOXL9555 *io=nullptr;static AudioState state;static std::atomic<bool> done{false},stop{false},cancel{false};static std::atomic<int> error{0},peak{0};static std::atomic<uint32_t> used{0};static uint8_t *pcm=nullptr;static bool recordingJob=false;static int toneVolume=20,toneId=0;static std::atomic<uint32_t> transmitted{0};static uint32_t lastTone=0;static String result;static bool transcribingJob=false,probeJob=false;static std::atomic<bool> limitReached{false},warming{false};static std::atomic<uint32_t> stoppedAt{0};static uint32_t startedAt=0,stopRequestedAt=0,transcribeStartedAt=0;
constexpr int RATE=DICTATION_RATE,MAX_BYTES=DICTATION_MAX_BYTES;
static void memorySample(){state.internalMin=min(state.internalMin,uint32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));state.psramMin=min(state.psramMin,uint32_t(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));}
// The ESP32-S3 I2S keeps MCLK (4.096 MHz), BCLK and LRCK on the pins after
// codec.close(); their harmonics sit on GPS L1 (MIA-M10Q CW suppression 35-75,
// slow GPS fixes). Park the clock pins low while idle and put back the saved
// GPIO-matrix routing before each open. Input routing (RX loopback) is untouched.
static const int clockPins[3]={BOARD_ES8311_MCLK,BOARD_ES8311_SCLK,BOARD_ES8311_LRCK};static uint32_t clockRouting[3]={};static bool clocksParked=false;
static void i2sClocks(bool on){if(on!=clocksParked)return;for(int i=0;i<3;i++){volatile uint32_t *routing=(volatile uint32_t*)(GPIO_FUNC0_OUT_SEL_CFG_REG+4*clockPins[i]);if(on)*routing=clockRouting[i];else{clockRouting[i]=*routing;gpio_set_level(gpio_num_t(clockPins[i]),0);esp_rom_gpio_connect_out_signal(clockPins[i],SIG_GPIO_OUT_IDX,false,false);}}clocksParked=!on;}
static void amplifier(bool enabled){if(io)io->digitalWrite(BOARD_XL9555_06_AMPLIFIER,enabled?HIGH:LOW);}
static void worker(void*){
 if(recordingJob){uint32_t skip=uint32_t(RATE)*DICTATION_WARMUP_MS/1000;/* frames discarded while the codec settles */while(!stop&&used+640<=MAX_BYTES){int16_t raw[640],buffer[320];int code=codec.read((uint8_t*)raw,sizeof(raw));if(stop.load())break;if(code!=0){error=1;break;}if(skip){skip-=min<uint32_t>(skip,320);if(!skip)warming=false;continue;}int p=microphoneMono(raw,buffer,320);peak=max(peak.load(),p);memcpy(pcm+44+used.load(),buffer,sizeof(buffer));used+=sizeof(buffer);}limitReached=used.load()+640>MAX_BYTES;stoppedAt=millis();}
 else {
  // Feed both I2S slots: ES8311 sees valid PCM regardless of selected DAC slot.
  int16_t buffer[512];const int frames=ringtoneFrames(toneId);
  for(int start=0;start<frames&&!cancel.load();start+=256){int count=min(256,frames-start);for(int i=0;i<count;i++){int16_t sample=ringtoneSample(toneId,start+i,toneVolume);buffer[2*i]=buffer[2*i+1]=sample;}int code=codec.write((uint8_t*)buffer,count*4);if(code){error=code;break;}transmitted+=count*4;}
  // i2s_write queues DMA; do not cut PA power before the final silence drains.
  vTaskDelay(pdMS_TO_TICKS(35));
 }
 done=true;vTaskDelete(nullptr);
}
void audioBegin(ExtensionIOXL9555 &expander,bool ready){prefs.begin("mmaudio",false);state.dictationConfigured=OFFLINE_ITALIAN_COMPILED;state.notifications=prefs.getBool("notify",true);state.volume=constrain(prefs.getInt("volume",20),0,100);state.messageTone=ringtoneIndex(prefs.getInt("tone",RINGTONE_MESSAGE_DEFAULT));state.mentionTone=ringtoneIndex(prefs.getInt("mtone",RINGTONE_MENTION_DEFAULT));if(!ready){state.message="Audio power unavailable";return;}io=&expander;io->pinMode(BOARD_XL9555_06_AMPLIFIER,OUTPUT);io->pinMode(BOARD_XL9555_12_AUDIO_SEL,OUTPUT);amplifier(false);io->digitalWrite(BOARD_XL9555_12_AUDIO_SEL,LOW);codec.setPaParams(-1,3.3f);codec.setPins(BOARD_ES8311_MCLK,BOARD_ES8311_SCLK,BOARD_ES8311_LRCK,BOARD_ES8311_ASDOUT,BOARD_ES8311_DSDIN);state.ready=codec.begin(Wire,BOARD_I2C_ADDR_ES8311,CODEC_TYPE_ES8311);if(state.ready)i2sClocks(false);state.message=state.ready?"ES8311 ready":"ES8311 unavailable";state.revision++;}
static bool start(bool recording,int tone=0,bool probe=false){if(!state.ready||state.busy)return false;if(recording)state.transcript="";state.lastError=0;state.transcribing=false;state.limitReached=false;state.captureMs=state.stopLatencyMs=state.transcribeMs=state.pcmBytes=0;state.internalBefore=state.internalMin=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);state.psramBefore=state.psramMin=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(recording){pcm=(uint8_t*)heap_caps_malloc(MAX_BYTES+44,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!pcm){state.message="Not enough audio memory / draft kept";state.revision++;return false;}}i2sClocks(true);if(codec.open(16,2,RATE)!=0){free(pcm);pcm=nullptr;state.message="Audio stream failed / draft kept";state.revision++;return false;}codec.setVolume(recording?state.volume:100);codec.setGain(12);codec.setMute(!recording);int muteError=codec.setOutputMute(recording);if(muteError){codec.close();i2sClocks(false);free(pcm);pcm=nullptr;state.message="Speaker mute control failed";state.lastError=muteError;state.revision++;return false;}io->digitalWrite(BOARD_XL9555_12_AUDIO_SEL,LOW);amplifier(!recording);toneId=ringtoneIndex(tone);toneVolume=state.volume;transmitted=0;used=0;peak=0;error=0;done=false;stop=false;cancel=false;limitReached=false;stoppedAt=0;startedAt=millis();stopRequestedAt=0;if(recording)offlineItalianReset();probeJob=probe;result="";transcribingJob=false;recordingJob=recording;warming=recording;state.busy=true;state.recording=recording;state.message=recording?"Recording...":"Notification tone";state.revision++;if(xTaskCreate(worker,"mmaudio",8192,nullptr,1,nullptr)!=pdPASS){codec.close();i2sClocks(false);amplifier(false);free(pcm);pcm=nullptr;state.busy=state.recording=false;state.message="Audio task failed";state.revision++;return false;}memorySample();return true;}
bool audioNotify(bool test,bool mention){if(!test&&(!state.notifications||state.volume==0||millis()-lastTone<2000))return false;if(!start(false,mention?state.mentionTone:state.messageTone))return false;lastTone=millis();return true;}
bool audioPreview(int tone){return start(false,tone);}
void audioRingtone(bool mention,int tone){tone=ringtoneIndex(tone);(mention?state.mentionTone:state.messageTone)=tone;prefs.putInt(mention?"mtone":"tone",tone);state.revision++;}
bool audioRecord(){if(!OFFLINE_ITALIAN_COMPILED){state.message="Italian model support not compiled / draft kept";state.revision++;return false;}return start(true);}
bool audioProbeRecord(){return start(true,0,true);}
void audioStop(){if(!recordingJob||!state.busy||stop.load()||done.load())return;stopRequestedAt=millis();stop=true;state.recording=false;codec.setMute(true);state.message="Finishing capture...";state.revision++;}
void audioCancel(){cancel=true;audioStop();stop=true;}

void audioNotifications(bool enabled){state.notifications=enabled;prefs.putBool("notify",enabled);state.revision++;}
void audioVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);state.revision++;}
bool audioDiagnosticPcm(const uint8_t *&data,size_t &bytes){data=state.transcribing&&pcm?pcm+44:nullptr;bytes=data?used.load():0;return data!=nullptr;}
String audioDiagnosticResult(){return state.busy?String(""):result;}
uint32_t audioRevision(){return state.revision;}
// The busy flag alone, without copying the two Strings of AudioState (main loop, every pass).
bool audioBusy(){return state.busy;}
bool audioHoldsPower(){return warming.load()||state.recording||state.transcribing;}// main loop, every pass: no String copies
AudioState audioState(){auto s=state;s.warming=warming.load();s.samples=used/2;s.level=peak.load()*100/32768;s.peak=peak.load();return s;}

static void transcriptionWorker(void*){suppressCaptureStart(reinterpret_cast<int16_t*>(pcm+44),used.load()/2);int code=offlineItalianTranscribe(reinterpret_cast<int16_t*>(pcm+44),used.load()/2,cancel,result);if(code&&!cancel.load())error=code;done=true;vTaskDelete(nullptr);}
bool takeTranscript(String &text){if(!state.transcript.length())return false;text=state.transcript;state.transcript="";return true;}
void audioLoop(){
 if(!state.busy)return;memorySample();if(!done.load())return;
 if(!transcribingJob){codec.setMute(true);codec.setOutputMute(true);amplifier(false);codec.close();i2sClocks(false);if(!recordingJob){state.txBytes=transmitted.load();state.playbacks++;}state.lastError=error.load();state.recording=false;warming=false;state.level=peak.load()*100/32768;state.peak=peak.load();state.samples=used.load()/2;state.pcmBytes=used.load();state.captureMs=state.samples*1000/RATE;state.limitReached=limitReached.load();state.stopLatencyMs=stopRequestedAt?uint32_t(stoppedAt.load()-stopRequestedAt):0;
  if(recordingJob&&OFFLINE_ITALIAN_COMPILED&&!probeJob&&!cancel.load()&&!error.load()&&used.load()>=16000&&peak.load()>100){
   done=false;transcribingJob=true;state.transcribing=true;transcribeStartedAt=millis();state.message=state.limitReached?String(DICTATION_MAX_SECONDS)+"s limit / Transcribing...":"Transcribing...";state.revision++;if(xTaskCreatePinnedToCore(transcriptionWorker,"mmit",32768,nullptr,1,nullptr,0)==pdPASS){memorySample();return;}error=4;
  }
 }
 if(transcribingJob)state.transcribeMs=millis()-transcribeStartedAt;
 state.busy=false;state.transcribing=false;state.lastError=error.load();state.message=cancel.load()?"Dictation cancelled / draft kept":error.load()?(transcribingJob&&error.load()>=10?offlineItalianMessage():"Audio / dictation failed ("+String(error.load())+") / draft kept"):transcribingJob?(result.length()?"Dictation ready / review then Enter":"No speech recognized / draft kept"):!recordingJob?"Playback complete":probeJob?"Capture probe complete / no transcription":!OFFLINE_ITALIAN_COMPILED?"Offline IT unavailable / draft kept":"No speech / draft kept";
 if(state.limitReached&&!transcribingJob&&!cancel.load())state.message=String(DICTATION_MAX_SECONDS)+"s recording limit / "+state.message;
 if(transcribingJob&&!cancel.load()&&!error.load())state.transcript=result;
 if(pcm){memset(pcm,0,44+used.load());free(pcm);}pcm=nullptr;done=false;state.revision++;
}
}
