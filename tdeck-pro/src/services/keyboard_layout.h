#pragma once
#include <stdint.h>
#include <string>
// Keyboard languages (Settings > Keyboard languages), shared by the T-Deck Max and Plus: pure functions, no board or
// font code. The physical keys stay QWERTY; a layout gives each letter key 'a'..'z' its letter, and the accent key
// (left Shift) tapped before a key gives the letters that have no key of their own: one tap the first variant, two
// taps the second (Russian: accent + З = Х, accent twice + З = Ъ). Alt + Space cycles the enabled languages.
namespace mm_kbd {
enum class Lang : uint8_t { Italian, English, German, French, Spanish, Portuguese, Russian, Ukrainian };
constexpr int LangCount=8;
constexpr uint32_t DefaultMask=1u<<int(Lang::Italian);
// Order of the Settings list and of Alt + Space: alphabetical by native name, Latin scripts first, then Cyrillic.
// The enum values are the NVS bits and never change.
constexpr Lang Order[LangCount]={Lang::German,Lang::English,Lang::Spanish,Lang::French,Lang::Italian,Lang::Portuguese,Lang::Russian,Lang::Ukrainian};
inline int orderPosition(Lang l){for(int i=0;i<LangCount;i++)if(Order[i]==l)return i;return 0;}
struct LangInfo { const char *code,*name,*english; bool cyrillic; };
inline const LangInfo &info(Lang l){
 static const LangInfo list[LangCount]={{"IT","Italiano","Italian",false},{"EN","English","English",false},
  {"DE","Deutsch","German",false},{"FR","Français","French",false},{"ES","Español","Spanish",false},
  {"PT","Português","Portuguese",false},{"RU","Русский","Russian",true},{"UK","Українська","Ukrainian",true}};
 return list[int(l)<LangCount?int(l):0];
}
inline std::string utf8(uint32_t cp){
 std::string s;
 if(cp<0x80)s+=char(cp);
 else if(cp<0x800){s+=char(0xc0|(cp>>6));s+=char(0x80|(cp&63));}
 else{s+=char(0xe0|(cp>>12));s+=char(0x80|((cp>>6)&63));s+=char(0x80|(cp&63));}
 return s;
}
// Lower case code point -> upper case for the letters these layouts produce (Latin-1 and Cyrillic U+0400..U+0491).
inline uint32_t upperCase(uint32_t cp){
 if(cp>='a'&&cp<='z')return cp-32;
 if(cp>=0xe0&&cp<=0xfe&&cp!=0xf7)return cp-32;// à..þ, not ÷
 if(cp>=0x430&&cp<=0x44f)return cp-32;// а..я
 if(cp>=0x450&&cp<=0x45f)return cp-80;// ѐ..џ (ё є і ї)
 if(cp==0x491)return 0x490;// ґ
 return cp;// ß and the rest keep their form
}
inline int keyIndex(char key){char k=char(key|0x20);return k>='a'&&k<='z'?k-'a':-1;}
// Russian ЙЦУКЕН on QWERTY keys (a..z), lower case.
inline uint32_t russianLetter(int k){
 static const uint16_t map[26]={0x444,0x438,0x441,0x432,0x443,0x430,0x43f,0x440,0x448,0x43e,0x43b,0x434,0x44c,
  0x442,0x449,0x437,0x439,0x43a,0x44b,0x435,0x433,0x43c,0x446,0x447,0x43d,0x44f};
 return map[k];
}
// Letter typed by a letter key; `upper` for Shift / caps. Not a letter key: empty string (the key works as before).
inline std::string letter(Lang l,char key,bool upper){
 int k=keyIndex(key);if(k<0)return std::string();
 uint32_t cp='a'+k;
 if(info(l).cyrillic){cp=russianLetter(k);if(l==Lang::Ukrainian&&k=='s'-'a')cp=0x456;}// і instead of ы
 return utf8(upper?upperCase(cp):cp);
}
// Letters behind the accent key for one key: index 0 for one accent tap, 1 for two taps ... (lower case).
struct Variants { uint16_t cp[4]; int count; };
inline Variants variants(Lang l,char key){
 int k=keyIndex(key);Variants v{{0,0,0,0},0};if(k<0)return v;
 auto set=[&](std::initializer_list<uint16_t> list){for(auto c:list)if(v.count<4)v.cp[v.count++]=c;};
 char c=char('a'+k);
 switch(l){
  case Lang::Italian:if(c=='a')set({0xe0});else if(c=='e')set({0xe8,0xe9});else if(c=='i')set({0xec});else if(c=='o')set({0xf2});else if(c=='u')set({0xf9});break;
  case Lang::English:break;
  case Lang::German:if(c=='a')set({0xe4});else if(c=='o')set({0xf6});else if(c=='u')set({0xfc});else if(c=='s')set({0xdf});else if(c=='e')set({0xe9});break;
  case Lang::French:if(c=='e')set({0xe9,0xe8,0xea,0xeb});else if(c=='a')set({0xe0,0xe2});else if(c=='c')set({0xe7});else if(c=='u')set({0xf9,0xfb,0xfc});else if(c=='i')set({0xee,0xef});else if(c=='o')set({0xf4});else if(c=='y')set({0xff});break;
  case Lang::Spanish:if(c=='a')set({0xe1});else if(c=='e')set({0xe9});else if(c=='i')set({0xed});else if(c=='o')set({0xf3});else if(c=='u')set({0xfa,0xfc});else if(c=='n')set({0xf1});break;
  case Lang::Portuguese:if(c=='a')set({0xe1,0xe0,0xe2,0xe3});else if(c=='e')set({0xe9,0xea});else if(c=='i')set({0xed});else if(c=='o')set({0xf3,0xf4,0xf5});else if(c=='u')set({0xfa});else if(c=='c')set({0xe7});break;
  // Missing ЙЦУКЕН keys are reached from their left neighbour on a standard keyboard: З→Х→Ъ, Д→Ж→Э, Ь→Б→Ю, Е→Ё.
  case Lang::Russian:if(c=='p')set({0x445,0x44a});else if(c=='l')set({0x436,0x44d});else if(c=='m')set({0x431,0x44e});else if(c=='t')set({0x451});break;
  case Lang::Ukrainian:if(c=='p')set({0x445,0x457});else if(c=='l')set({0x436,0x454});else if(c=='m')set({0x431,0x44e});else if(c=='u')set({0x491});break;
 }
 return v;
}
inline std::string variant(Lang l,char key,int index,bool upper){
 auto v=variants(l,key);if(!v.count)return std::string();uint32_t cp=v.cp[((index%v.count)+v.count)%v.count];
 return utf8(upper?upperCase(cp):cp);
}
// The letters the next key gives after `taps` accent presses (0 = accent held), in key order, separated by `sep`:
// the bar over the composer while the accent is armed. Empty when the language has none at that count.
inline std::string accentBar(Lang l,int taps,const char *sep){
 std::string out;int index=taps>0?taps-1:0;
 for(char key='a';key<='z';key++){auto v=variants(l,key);if(index>=v.count)continue;if(!out.empty())out+=sep;out+=utf8(v.cp[index]);}
 return out;
}
// Accent key help shown in Settings ("" when the accent key adds nothing). Keys are named as printed on the
// keycaps (the Cyrillic layouts put ЙЦУКЕН on the QWERTY keys).
inline const char *accentHelp(Lang l){
 switch(l){
  case Lang::Italian:return "Accent + vowel: à è ì ò ù (2 taps + e: é)";
  case Lang::German:return "Accent + a o u s e: ä ö ü ß é";
  case Lang::French:return "Accent + e a c u i o y: é à ç ù î ô ÿ (2-4 taps: è ê ë â û ü ï)";
  case Lang::Spanish:return "Accent + vowel: á é í ó ú, + n: ñ (u, 2 taps: ü)";
  case Lang::Portuguese:return "Accent + vowel: á é í ó ú, + c: ç (2-4 taps: à â ã ê ô õ)";
  case Lang::Russian:return "Keys in ЙЦУКЕН order. Accent + P: Х (2 taps: Ъ), + L: Ж (Э), + M: Б (Ю), + T: Ё";
  case Lang::Ukrainian:return "Keys in ЙЦУКЕН order, S: І. Accent + P: Х (2 taps: Ї), + L: Ж (Є), + M: Б (Ю), + U: Ґ";
  default:return "";
 }
}
inline int enabledCount(uint32_t mask){int n=0;for(int i=0;i<LangCount;i++)if(mask&(1u<<i))n++;return n;}
inline uint32_t sanitize(uint32_t mask){mask&=(1u<<LangCount)-1;return mask?mask:DefaultMask;}
// Next enabled language after `current` in Order (Alt + Space); `current` itself when it is the only one.
inline Lang next(uint32_t mask,Lang current){
 mask=sanitize(mask);int at=orderPosition(current);
 for(int step=1;step<=LangCount;step++){Lang l=Order[(at+step)%LangCount];if(mask&(1u<<int(l)))return l;}return current;
}
// The language to type with: `current` if enabled, else the first enabled one.
inline Lang active(uint32_t mask,Lang current){mask=sanitize(mask);if(mask&(1u<<int(current)))return current;return next(mask,current);}
// Settings search: case-insensitive match on the code, the native and the English name (ASCII query).
inline bool matches(Lang l,const std::string &query){
 if(query.empty())return true;
 auto &i=info(l);
 auto lower=[](std::string s){for(auto &c:s)if(c>='A'&&c<='Z')c=char(c+32);return s;};
 std::string q=lower(query);
 return lower(i.code).find(q)!=std::string::npos||lower(i.name).find(q)!=std::string::npos||lower(i.english).find(q)!=std::string::npos;
}
}
