#pragma once
#include <string>
namespace device {
// A message that has no words yet: empty, or only "@[name]" mentions and spaces, so the next letter opens it.
inline bool messageOpening(const std::string &text){
 for(size_t at=0;at<text.size();){if(text[at]==' '){at++;continue;}if(text.compare(at,2,"@[")!=0)return false;size_t end=text.find(']',at+2);if(end==std::string::npos)return false;at=end+1;}
 return true;
}
// Left Shift is the accent key: the vowel typed after it becomes à è ì ò ù. "é" after "ch" (perché, poiché, finché,
// benché ...) and in the words "né" / "sé"; upper case with the right Shift or when the vowel opens a sentence ("È").
// `before` is the text in front of the caret (UTF-8). Not a vowel: empty string, the key is typed as it is.
inline std::string accented(char vowel,const std::string &before,bool upper){
 char v=char(vowel|0x20);const char *vowels="aeiou";int index=-1;for(int i=0;i<5;i++)if(vowels[i]==v)index=i;if(index<0)return std::string();
 size_t start=before.size();while(start>0){char c=char(before[start-1]|0x20);if(c<'a'||c>'z')break;start--;}
 std::string word;for(size_t i=start;i<before.size();i++)word+=char(before[i]|0x20);
 bool acute=v=='e'&&((word.size()>=2&&word.compare(word.size()-2,2,"ch")==0)||word=="n"||word=="s");
 if(!upper&&word.empty()){size_t at=before.size();while(at>0&&before[at-1]==' ')at--;upper=at==0||before[at-1]=='.'||before[at-1]=='!'||before[at-1]=='?'||before[at-1]=='\n'||messageOpening(before);}
 static const unsigned char grave[5]={0xa0,0xa8,0xac,0xb2,0xb9};// à è ì ò ù (U+00E0 ...), upper case is 0x20 lower
 unsigned char second=acute?0xa9:grave[index];if(upper)second-=0x20;
 return std::string{char(0xc3),char(second)};
}
}
