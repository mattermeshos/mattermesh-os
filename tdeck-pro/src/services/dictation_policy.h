#pragma once
#include "../ui/utf8.h"
#include <stdint.h>
#include <string>
namespace device {
constexpr int DICTATION_RATE=16000,DICTATION_MAX_SECONDS=6;
// Codec settling: the first second after the MIC press is discarded; the timer and the kept audio start together after it.
constexpr uint32_t DICTATION_WARMUP_MS=1000;
constexpr size_t DICTATION_MAX_BYTES=DICTATION_RATE*DICTATION_MAX_SECONDS*2;
#if defined(MM_OFFLINE_ITALIAN) && MM_OFFLINE_ITALIAN
constexpr bool OFFLINE_ITALIAN_COMPILED=true;
#else
constexpr bool OFFLINE_ITALIAN_COMPILED=false;
#endif
// Hardware validation is distinct from a successful host/firmware build.
// Kept false until an explicitly authorized device inference test passes.
constexpr bool OFFLINE_ITALIAN_AVAILABLE=false;
enum class MicSource : uint8_t { None,Keyboard,Touch };
struct DictationHold {
 MicSource source=MicSource::None;
 bool press(MicSource owner){if(source!=MicSource::None)return false;source=owner;return true;}
 bool release(MicSource owner){if(source!=owner)return false;source=MicSource::None;return true;}
 void cancel(){source=MicSource::None;}
};
inline std::string appendDictation(const std::string &draft,const std::string &text,size_t maxBytes,bool &trimmed){
 trimmed=false;if(text.empty())return draft;
 std::string separator=draft.empty()||draft.back()==' '?"":" ";
 if(draft.size()+separator.size()>=maxBytes){trimmed=true;return draft;}
 size_t room=maxBytes-draft.size()-separator.size(),end=text.size();
 if(end>room){end=mm::utf8Floor(text,room);trimmed=true;}
 return end?draft+separator+text.substr(0,end):draft;
}
}
