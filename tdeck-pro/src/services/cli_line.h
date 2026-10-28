#pragma once
#include "../ui/utf8.h"
#include <stddef.h>
#include <string>
#include <vector>
// CLI app: editable command line (cursor, history, completion). Pure C++ for host tests.
namespace device {
constexpr size_t CLI_INPUT_MAX=160;constexpr int CLI_HISTORY_MAX=32;
struct CliLine {
 std::string text;size_t cursor=0;
 static bool continuation(char c){return mm::utf8Continuation(static_cast<unsigned char>(c));}
 void clamp(){if(cursor>text.size())cursor=text.size();while(cursor>0&&cursor<text.size()&&continuation(text[cursor]))cursor--;}
 bool insert(const std::string &value){if(value.empty()||text.size()+value.size()>CLI_INPUT_MAX)return false;clamp();text.insert(cursor,value);cursor+=value.size();return true;}
 bool erase(){clamp();if(!cursor)return false;size_t at=cursor-1;while(at>0&&continuation(text[at]))at--;text.erase(at,cursor-at);cursor=at;return true;}
 void left(){clamp();if(!cursor)return;cursor--;while(cursor>0&&continuation(text[cursor]))cursor--;}
 void right(){clamp();if(cursor>=text.size())return;cursor++;while(cursor<text.size()&&continuation(text[cursor]))cursor++;}
 void set(const std::string &value){text=value.substr(0,CLI_INPUT_MAX);cursor=text.size();}
 void clear(){text.clear();cursor=0;}
};
// Up/Down browse older/newer commands; the draft typed before browsing comes back at the end.
class CliHistory {
 std::vector<std::string> items;int browse=-1;std::string draft;
public:
 void add(const std::string &command){browse=-1;draft.clear();if(command.empty()||(!items.empty()&&items.back()==command))return;items.push_back(command);if(int(items.size())>CLI_HISTORY_MAX)items.erase(items.begin());}
 bool older(CliLine &line){if(items.empty())return false;if(browse<0){draft=line.text;browse=int(items.size());}if(browse==0)return false;line.set(items[--browse]);return true;}
 bool newer(CliLine &line){if(browse<0)return false;if(++browse>=int(items.size())){browse=-1;line.set(draft);draft.clear();return true;}line.set(items[browse]);return true;}
 void reset(){browse=-1;draft.clear();}
 int size()const{return items.size();}
};
// Completes the word before the cursor against `words` (full command strings such as
// "get radio"). Extends the line to the longest common prefix; `matches` lists every
// candidate when more than one fits. Returns true when the line changed.
inline bool cliComplete(CliLine &line,const std::vector<std::string> &words,std::vector<std::string> &matches){
 matches.clear();line.clamp();std::string before=line.text.substr(0,line.cursor);
 for(auto &w:words)if(w.compare(0,before.size(),before)==0)matches.push_back(w);
 if(matches.empty())return false;
 std::string common=matches[0];for(auto &m:matches){size_t i=0;while(i<common.size()&&i<m.size()&&common[i]==m[i])i++;common.resize(i);}
 if(matches.size()==1)common+=' ';
 if(common.size()<=before.size()||line.text.size()-before.size()+common.size()>CLI_INPUT_MAX)return false;
 line.text=common+line.text.substr(line.cursor);line.cursor=common.size();
 if(matches.size()==1)matches.clear();
 return true;
}
// Splits "set radio 869.6,62.5" into words.
inline std::vector<std::string> cliWords(const std::string &text){std::vector<std::string> out;std::string word;for(char c:text){if(c==' '||c=='\t'){if(!word.empty())out.push_back(word);word.clear();}else word+=c;}if(!word.empty())out.push_back(word);return out;}
}
