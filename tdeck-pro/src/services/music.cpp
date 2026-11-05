#include "music.h"
#include "audio_output.h"
#include <SD.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <Preferences.h>
#include <algorithm>
namespace device {
static AudioFileSourceSD *src=nullptr;static AudioGeneratorMP3 *mp3=nullptr;
static MusicState state;static Preferences prefs;static uint32_t lastTickMs=0;
static bool endsWithMp3(const String &name){int n=name.length();return n>4&&name.substring(n-4).equalsIgnoreCase(".mp3");}
std::vector<String> musicList(){
 std::vector<String> out;if(!state.available)return out;
 File dir=SD.open("/music");if(!dir||!dir.isDirectory())return out;
 for(File f=dir.openNextFile();f;f=dir.openNextFile()){
  if(!f.isDirectory()){String name=String(f.name());int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);if(endsWithMp3(name))out.push_back(name);}
  f.close();
 }
 dir.close();std::sort(out.begin(),out.end(),[](const String &a,const String &b){return a.compareTo(b)<0;});
 return out;
}
static void cleanup(){if(mp3){mp3->stop();delete mp3;mp3=nullptr;}if(src){delete src;src=nullptr;}}
void musicBegin(bool sdReady){prefs.begin("mmmusic",false);state.volume=constrain(prefs.getInt("volume",70),0,100);state.available=sdReady;i2sOutput().SetGain(state.volume/100.0f);}
bool musicPlay(const String &filename){
 if(!state.available)return false;
 cleanup();
 String path="/music/"+filename;
 src=new AudioFileSourceSD(path.c_str());
 if(!src->isOpen()){delete src;src=nullptr;state.message="Could not open "+filename;state.revision++;return false;}
 mp3=new AudioGeneratorMP3();
 i2sOutput().SetGain(state.volume/100.0f);
 if(!mp3->begin(src,&i2sOutput())){delete mp3;mp3=nullptr;delete src;src=nullptr;state.message="Playback failed / not a valid MP3";state.revision++;return false;}
 state.track=filename;state.playing=true;state.paused=false;state.elapsedSec=0;lastTickMs=millis();state.message="";state.revision++;
 return true;
}
void musicPause(){if(state.playing&&!state.paused){state.paused=true;state.revision++;}}
void musicResume(){if(state.playing&&state.paused){state.paused=false;lastTickMs=millis();state.revision++;}}
void musicStop(){cleanup();if(state.playing){state.playing=state.paused=false;state.revision++;}}
void musicVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);i2sOutput().SetGain(state.volume/100.0f);state.revision++;}
void musicLoop(){
 if(!state.playing||state.paused||!mp3)return;
 uint32_t now=millis();if(now-lastTickMs>=1000){state.elapsedSec+=(now-lastTickMs)/1000;lastTickMs=now;state.revision++;}
 if(!mp3->isRunning()||!mp3->loop()){cleanup();state.playing=state.paused=false;state.message="Playback finished";state.revision++;}
}
MusicState musicState(){return state;}
}
