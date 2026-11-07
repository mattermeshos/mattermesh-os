// Link viewer text helpers: URL detection in chat text, relative URL resolution, HTML to text (reader mode).
#include "services/link_html.h"
#include <cassert>
#include <iostream>
using namespace device;
int main(){
 assert(linkFindUrl("look https://example.com/a?b=1.")=="https://example.com/a?b=1");
 assert(linkFindUrl("(see www.cai.it/rifugi)")=="https://www.cai.it/rifugi");
 assert(linkFindUrl("no link here")=="");assert(linkFindUrl("mailto:me@x.it")=="");assert(linkFindUrl("xhttps://a.b")=="");
 assert(linkFindUrl("https:// broken")=="");assert(linkFindUrl("HTTPS://Host.IT/x!")=="https://Host.IT/x");
 assert(linkFindUrl("see https://it.wikipedia.org/wiki/Cervino_(disambigua)")=="https://it.wikipedia.org/wiki/Cervino_(disambigua)");assert(linkFindUrl("(https://a.it/x)")=="https://a.it/x");
 assert(linkResolve("https://h/a/b?x","?q=1")=="https://h/a/b?q=1");
 {std::string t="<head><meta property=\"og:title\" content=\"OG\"><title>Real</title></head><form><p>Body text</p></form><button>Go<p>after</p>";auto h=linkParseHtml(t.data(),t.size(),"https://a.it",500,5);assert(h.title=="Real"&&h.text.find("Body text")!=std::string::npos&&h.text.find("after")!=std::string::npos);}
 assert(linkHost("https://user@www.example.com:8080/p")=="www.example.com:8080");
 assert(linkResolve("https://a.it/x/y.html?q","z.png")=="https://a.it/x/z.png");
 assert(linkResolve("https://a.it/x/y","/root")=="https://a.it/root");assert(linkResolve("https://a.it","//cdn.it/i.jpg")=="https://cdn.it/i.jpg");
 assert(linkResolve("https://a.it/x/","javascript:alert(1)")=="");assert(linkResolve("https://a.it/","#top")=="");assert(linkResolve("https://a.it","p")=="https://a.it/p");
 assert(linkEntities("a &amp; b &lt;c&gt; &#233; &#x1F600; &bogus;")=="a & b <c> \xc3\xa9 \xf0\x9f\x98\x80 &bogus;");
 std::string html="<html><head><title>T &amp; X</title><meta property=\"og:image\" content=\"/i.jpg\"><style>p{}</style></head><body>"
  "<nav><a href=\"/menu\">Menu</a></nav><main><h1>Head</h1><p>One <b>two</b>\n three</p><script>bad()</script><p>Go <a href=\"b.html\">next page</a>.</p>"
  "<!-- hidden <p>x</p> --><img alt=\"pic\"><footer>Foot</footer></main></body></html>";
 auto h=linkParseHtml(html.data(),html.size(),"https://s.it/d/p.html",1000,10);
 assert(h.title=="T & X");assert(h.image=="https://s.it/i.jpg");
 assert(h.text.find("Menu")==std::string::npos&&h.text.find("bad")==std::string::npos&&h.text.find("hidden")==std::string::npos&&h.text.find("Foot")==std::string::npos);
 assert(h.text.find("Head\nOne two three\nGo next page.")!=std::string::npos);assert(h.text.find("[pic]")!=std::string::npos);
 assert(h.links.size()==1&&h.links[0].url=="https://s.it/d/b.html"&&h.links[0].text=="next page");
 // Limits: text capped, links still collected after the cap.
 std::string big="<p>";for(int i=0;i<500;i++)big+="word ";big+="<a href=\"https://x.it\">x</a></p>";auto c=linkParseHtml(big.data(),big.size(),"https://s.it",100,5);assert(c.text.size()<=101&&c.links.size()==1);
 std::cout<<"PASS: link URL detection, resolution, entities, reader-mode HTML text, title/og:image, links, limits\n";
}
