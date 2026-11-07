#include <cassert>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include "services/ringtones.h"
int main(){
 using namespace device;
 assert(RINGTONE_COUNT==15&&std::string(RINGTONES[7].name)=="Gran Vals"&&RINGTONES[13].notes[0].toHz==900&&std::string(RINGTONES[0].name)=="Classic"&&RINGTONE_MESSAGE_DEFAULT!=RINGTONE_MENTION_DEFAULT);
 assert(ringtoneIndex(-1)==RINGTONE_MESSAGE_DEFAULT&&ringtoneIndex(RINGTONE_COUNT)==RINGTONE_MESSAGE_DEFAULT&&ringtoneIndex(3)==3);
 // The M9 plays them on its buzzer (no speaker samples): distinct names and notes it can sound.
 std::set<std::string> names;
 for(int t=0;t<RINGTONE_COUNT;t++){names.insert(RINGTONES[t].name);assert(RINGTONES[t].count>0);
  for(int n=0;n<RINGTONES[t].count;n++){const auto &note=RINGTONES[t].notes[n];assert(note.hz>=880&&(!note.toHz||note.toHz>=880)&&note.ms>=45);}}
 assert(int(names.size())==RINGTONE_COUNT);
 // Classic keeps the original beep: 880 Hz for 220 ms after the 30 ms warm-up.
 assert(RINGTONES[0].count==1&&RINGTONES[0].notes[0].hz==880&&RINGTONES[0].notes[0].ms==220);
 assert(mentionsName("@[Aria T-Deck] ciao","Aria T-Deck")&&mentionsName("ok @[aria t-deck]!","Aria T-Deck"));
 assert(!mentionsName("Aria T-Deck ciao","Aria T-Deck")&&!mentionsName("@[Aria] ciao","Aria T-Deck")&&!mentionsName("@[Aria T-Deck2]","Aria T-Deck")&&!mentionsName("@[x]","")&&!mentionsName("","Aria"));
 std::cout<<"PASS: "<<RINGTONE_COUNT<<" distinct ringtones (Classic unchanged), notes the buzzer can sound; @[name] mention match is exact and case-insensitive\n";
}
