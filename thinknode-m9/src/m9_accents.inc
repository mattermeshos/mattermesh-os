// M9 accented letters (the keyboard prints plain QWERTY). In a text field: Ctrl, then a letter -> its first accented
// form; Ctrl again within 2 s -> the next form of that letter (it, es, fr, de, pt, pl, nordic, tr). Ctrl twice before
// a letter: the emoji (chat). The field's own limits stay with its key handler; here only the text is inserted.
struct AccentSet{char base;const char *forms[8];};
static const AccentSet AccentSets[]={
 {'a',{"\xC3\xA0","\xC3\xA1","\xC3\xA2","\xC3\xA4","\xC3\xA3","\xC3\xA5","\xC3\xA6",nullptr}},{'e',{"\xC3\xA8","\xC3\xA9","\xC3\xAA","\xC3\xAB","\xC4\x99",nullptr}},
 {'i',{"\xC3\xAC","\xC3\xAD","\xC3\xAE","\xC3\xAF","\xC4\xB1",nullptr}},{'o',{"\xC3\xB2","\xC3\xB3","\xC3\xB4","\xC3\xB6","\xC3\xB5","\xC3\xB8",nullptr}},
 {'u',{"\xC3\xB9","\xC3\xBA","\xC3\xBB","\xC3\xBC",nullptr}},{'c',{"\xC3\xA7","\xC4\x87","\xC4\x8D",nullptr}},{'n',{"\xC3\xB1","\xC5\x84",nullptr}},
 {'s',{"\xC3\x9F","\xC5\x9B","\xC5\xA1","\xC5\x9F",nullptr}},{'z',{"\xC5\xBC","\xC5\xBA","\xC5\xBE",nullptr}},{'l',{"\xC5\x82",nullptr}},
 {'y',{"\xC3\xBD","\xC3\xBF",nullptr}},{'g',{"\xC4\x9F",nullptr}},
 {'A',{"\xC3\x80","\xC3\x81","\xC3\x82","\xC3\x84","\xC3\x83","\xC3\x85","\xC3\x86",nullptr}},{'E',{"\xC3\x88","\xC3\x89","\xC3\x8A","\xC3\x8B",nullptr}},
 {'I',{"\xC3\x8C","\xC3\x8D","\xC3\x8E","\xC3\x8F","\xC4\xB0",nullptr}},{'O',{"\xC3\x92","\xC3\x93","\xC3\x94","\xC3\x96","\xC3\x98",nullptr}},
 {'U',{"\xC3\x99","\xC3\x9A","\xC3\x9B","\xC3\x9C",nullptr}},{'C',{"\xC3\x87","\xC4\x8C",nullptr}},{'N',{"\xC3\x91",nullptr}},{'S',{"\xC5\xA0","\xC5\x9E",nullptr}},{'Z',{"\xC5\xBB","\xC5\xBD",nullptr}}};
static const AccentSet *accentSet(char c){for(auto &s:AccentSets)if(s.base==c)return &s;return nullptr;}
static int accentForms(const AccentSet *s){int n=0;while(n<8&&s->forms[n])n++;return n;}
static bool accentArmed=false;static const AccentSet *accentLast=nullptr;static int accentIndex=0;static uint32_t accentAt=0;
static void accentPut(String *field,const char *form,bool replace){
 if(replace){if(field==&input)inputEraseBeforeCursor();else eraseLastCodepoint(*field);}
 if(field==&input)inputInsert(String(form));else *field+=form;notice="";repaintSettled();}
// Ctrl in a text field. Returns true when it was taken (accent armed, next form, or the emoji on a second Ctrl).
bool accentCtrl(){
 String *field=editableField();if(!field)return false;
 if(accentLast&&uint32_t(millis()-accentAt)<2000){accentIndex=(accentIndex+1)%accentForms(accentLast);accentAt=millis();accentPut(field,accentLast->forms[accentIndex],true);return true;}
 if(accentArmed&&field==&input&&screen==Screen::Chat){accentArmed=false;openEmojiPicker();render();return true;}
 accentArmed=!accentArmed;accentLast=nullptr;repaintSettled();return true;}
// A key after Ctrl: the letter's first accented form (true), or the key goes on as typed (false).
bool accentKey(char c){
 if(!accentArmed){accentLast=nullptr;return false;}/* typed on: Ctrl no longer changes that accent */accentArmed=false;String *field=editableField();
 const AccentSet *s=field?accentSet(c):nullptr;if(!s){notice="";return false;}
 accentLast=s;accentIndex=0;accentAt=millis();accentPut(field,s->forms[0],false);return true;}
// While Ctrl waits for a letter, or a second Ctrl can still change the accent just typed: a pill under the header says
// so (2026-11-04: Ctrl showed nothing, the user could not tell it was armed).
bool accentHintShown(){return accentArmed||(accentLast&&uint32_t(millis()-accentAt)<2000);}
void drawAccentHint(){
 if(!accentHintShown())return;mm::Components c(frame);
 std::string text=accentArmed?(screen==Screen::Chat&&editableField()==&input?"Accent: type a letter / Ctrl: emoji":"Accent: type a letter"):"Ctrl: next form";
 const int w=c.width(text,mm::Font::ChatMeta)+20,h=22,x=mm::W-mm::Margin-w,y=mm::Top+4;
 c.fillRound(x,y,w,h,11,mm::Accent);c.text(x+10,y+(h-c.lineHeight(mm::Font::ChatMeta))/2,text,mm::Font::ChatMeta,mm::OnAccent);}
// The pill goes away by itself: one repaint when the 2 s for the next form run out.
void accentTick(){static bool was=false;bool now=accentHintShown();if(was&&!now&&!deviceLocked)repaintSettled();was=now;}
