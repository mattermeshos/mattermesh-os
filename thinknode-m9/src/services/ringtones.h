#pragma once
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <string>
namespace device {
// Notification ringtones: short note sequences played by the buzzer.
// Tone 0 "Classic" is the original 880 Hz / 220 ms beep and stays the message default.
// Retro tones use shape 3 (soft square wave) like monophonic phones, PC speakers and 8-bit consoles:
// "Gran Vals" is the opening of the Tarrega waltz (1902, public domain) known as the classic phone tune,
// "SMS Morse" spells ... -- ...; a note with toHz glides linearly (8-bit zap / jump). Every tone stays under 1 s.
struct RingNote {uint16_t hz,ms,gapMs;uint8_t shape;uint16_t toHz;};// shape: 0 flat, 1 bell (decay + octave), 2 soft (slow attack/release), 3 retro square; toHz>0 glides
struct Ringtone {const char *name;RingNote notes[8];uint8_t count;};
inline constexpr Ringtone RINGTONES[]={
 {"Classic",{{880,220,0,0}},1},
 {"Chirp",{{1319,70,30,0},{1760,90,0,0}},2},
 {"Double",{{880,90,70,0},{880,90,0,0}},2},
 {"Bell",{{1319,450,0,1}},1},
 {"Rising",{{1047,90,20,0},{1319,90,20,0},{1760,170,0,1}},3},
 {"Soft",{{1175,320,0,2}},1},
 {"Alert",{{1568,60,40,0},{1568,60,40,0},{1568,60,0,0}},3},
 {"Gran Vals",{{2637,100,10,3},{2349,100,10,3},{1480,210,10,3},{1661,210,0,3}},4},
 {"SMS Morse",{{1760,45,40,3},{1760,45,40,3},{1760,45,80,3},{1760,120,40,3},{1760,120,80,3},{1760,45,40,3},{1760,45,40,3},{1760,45,0,3}},8},
 {"PC Beep",{{988,140,0,3}},1},
 {"Coin",{{1976,70,0,3},{2637,230,0,3}},2},
 {"Power Up",{{1047,55,0,3},{1319,55,0,3},{1568,55,0,3},{2093,55,0,3},{2637,110,0,3}},5},
 {"Blip",{{1568,60,20,3},{2093,70,0,3}},2},
 {"Zap",{{2400,150,0,3,900}},1},
 {"Jump",{{900,140,0,3,2000}},1},
};
constexpr int RINGTONE_COUNT=sizeof(RINGTONES)/sizeof(RINGTONES[0]);
constexpr int RINGTONE_MESSAGE_DEFAULT=0,RINGTONE_MENTION_DEFAULT=4;
inline int ringtoneIndex(int tone){return tone<0||tone>=RINGTONE_COUNT?RINGTONE_MESSAGE_DEFAULT:tone;}
// A message mentions us when it contains "@[<own companion name>]" (MeshCore reply prefix), case-insensitive.
inline bool mentionsName(const std::string &body,const std::string &name){
 if(name.empty())return false;std::string needle="@["+name+"]";if(body.size()<needle.size())return false;
 auto lower=[](char c){return c>='A'&&c<='Z'?char(c-'A'+'a'):c;};
 for(size_t i=0;i+needle.size()<=body.size();i++){size_t j=0;while(j<needle.size()&&lower(body[i+j])==lower(needle[j]))j++;if(j==needle.size())return true;}
 return false;
}
}
