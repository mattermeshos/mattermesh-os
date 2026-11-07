#include "journal.h"
#include <algorithm>
namespace matter {
#include "tsv_field.h"
static String safe(const String &s){String out;out.reserve(s.length());for(unsigned i=0;i<s.length();i++)out+=(s[i]=='\n'||s[i]=='\r'||s[i]=='\t')?' ':s[i];return out;}
// Tab offsets found in one pass: a record row has 13 fields, and field() rescanned from the start for each one.
struct Tabs{const String &s;int at[16];int n=0;explicit Tabs(const String &row):s(row){at[n++]=0;int p=0;while(n<16){int t=s.indexOf('\t',p);if(t<0)break;p=t+1;at[n++]=p;}}
 String operator[](int i)const{if(i>=n)return "";int b=s.indexOf('\t',at[i]);return b<0?s.substring(at[i]):s.substring(at[i],b);}};
static bool decode(const String &row,Record &r){
 if(!row.startsWith("R1\t")&&!row.startsWith("R2\t"))return false;Tabs f(row);
 r=Record{};r.token=strtoul(f[1].c_str(),nullptr,16);r.chat=f[2].toInt();r.mine=f[3]=="1";
 r.timestamp=strtoul(f[4].c_str(),nullptr,10);r.hops=f[5].toInt();r.sender=f[6];r.scope=f[7];r.status=f[8];r.body=f[9];
 if(row.startsWith("R2\t")){r.path=f[10];r.repeaters=f[11].toInt();r.heard=f[12];}
 return r.token&&r.chat>=0&&r.chat<CHAT_SLOTS&&r.body.length()&&r.body.length()<=160;
}
static void rebootStatus(Record &r){if(r.status.startsWith("Queued")||(r.mine&&r.status=="Pending send"))r.status=r.mine?"Not confirmed / reboot":"Received";}
static bool carriesHeard(const String &row){return field(row,3).length()||field(row,2).toInt()==0;}
static void echo(Record &r,const String &row){r.repeaters=field(row,2).toInt();if(carriesHeard(row))r.heard=field(row,3);}
static void cache(std::vector<Record> &records,const Record &r){if(records.size()>=JOURNAL_CACHE_RECORDS)records.erase(records.begin());records.push_back(r);}
void Journal::begin(fs::FS &storage,const char *path){dirReady=false;
 fs=&storage;journalPath=path;records.clear();index.clear();healthy=true;tailOpen=false;clearedChats=readTrackedChats=0;
 for(int c=0;c<CHAT_SLOTS;c++){auto revision=summaries[c].revision+1;summaries[c]=Summary{};summaries[c].revision=revision;readTokens[c]=localClearThrough[c]=0;}
 File f=fs->open(journalPath.c_str());if(!f)return;
 while(f.available()){
  uint32_t position=f.position();String row=f.readStringUntil('\n');Record r;tailOpen=f.position()==position+row.length();// last row without its new line
  if(decode(row,r)){
   if(index.find(r.token))continue;MessageIndexEntry e;e.token=r.token;e.chat=r.chat;e.mine=r.mine;e.position=position;e.restored=true;
   if(!index.push(e)){healthy=false;break;}
   auto &s=summaries[r.chat];s.last=r;s.total++;s.order=index.size();if(r.mine)s.lastOwn=r.token;else s.lastRX=r.token;cache(records,r);
  }else if(row.startsWith("D1\t")&&field(row,3)=="CLEAR"){
   String id=field(row,1);int c=id.toInt();if(id.length()>2||!id.length()||id[0]<'0'||id[0]>'9'||(id.length()>1&&(id[1]<'0'||id[1]>'9'))||c>=CHAT_SLOTS)continue;
   clearedChats|=uint32_t(1)<<c;readTrackedChats|=uint32_t(1)<<c;readTokens[c]=0;localClearThrough[c]=strtoul(field(row,2).c_str(),nullptr,10);
   for(auto &entry:index)if(entry.chat==c)entry.chat=-1;
   auto rev=summaries[c].revision+1;summaries[c]=Summary{};summaries[c].revision=rev;
   records.erase(std::remove_if(records.begin(),records.end(),[&](const Record &v){return v.chat==c;}),records.end());
  }else if(row.startsWith("V1\t")&&field(row,3)=="READ"){
   int c=field(row,1).toInt();if(c>=0&&c<CHAT_SLOTS){readTrackedChats|=uint32_t(1)<<c;readTokens[c]=strtoul(field(row,2).c_str(),nullptr,16);}
  }else if(row.startsWith("I1\t")&&field(row,3)=="READ_ONE"){
   int c=field(row,1).toInt();auto e=index.find(strtoul(field(row,2).c_str(),nullptr,16));if(e&&e->chat==c&&!e->mine){e->readLocally=true;for(auto &v:records)if(v.token==e->token)v.readLocally=true;}
  }else if(row.startsWith("C1\t")||row.startsWith("S1\t")){
   auto e=index.find(strtoul(field(row,1).c_str(),nullptr,16));if(!e)continue;
   if(row.startsWith("C1\t")){e->echoPosition=position;if(carriesHeard(row))e->heardPosition=position;}else e->statusPosition=position;
   auto apply=[&](Record &v){if(row.startsWith("C1\t"))echo(v,row);else v.status=field(row,2);};
   for(auto &v:records)if(v.token==e->token)apply(v);if(summaries[e->chat].last.token==e->token)apply(summaries[e->chat].last);
  }
 }
 f.close();for(auto &r:records)rebootStatus(r);for(auto &s:summaries)rebootStatus(s.last);
}
bool Journal::readRecord(uint32_t token,Record &r,bool useCache){
 auto e=index.find(token);if(!healthy||!fs||!e)return false;
 if(useCache)for(auto &v:records)if(v.token==token){r=v;return true;}
 File f=fs->open(journalPath.c_str());if(!f)return false;bool ok=readRecordFrom(f,token,r);f.close();return ok;
}
bool Journal::readRecordFrom(File &f,uint32_t token,Record &r){
 auto e=index.find(token);if(!healthy||!f||!e||!f.seek(e->position))return false;
 bool ok=decode(f.readStringUntil('\n'),r)&&r.token==token&&r.chat==e->chat;
 if(ok&&e->statusPosition){ok=f.seek(e->statusPosition);if(ok){auto row=f.readStringUntil('\n');ok=row.startsWith("S1\t")&&strtoul(field(row,1).c_str(),nullptr,16)==token;if(ok)r.status=field(row,2);}}
 auto echoRow=[&](uint32_t at,String &row){if(!f.seek(at))return false;row=f.readStringUntil('\n');return row.startsWith("C1\t")&&strtoul(field(row,1).c_str(),nullptr,16)==token;};
 if(ok&&e->heardPosition&&e->heardPosition!=e->echoPosition){String row;ok=echoRow(e->heardPosition,row);if(ok)echo(r,row);}
 if(ok&&e->echoPosition){String row;ok=echoRow(e->echoPosition,row);if(ok)echo(r,row);}
 if(ok){r.readLocally=e->readLocally;if(e->restored)rebootStatus(r);}return ok;
}
// Every row is appended here. A short write (full card, bus error) leaves a row without its new line: the next row
// starts with one, so it is not glued to the broken one (begin() sees the same at the end of the file).
bool Journal::appendRow(const String &row,uint32_t *position){
 File f=fs->open(journalPath.c_str(),FILE_APPEND);if(!f)return false;String out=row;if(tailOpen)out=String("\n")+row;
 size_t before=f.size();if(position)*position=uint32_t(before)+(tailOpen?1:0);size_t written=f.print(out);f.flush();
 // print() can report the whole row while the buffered write fails at flush(): the size says what reached the card.
 if(f.size()>=before&&f.size()-before<written)written=f.size()-before;f.close();
 bool ok=written==out.length();if(written)tailOpen=out[written-1]!='\n';return ok;
}
bool Journal::append(const Record &r){
 if(!healthy||!fs||!r.token||index.find(r.token)||r.chat<0||r.chat>=CHAT_SLOTS||r.body.length()>160||!r.body.length()||!index.prepare())return false;
 if(!(readTrackedChats&(uint32_t(1)<<r.chat))&&!markRead(r.chat,0))return false;
 ensureDir();uint32_t position=0;
 String row="R2\t"+String(r.token,HEX)+"\t"+String(r.chat)+"\t"+String(r.mine)+"\t"+String(r.timestamp)+"\t"+String(r.hops)+"\t"+safe(r.sender)+"\t"+safe(r.scope)+"\t"+safe(r.status)+"\t"+safe(r.body)+"\t"+safe(r.path)+"\t"+String(r.repeaters)+"\t"+safe(r.heard)+"\n";
 bool ok=appendRow(row,&position);
 if(ok){MessageIndexEntry e;e.token=r.token;e.chat=r.chat;e.mine=r.mine;e.position=position;if(!index.push(e)){healthy=false;return false;}cache(records,r);auto &s=summaries[r.chat];s.last=r;s.total++;s.order=index.size();s.revision++;if(r.mine)s.lastOwn=r.token;else s.lastRX=r.token;}return ok;
}
bool Journal::update(uint32_t token,const String &status){
 auto e=index.find(token);if(!healthy||!fs||!e)return false;
 uint32_t position=0;String row="S1\t"+String(token,HEX)+"\t"+safe(status)+"\n";bool ok=appendRow(row,&position);
 if(ok){e->statusPosition=position;for(auto &r:records)if(r.token==token)r.status=status;auto &s=summaries[e->chat];if(s.last.token==token)s.last.status=status;s.revision++;}return ok;
}
bool Journal::repeats(uint32_t token,int count,const String &heard){
 auto e=index.find(token);if(!healthy||!fs||!e||count<0)return false;
 uint32_t position=0;String row="C1\t"+String(token,HEX)+"\t"+String(count)+"\t"+safe(heard)+"\n";bool ok=appendRow(row,&position);
 if(ok){e->echoPosition=position;if(heard.length()||count==0)e->heardPosition=position;auto apply=[&](Record &r){r.repeaters=count;r.heard=heard;};for(auto &r:records)if(r.token==token)apply(r);auto &s=summaries[e->chat];if(s.last.token==token)apply(s.last);s.revision++;}return ok;
}
bool Journal::clearChat(int chat,uint32_t boundary){
 if(!healthy||!fs||chat<0||chat>=CHAT_SLOTS)return false;ensureDir();String row="D1\t"+String(chat)+"\t"+String(boundary)+"\tCLEAR\n";bool ok=appendRow(row,nullptr);if(!ok)return false;
 clearedChats|=uint32_t(1)<<chat;readTrackedChats|=uint32_t(1)<<chat;readTokens[chat]=0;localClearThrough[chat]=boundary;
 for(auto &e:index)if(e.chat==chat)e.chat=-1;auto revision=summaries[chat].revision+1;summaries[chat]=Summary{};summaries[chat].revision=revision;
 records.erase(std::remove_if(records.begin(),records.end(),[&](const Record &r){return r.chat==chat;}),records.end());return true;
}
bool Journal::markRead(int chat,uint32_t token){
 if(!healthy||!fs||chat<0||chat>=CHAT_SLOTS)return false;if((readTrackedChats&(uint32_t(1)<<chat))&&readTokens[chat]==token)return true;
 ensureDir();String row="V1\t"+String(chat)+"\t"+String(token,HEX)+"\tREAD\n";bool ok=appendRow(row,nullptr);if(ok){readTrackedChats|=uint32_t(1)<<chat;readTokens[chat]=token;}return ok;
}
bool Journal::markOneRead(int chat,uint32_t token){
 auto e=index.find(token);if(!healthy||!fs||!e||e->chat!=chat||e->mine)return false;if(e->readLocally)return true;
 String row="I1\t"+String(chat)+"\t"+String(token,HEX)+"\tREAD_ONE\n";bool ok=appendRow(row,nullptr);if(ok){e->readLocally=true;for(auto &r:records)if(r.token==token)r.readLocally=true;}return ok;
}
}
