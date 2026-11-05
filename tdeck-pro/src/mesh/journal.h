#pragma once
#include "chat_limits.h"
#include <Arduino.h>
#include <FS.h>
#include <vector>
#include "message_index.h"
namespace matter {
// Newest records kept in RAM; older history is read from SD on demand.
constexpr size_t JOURNAL_CACHE_RECORDS=16;
struct Record {uint32_t token=0,timestamp=0;int chat=0,hops=-1;bool mine=false,readLocally=false;String sender,body,scope,status,path,heard;int repeaters=-1;};
class Journal {
 fs::FS *fs=nullptr;
 String journalPath="/mattermesh/messages.tsv";
 bool dirReady=false;// /mattermesh checked once instead of a mkdir on the SD for every appended row
 void ensureDir(){if(!dirReady)dirReady=fs->exists("/mattermesh")||fs->mkdir("/mattermesh");}
 // Appends one row; `position` gets the offset it was written at.
 bool appendRow(const String &row,uint32_t *position=nullptr);
public:
 struct Summary {Record last;uint32_t total=0,order=0,lastRX=0,lastOwn=0,revision=0;};
 Summary summaries[CHAT_SLOTS];MessageIndex index;bool healthy=true,tailOpen=false;
 std::vector<Record> records;
 uint32_t clearedChats=0,readTrackedChats=0,readTokens[CHAT_SLOTS]={};
 void begin(fs::FS &storage,const char *path="/mattermesh/messages.tsv");
 bool append(const Record &record);
 bool update(uint32_t token,const String &status);
 bool repeats(uint32_t token,int count,const String &heard="");
 bool clearChat(int chat);
 bool markRead(int chat,uint32_t token);
 bool markOneRead(int chat,uint32_t token);
 bool readRecord(uint32_t token,Record &record,bool useCache=true);
 // Several uncached reads on one open file (history pages): open with openForRead(), close it after the batch.
 File openForRead(){return fs?fs->open(journalPath.c_str()):File();}
 bool readRecordFrom(File &f,uint32_t token,Record &record);
 const Summary *summary(int chat)const{return chat>=0&&chat<CHAT_SLOTS?&summaries[chat]:nullptr;}
};
}
