#pragma once
#include <cmath>
namespace matter {
// Snapshot of the official MeshCore app catalog, 2026-10-14.
// Full provenance and 433 MHz entries: assets/config/meshcore-radio-presets.json.
struct RadioPreset {const char *name;float frequency,bandwidth;int sf,cr,pathBytes;};
inline constexpr RadioPreset RADIO_PRESETS[]={
 {"Australia",915.8f,250.0f,10,5,0},
 {"Australia (Narrow)",916.575f,62.5f,7,7,0},
 {"Australia (Mid)",915.075f,125.0f,9,5,0},
 {"Australia: SA, WA",923.125f,62.5f,8,8,0},
 {"Australia: QLD",923.125f,62.5f,8,5,0},
 {"Brazil",923.125f,62.5f,8,8,0},
 {"Canada",910.525f,62.5f,7,5,3},
 {"Costa Rica",910.525f,125.0f,11,5,0},
 {"EU/UK (Narrow)",869.618f,62.5f,8,8,0},
 {"EU/UK (Deprecated)",869.525f,250.0f,11,5,0},
 {"Czech Republic (Narrow)",869.432f,62.5f,7,5,2},
 {"Hungary",869.618f,62.5f,7,5,2},
 {"Netherlands",869.618f,62.5f,7,5,0},
 {"Netherlands (Limburg)",869.618f,62.5f,8,8,2},
 {"New Zealand (Narrow)",917.375f,62.5f,7,5,2},
 {"New Zealand (Gisborne)",917.375f,250.0f,11,5,1},
 {"Portugal 868",869.618f,62.5f,7,6,0},
 {"Slovakia",869.618f,62.5f,7,5,2},
 {"Switzerland",869.618f,62.5f,8,8,0},
 {"USA",910.525f,62.5f,7,5,0},
 {"USA - Southern California",927.875f,62.5f,7,5,3},
 {"Vietnam (Narrow)",920.25f,62.5f,8,5,0},
 {"Vietnam (Deprecated)",920.25f,250.0f,11,5,0},
};
constexpr int RADIO_PRESET_COUNT=sizeof(RADIO_PRESETS)/sizeof(RADIO_PRESETS[0]);
inline int radioBand(float frequency){return frequency<900?868:900;}
inline int matchPreset(float frequency,float bandwidth,int sf,int cr){
 for(int i=0;i<RADIO_PRESET_COUNT;i++){auto &p=RADIO_PRESETS[i];if(std::fabs(frequency-p.frequency)<0.0004f&&bandwidth==p.bandwidth&&sf==p.sf&&cr==p.cr)return i;}
 return -1;
}
inline int nextRadioPreset(int current,int band,int direction){
 int index=current<0?(direction<0?0:-1):current;
 for(int i=0;i<RADIO_PRESET_COUNT;i++){index=(index+(direction<0?RADIO_PRESET_COUNT-1:1))%RADIO_PRESET_COUNT;if(radioBand(RADIO_PRESETS[index].frequency)==band)return index;}
 return -1;
}
}
