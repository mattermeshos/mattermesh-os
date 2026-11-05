#pragma once
#include <stdint.h>
namespace device {
// Notification vibration patterns: segments of {ms, strength %} played in order; strength scales the user's
// vibration intensity and 0 is a pause. The DRV2605 drives an ERM motor that needs ~50 ms to spin up, so every
// pulse is >=50 ms. Pattern 0 "Single" is the original 120 ms pulse and stays the message default.
struct VibrationStep {uint16_t ms;uint8_t strength;};
struct VibrationPattern {const char *name;VibrationStep steps[9];uint8_t count;};
static constexpr VibrationPattern VIBRATIONS[]={
 {"Single",{{120,100}},1},
 {"Double",{{90,100},{110,0},{90,100}},3},
 {"Triple",{{60,100},{80,0},{60,100},{80,0},{60,100}},5},
 {"Long",{{350,100}},1},
 {"Heartbeat",{{70,70},{90,0},{140,100}},3},
 {"Ramp",{{80,40},{80,70},{120,100}},3},
 {"Rapid",{{50,100},{60,0},{50,100},{60,0},{50,100},{60,0},{50,100}},7},
};
constexpr int VIBRATION_COUNT=sizeof(VIBRATIONS)/sizeof(VIBRATIONS[0]);
constexpr int VIBRATION_MESSAGE_DEFAULT=0,VIBRATION_MENTION_DEFAULT=1;
inline int vibrationIndex(int pattern){return pattern<0||pattern>=VIBRATION_COUNT?VIBRATION_MESSAGE_DEFAULT:pattern;}
inline int vibrationMs(int pattern){const auto &p=VIBRATIONS[vibrationIndex(pattern)];int ms=0;for(int i=0;i<p.count;i++)ms+=p.steps[i].ms;return ms;}
// Strength (0..100 % of the user intensity) at elapsed ms, or -1 once the pattern has ended.
inline int vibrationStrength(int pattern,uint32_t elapsed){const auto &p=VIBRATIONS[vibrationIndex(pattern)];for(int i=0;i<p.count;i++){if(elapsed<p.steps[i].ms)return p.steps[i].strength;elapsed-=p.steps[i].ms;}return -1;}
}
