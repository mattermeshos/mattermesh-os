#pragma once
// Link viewer text helpers (no Arduino types, host-testable in sim/test_link_html.cpp): URL detection in chat
// text, relative URL resolution and a tolerant HTML-to-text pass (title, readable text, links, og:image).
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
namespace device {
struct LinkHtmlLink {std::string text,url;};
struct LinkHtml {std::string title,text,image;std::vector<LinkHtmlLink> links;};

inline bool linkIsSpace(char c){return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f';}
inline char linkLower(char c){return c>='A'&&c<='Z'?char(c+32):c;}
inline bool linkStartsI(const char *s,size_t n,size_t at,const char *word){size_t i=0;for(;word[i];i++)if(at+i>=n||linkLower(s[at+i])!=word[i])return false;return true;}

// First http(s):// or www. URL in a chat message; ")", ".", ",", "!", "?", ";", ":" and quotes at its end are dropped.
inline std::string linkFindUrl(const std::string &text,size_t *spanAt=nullptr,size_t *spanLen=nullptr){
 size_t n=text.size();
 for(size_t i=0;i<n;i++){
  bool http=linkStartsI(text.data(),n,i,"http://")||linkStartsI(text.data(),n,i,"https://"),www=!http&&linkStartsI(text.data(),n,i,"www.");
  if(!http&&!www)continue;if(i>0&&!linkIsSpace(text[i-1])&&text[i-1]!='('&&text[i-1]!='<'&&text[i-1]!='"'&&text[i-1]!='\'')continue;
  size_t end=i;while(end<n&&!linkIsSpace(text[end])&&text[end]!='<'&&text[end]!='>'&&text[end]!='"')end++;
  // Trailing punctuation is not part of the link; ")" only when unbalanced (keeps Wikipedia's ..._(disambigua)).
  for(;;){if(end<=i)break;char last=text[end-1];if(std::string(".,!?;:'\"]").find(last)!=std::string::npos){end--;continue;}
   if(last==')'){int open=0,close=0;for(size_t k=i;k<end;k++){open+=text[k]=='(';close+=text[k]==')';}if(close>open){end--;continue;}}break;}
  std::string url=text.substr(i,end-i);if(www)url="https://"+url;
  for(size_t k=0;k<url.size()&&url[k]!=':';k++)url[k]=linkLower(url[k]);// HTTPS:// -> https://
  size_t scheme=url.find("://");if(scheme==std::string::npos||url.size()<=scheme+3||url[scheme+3]=='/')continue;
  if(spanAt)*spanAt=i;if(spanLen)*spanLen=end-i;// where the link sits in `text` (as written)
  return url;
 }
 return "";
}
// "https://host/a/b?q" -> origin "https://host", directory "https://host/a/".
inline std::string linkOrigin(const std::string &base){size_t s=base.find("://");if(s==std::string::npos)return "";size_t p=base.find_first_of("/?#",s+3);return p==std::string::npos?base:base.substr(0,p);}
inline std::string linkHost(const std::string &url){std::string o=linkOrigin(url);size_t s=o.find("://");std::string h=s==std::string::npos?o:o.substr(s+3);size_t at=h.rfind('@');if(at!=std::string::npos)h=h.substr(at+1);return h;}
inline std::string linkResolve(const std::string &base,std::string href){
 {size_t a=0,b=href.size();while(a<b&&linkIsSpace(href[a]))a++;while(b>a&&linkIsSpace(href[b-1]))b--;href=href.substr(a,b-a);}
 if(href.empty()||href[0]=='#')return "";
 if(linkStartsI(href.data(),href.size(),0,"http://")||linkStartsI(href.data(),href.size(),0,"https://"))return href;
 if(href.find(':')!=std::string::npos&&href.find(':')<href.find('/'))return "";// mailto:, javascript:, tel:, data: ...
 if(href.rfind("//",0)==0){size_t s=base.find("://");return (s==std::string::npos?"https":base.substr(0,s))+":"+href;}
 std::string origin=linkOrigin(base);if(origin.empty())return "";
 if(href[0]=='/')return origin+href;
 if(href[0]=='?')return base.substr(0,base.find_first_of("?#",origin.size()))+href;
 std::string dir=base.substr(0,base.find_first_of("?#",origin.size()));size_t slash=dir.rfind('/');dir=slash!=std::string::npos&&slash>=origin.size()?dir.substr(0,slash+1):origin+"/";
 return dir+href;
}
inline void linkAppendUtf8(std::string &out,uint32_t cp){
 if(cp<0x80)out+=char(cp);else if(cp<0x800){out+=char(0xC0|cp>>6);out+=char(0x80|(cp&63));}
 else if(cp<0x10000){out+=char(0xE0|cp>>12);out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
 else if(cp<0x110000){out+=char(0xF0|cp>>18);out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
}
// Decodes the common named entities and &#N; / &#xH; (others are kept as written).
inline std::string linkEntities(const std::string &in){
 std::string out;out.reserve(in.size());
 for(size_t i=0;i<in.size();i++){
  if(in[i]!='&'){out+=in[i];continue;}size_t semi=std::string::npos;for(size_t k=i+1;k<in.size()&&k<=i+10;k++)if(in[k]==';'){semi=k;break;}if(semi==std::string::npos){out+='&';continue;}// look 10 bytes ahead only
  std::string e=in.substr(i+1,semi-i-1);uint32_t cp=0;bool ok=true;
  if(!e.empty()&&e[0]=='#'){char *end=nullptr;cp=e.size()>1&&(e[1]=='x'||e[1]=='X')?strtoul(e.c_str()+2,&end,16):strtoul(e.c_str()+1,&end,10);ok=end&&!*end&&cp>0;}
  else if(e=="amp")cp='&';else if(e=="lt")cp='<';else if(e=="gt")cp='>';else if(e=="quot")cp='"';else if(e=="apos")cp='\'';else if(e=="nbsp")cp=' ';
  else if(e=="hellip")cp=0x2026;else if(e=="mdash")cp=0x2014;else if(e=="ndash")cp=0x2013;else if(e=="rsquo")cp=0x2019;else if(e=="lsquo")cp=0x2018;else if(e=="ldquo")cp=0x201C;else if(e=="rdquo")cp=0x201D;
  else if(e=="egrave")cp=0xE8;else if(e=="eacute")cp=0xE9;else if(e=="agrave")cp=0xE0;else if(e=="ograve")cp=0xF2;else if(e=="ugrave")cp=0xF9;else if(e=="igrave")cp=0xEC;else if(e=="euro")cp=0x20AC;else if(e=="copy")cp=0xA9;
  else ok=false;
  if(!ok){out+='&';continue;}linkAppendUtf8(out,cp);i=semi;
 }
 return out;
}
// Value of attribute `name` inside a tag body ("a href=... class=..."), quotes optional; "" when absent.
constexpr size_t LinkMaxAttr=512,LinkMaxTitle=160;// longer attribute values (URLs) are dropped, titles cut
inline std::string linkAttr(const std::string &tag,const char *name){
 size_t n=tag.size(),len=strlen(name);
 for(size_t i=0;i+len<=n;i++){
  if(!linkStartsI(tag.data(),n,i,name)||(i>0&&!linkIsSpace(tag[i-1])))continue;size_t j=i+len;while(j<n&&linkIsSpace(tag[j]))j++;if(j>=n||tag[j]!='=')continue;j++;while(j<n&&linkIsSpace(tag[j]))j++;
  size_t from=j,e;if(j<n&&(tag[j]=='"'||tag[j]=='\'')){char q=tag[j];from=j+1;e=tag.find(q,from);if(e==std::string::npos)e=n;}else{e=j;while(e<n&&!linkIsSpace(tag[e])&&tag[e]!='>')e++;}
  return e-from>LinkMaxAttr?std::string():linkEntities(tag.substr(from,e-from));
 }
 return "";
}
// A page that is mostly a photo (photo hosts, or little text around an og:image): the viewer opens the picture
// directly and keeps the text for PAGE.
inline bool linkPhotoPage(const std::string &url,const LinkHtml &page){
 if(page.image.empty())return false;std::string host=linkHost(url);for(auto &c:host)c=linkLower(c);
 static const char *const hosts[]={"ibb.co","imgbb.com","imgur.com","postimg.cc","postimages.org","flickr.com","flic.kr","photos.app.goo.gl","photos.google.com","500px.com","unsplash.com","pixabay.com","pexels.com","tinypic.com","prnt.sc","gyazo.com","imgbox.com","instagram.com"};
 for(auto h:hosts){std::string d(h);if(host==d||(host.size()>d.size()&&host.compare(host.size()-d.size(),d.size(),d)==0&&host[host.size()-d.size()-1]=='.'))return true;}
 return page.text.size()<600;
}
// HTML -> text. Block tags start a new line; script/style/noscript/svg/head content is skipped (title and meta
// are read from the head); runs of spaces collapse; at most maxText bytes of text and maxLinks links are kept.
inline size_t linkFindI(const char *s,size_t n,const char *word,size_t from=0){for(size_t i=from;i<n;i++)if(linkStartsI(s,n,i,word))return i;return n;}
inline LinkHtml linkParseHtml(const char *s,size_t n,const std::string &base,size_t maxText,size_t maxLinks){
 LinkHtml out;
 // Reader mode: when the page marks its content (<main>, <article>, Wikipedia's mw-content-text), text and links
 // are taken from there on; title and meta tags are read from the whole document.
 // The most specific marker wins: Wikipedia's content block, then <article>, then <main>.
 size_t textFrom=linkFindI(s,n,"id=\"mw-content-text\"");if(textFrom>=n)textFrom=linkFindI(s,n,"<article");if(textFrom>=n)textFrom=linkFindI(s,n,"<main");if(textFrom>=n)textFrom=0;
 size_t pos=0;std::string pending,ogTitle;bool inTitle=false,inLink=false,space=false;std::string linkUrl,linkText;
 auto newline=[&](){while(!out.text.empty()&&out.text.back()==' ')out.text.pop_back();if(!out.text.empty()&&out.text.back()!='\n')out.text+='\n';space=false;};
 auto emit=[&](const std::string &raw){
  std::string t=linkEntities(raw);
  for(char c:t){if(linkIsSpace(c)){space=true;continue;}bool gap=space;space=false;
   if(inTitle){if(out.title.size()<LinkMaxTitle){if(gap&&!out.title.empty())out.title+=' ';out.title+=c;}continue;}
   if(pos<textFrom)continue;if(inLink&&linkText.size()<80){if(gap&&!linkText.empty())linkText+=' ';linkText+=c;}
   if(out.text.size()>=maxText)continue;if(gap&&!out.text.empty()&&out.text.back()!='\n')out.text+=' ';out.text+=c;}};
 static const char *const skip[]={"script","style","noscript","svg","template","iframe","nav","aside","footer","select"};
 static const char *const blocks[]={"p","div","br","li","tr","h1","h2","h3","h4","h5","h6","section","article","header","blockquote","form","pre","table","ul","ol","hr","figure","figcaption","dt","dd"};
 size_t i=0;
 while(i<n){
  pos=i;if(s[i]!='<'){size_t e=i;while(e<n&&s[e]!='<')e++;emit(std::string(s+i,e-i));i=e;continue;}
  if(linkStartsI(s,n,i,"<!--")){const char *end=nullptr;for(size_t k=i+4;k+2<n;k++)if(s[k]=='-'&&s[k+1]=='-'&&s[k+2]=='>'){end=s+k;break;}i=end?size_t(end-s)+3:n;continue;}
  size_t e=i+1;char q=0;while(e<n){char c=s[e];if(q){if(c==q)q=0;}else if(c=='"'||c=='\'')q=c;else if(c=='>')break;e++;}
  std::string tag(s+i+1,(e<n?e:n)-i-1);i=e<n?e+1:n;
  bool closing=!tag.empty()&&tag[0]=='/';size_t a=closing?1:0;size_t b=a;while(b<tag.size()&&!linkIsSpace(tag[b])&&tag[b]!='/'&&tag[b]!='>')b++;
  std::string name;for(size_t k=a;k<b;k++)name+=linkLower(tag[k]);
  bool skipped=false,selfClosing=!tag.empty()&&tag.back()=='/';
  for(auto sk:skip)if(name==sk&&!closing&&!selfClosing){// jump to the matching close tag (kept in place when there is none)
   std::string close=std::string("</")+sk;size_t k=linkFindI(s,n,close.c_str(),i);if(k<n){i=k;skipped=true;}break;}
  if(skipped)continue;
  if(name=="title"){inTitle=!closing;space=false;continue;}
  if(name=="meta"&&!closing){std::string prop=linkAttr(tag,"property");if(prop.empty())prop=linkAttr(tag,"name");for(auto &c:prop)c=linkLower(c);
   if((prop=="og:image"||prop=="og:image:url"||prop=="og:image:secure_url"||prop=="twitter:image")&&out.image.empty())out.image=linkResolve(base,linkAttr(tag,"content"));
   if(prop=="og:title"&&ogTitle.empty())ogTitle=linkAttr(tag,"content");continue;}
  if(name=="img"&&!closing){std::string alt=linkAttr(tag,"alt");if(!alt.empty()&&alt.size()<80)emit(" ["+alt+"] ");continue;}
  if(name=="a"){
   if(!closing){inLink=pos>=textFrom;linkText.clear();linkUrl=linkResolve(base,linkAttr(tag,"href"));}
   else if(inLink){inLink=false;if(!linkUrl.empty()&&out.links.size()<maxLinks){bool dup=false;for(auto &l:out.links)if(l.url==linkUrl)dup=true;if(!dup)out.links.push_back({linkText.empty()?linkUrl:linkText,linkUrl});}}
   continue;}
  for(auto bl:blocks)if(name==bl){newline();break;}
 }
 if(out.title.empty())out.title=ogTitle.substr(0,LinkMaxTitle);// <title> first, og:title as fallback
 while(!out.text.empty()&&(out.text.back()=='\n'||out.text.back()==' '))out.text.pop_back();
 size_t start=0;while(start<out.text.size()&&out.text[start]=='\n')start++;out.text.erase(0,start);
 // Collapse runs of empty lines.
 std::string compact;int blank=0;for(char c:out.text){if(c=='\n'){if(++blank>1)continue;}else blank=0;compact+=c;}out.text=compact;
 return out;
}
}
