#pragma once
#include <Arduino.h>
#include "dictation_policy.h"
#include <ExtensionIOXL9555.hpp>
namespace device {
constexpr int MICROPHONE_KEYCODE=34;
struct AudioState {bool ready=false,notifications=true,busy=false,recording=false,warming=false/* MIC held, codec settling: nothing kept yet */,dictationConfigured=false;int messageTone=0,mentionTone=4,volume=20,level=0,peak=0,lastError=0;uint32_t revision=0,samples=0,playbacks=0,txBytes=0;bool transcribing=false,limitReached=false;uint32_t captureMs=0,stopLatencyMs=0,transcribeMs=0,pcmBytes=0,internalBefore=0,psramBefore=0,internalMin=0,psramMin=0;String message,transcript;};
bool audioDiagnosticPcm(const uint8_t *&data,size_t &bytes);
String audioDiagnosticResult();
uint32_t audioRevision();
bool takeTranscript(String &text);
void audioBegin(ExtensionIOXL9555 &io,bool ready);void audioLoop();AudioState audioState();bool audioBusy();bool audioHoldsPower();bool audioNotify(bool test=false,bool mention=false);bool audioPreview(int tone);void audioRingtone(bool mention,int tone);void audioNotifications(bool enabled);void audioVolume(int value);bool audioRecord();bool audioProbeRecord();void audioStop();void audioCancel();
}
