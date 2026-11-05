#pragma once
// MatterMesh Web: the page the T-Deck serves (web_app.inc). One file: style, markup and script; the data comes from
// /api/... on the T-Deck. Palettes = src/ui/components.cpp (Alpine Light, Night Ridge, Expedition). Drawn for a
// 1440 px window and zoomed with the screen (1080p, 2K, 4K). The map uses Leaflet + OpenStreetMap from the internet.
static const char WebPage[]=R"HTML(<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MatterMesh Web</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"><script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>
:root{--bg:#F4F1EA;--fg:#101B22;--acc:#225E4A;--on:#FFFFFF;--surf:#DAE2D9;--alt:#D0DBE5;--muted:#45545B;--line:#AEB8B2;--mist:#BFCCD5;--sbar:#F4F1EA;--sfg:#101B22}
body.night{--bg:#0D1823;--fg:#EAF2F7;--acc:#3ECDE6;--on:#0A151E;--surf:#17293B;--alt:#1B3145;--muted:#93A8B8;--line:#2E465C;--mist:#23394E;--sbar:#081119;--sfg:#EAF2F7}
body.exp{--bg:#ECE4D3;--fg:#1E1D17;--acc:#C05028;--on:#FFF6E8;--surf:#F7F2E6;--alt:#E2D8C6;--muted:#5A5648;--line:#8E8772;--mist:#D5C8AE;--sbar:#485133;--sfg:#F2EBDA}
*{box-sizing:border-box}html,body{margin:0;height:100%}
body{background:var(--bg);color:var(--fg);font:15px/1.4 Inter,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
button{font:600 14px Inter,"Segoe UI",sans-serif;cursor:pointer}
.app{height:calc(100vh / var(--z,1));display:flex;flex-direction:column}
.sbar{height:34px;flex:none;display:flex;align-items:center;gap:14px;padding:0 18px;border-bottom:1px solid var(--line);background:var(--sbar);color:var(--sfg);font-weight:600;font-size:14px}
.brand{display:flex;align-items:center;gap:8px;font-weight:800;letter-spacing:.5px}.brand svg{width:22px;height:22px}.sp{flex:1}
.pill{display:inline-flex;align-items:center;gap:6px;padding:3px 10px;border-radius:12px;font-size:12px;font-weight:700;background:var(--acc);color:var(--on)}
.pill.ghost{background:transparent;border:1.5px solid var(--muted);color:var(--muted)}
.themes{display:inline-flex;border:1px solid var(--line);border-radius:9px;overflow:hidden}.themes b{padding:3px 10px;font-size:12px;font-weight:600;cursor:pointer;display:flex;align-items:center;gap:6px}.themes b i{width:10px;height:10px;border-radius:50%;display:inline-block}.themes b.on{background:var(--acc);color:var(--on)}
.desk{flex:1;display:flex;min-height:0}
.rail{width:74px;flex:none;border-right:1px solid var(--line);display:flex;flex-direction:column;align-items:center;padding:12px 0;gap:6px}
.rail a{width:58px;height:58px;border-radius:12px;display:flex;flex-direction:column;align-items:center;justify-content:center;font-size:11px;font-weight:600;color:var(--fg);gap:3px;cursor:pointer}
.rail a.on{background:var(--acc);color:var(--on)}.rail a span{font-size:20px;line-height:1}.grow{flex:1}
.list{width:330px;flex:none;border-right:1px solid var(--line);display:flex;flex-direction:column;min-height:0}
h2{margin:14px 16px 8px;font-size:22px;display:flex;align-items:center}h2 small{margin-left:auto;font-size:13px;color:var(--muted);font-weight:500}
.search{margin:0 14px 10px;height:38px;border-radius:9px;background:var(--surf);border:0;padding:0 12px;color:var(--fg);font-size:15px}
.rows{flex:1;overflow:auto;padding:0 10px}
.chat{display:flex;gap:10px;align-items:center;padding:9px 10px;border-radius:10px;margin-bottom:4px;background:var(--surf);cursor:pointer}
.chat.on{background:var(--acc);color:var(--on)}
.av{width:36px;height:36px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-weight:800;color:#fff;flex:none}
.chat .t{flex:1;min-width:0}.chat .n{font-weight:700;font-size:16px;display:flex}.chat .n i{margin-left:auto;font-style:normal;font-weight:500;font-size:12px;opacity:.8}
.chat .p{font-size:13px;opacity:.85;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.badge{min-width:22px;height:18px;border-radius:9px;background:var(--acc);color:var(--on);font-size:11px;font-weight:800;display:inline-flex;align-items:center;justify-content:center;padding:0 6px}
.chat.on .badge{background:var(--on);color:var(--acc)}
.newbtn{margin:10px auto 14px;display:inline-flex;align-items:center;gap:8px;padding:8px 18px;border-radius:18px;background:var(--surf);border:0;color:var(--fg)}
.conv{flex:1;display:flex;flex-direction:column;min-width:0}
.convh{height:58px;flex:none;display:flex;align-items:center;gap:12px;padding:0 20px;border-bottom:1px solid var(--line)}
.convh .nm{font-size:21px;font-weight:800}.convh .sub{color:var(--muted);font-size:13px}
.btn{border:0;border-radius:9px;padding:8px 14px;background:var(--surf);color:var(--fg)}.btn.acc{background:var(--acc);color:var(--on)}
.msgs{flex:1;overflow:auto;padding:16px 24px;display:flex;flex-direction:column;gap:4px}
.b{max-width:62%;padding:6px 12px 7px;border-radius:12px;background:var(--alt);font-size:16.5px;line-height:1.3;white-space:pre-wrap;word-wrap:break-word;align-self:flex-start}
.b .s{font-size:13px;font-weight:600;color:var(--acc);display:block;margin-bottom:1px}
.meta{font-size:12px;color:var(--muted);margin:2px 0 10px 2px;align-self:flex-start}.meta i{font-style:normal;opacity:.6;margin:0 5px}
.b.me{align-self:flex-end;background:var(--acc);color:var(--on)}.meta.me{align-self:flex-end;margin-right:2px}
.mention{font-weight:700}
.b .s{cursor:pointer}.b .s:hover{text-decoration:underline}
.acts{margin-left:10px;opacity:0;transition:opacity .15s}.acts a{cursor:pointer;color:var(--acc);font-weight:700;margin-left:10px}.meta:hover .acts,.b:hover+.meta .acts{opacity:1}
.composer{position:relative}.mpop{position:absolute;left:0;bottom:calc(100% + 6px);background:var(--bg);border:1px solid var(--line);border-radius:12px;box-shadow:0 10px 30px rgba(0,0,0,.25);max-height:300px;overflow:auto;min-width:260px;padding:6px;z-index:5}
.mpop div{padding:7px 10px;border-radius:8px;cursor:pointer;font-weight:600}.mpop div.on{background:var(--acc);color:var(--on)}.mpop small{display:block;color:var(--muted);padding:4px 10px;font-weight:700;letter-spacing:.6px}
.route{display:flex;flex-direction:column;gap:4px;margin-top:6px}.route span{background:var(--surf);border-radius:8px;padding:5px 10px;font-size:14px}.route b{color:var(--acc)}
.composer{margin:0 20px 16px;border:1.5px solid var(--line);border-radius:12px;display:flex;align-items:center;padding:8px 8px 8px 16px;gap:10px}
.composer input{flex:1;border:0;background:transparent;color:var(--fg);font-size:16px;outline:none}.composer .cnt{font-size:12px;color:var(--muted)}
.info{width:280px;flex:none;border-left:1px solid var(--line);padding:16px;display:flex;flex-direction:column;gap:10px;overflow:auto}
.lbl{font-size:11px;font-weight:700;letter-spacing:.8px;color:var(--muted);margin-top:6px}
.card{background:var(--surf);border-radius:10px;padding:10px 12px}
.kv{display:flex;justify-content:space-between;gap:10px;font-size:14px;padding:3px 0}.kv b{font-weight:700;text-align:right}
.center{flex:1;display:flex;align-items:center;justify-content:center;background:linear-gradient(180deg,var(--bg) 0%,var(--bg) 55%,var(--mist) 100%)}
.login{width:430px;background:var(--bg);border:1px solid var(--line);border-radius:18px;padding:30px 34px;box-shadow:0 18px 50px rgba(16,27,34,.12);text-align:center}
.login svg{width:64px;height:64px}.login h1{font-size:24px;margin:12px 0 2px;letter-spacing:.5px}.login p{color:var(--muted);margin:6px 0 18px}
.pin{display:flex;gap:10px;justify-content:center;margin:12px 0 18px}.pin input{width:52px;height:60px;border-radius:12px;background:var(--surf);border:0;font-size:28px;font-weight:800;text-align:center;color:var(--fg)}
.pin input:focus{outline:2.5px solid var(--acc)}
.err{color:#b3261e;font-weight:600;min-height:20px}
.dim{position:fixed;inset:0;background:rgba(16,27,34,.42);display:flex;align-items:center;justify-content:center}
.modal{width:620px;background:var(--bg);border-radius:16px;padding:22px 26px;box-shadow:0 20px 60px rgba(0,0,0,.3)}
.modal h3{margin:0 0 4px;font-size:22px}.tabs{display:flex;gap:8px;margin:14px 0;flex-wrap:wrap}
.tab{padding:7px 14px;border-radius:8px;background:var(--surf);font-weight:600;font-size:14px;cursor:pointer}.tab.on{background:var(--acc);color:var(--on)}
.field{background:var(--surf);border-radius:10px;padding:8px 12px;margin-bottom:10px}.field small{display:block;font-size:11px;font-weight:700;letter-spacing:.6px;color:var(--muted)}
.field input{width:100%;border:0;background:transparent;font-size:17px;margin-top:2px;color:var(--fg);outline:none}
.grid{flex:1;display:grid;grid-template-columns:repeat(3,1fr);gap:16px;padding:18px 22px;align-content:start;overflow:auto}
.big{font-size:34px;font-weight:800;line-height:1.1}.wide{grid-column:span 2}
table{width:100%;border-collapse:collapse;font-size:14px}td,th{text-align:left;padding:6px;border-bottom:1px solid var(--line)}th{font-size:11px;letter-spacing:.7px;color:var(--muted)}
.mapbox{height:calc(280px*var(--z,1));border-radius:calc(10px*var(--z,1));background:var(--mist);margin-top:calc(8px*var(--z,1));zoom:calc(1/var(--z,1))}/* the map at the screen's own pixels: Leaflet tiles show seams under a page zoom */
.mmpin{font:700 12px Inter,sans-serif;background:var(--bg);color:var(--fg);border:1.5px solid var(--acc);border-radius:7px;padding:1px 7px;white-space:nowrap;box-shadow:0 1px 4px rgba(0,0,0,.25)}.mmpin.rpt{background:var(--acc);color:var(--on)}
.mmme{width:14px;height:14px;border-radius:50%;background:#fff;border:4px solid var(--acc)}
body.night .leaflet-tile{filter:invert(.92) hue-rotate(185deg) brightness(1.05) contrast(.85)}body.exp .leaflet-tile{filter:sepia(.35) saturate(.9)}
.tcards{display:flex;gap:18px;margin-top:10px;flex-wrap:wrap}.tcard{width:300px;border-radius:14px;background:var(--surf);padding:10px;cursor:pointer}.tcard.on{outline:3px solid var(--acc)}
.tprev{height:150px;border-radius:9px;overflow:hidden;display:flex;flex-direction:column}.tprev .tb{height:14px}.tprev .tm{flex:1;display:flex;gap:6px;padding:8px}.tprev .tl{width:35%;display:flex;flex-direction:column;gap:5px}.tprev .tl i{height:18px;border-radius:5px}.tprev .tc{flex:1;display:flex;flex-direction:column;gap:6px}.tprev .tc i{height:15px;border-radius:7px;width:70%}
.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:var(--fg);color:var(--bg);padding:9px 16px;border-radius:10px;font-weight:600;display:none}
.empty{color:var(--muted);text-align:center;margin-top:40px}
</style></head><body>
<svg style="display:none"><symbol id="logo" viewBox="0 0 64 64"><rect x="2" y="2" width="60" height="60" rx="14" fill="#152131"/><polygon points="6,52 12,44 15,43 30,9 34,7.5 36,11 38,22 44,26 47,33 52,37 58,52" fill="#fff"/><rect x="7" y="52" width="50" height="3.5" fill="#fff"/><polyline points="32,18 26,33 42,37 21,46" fill="none" stroke="#152131" stroke-width="2.6" stroke-linejoin="round" stroke-linecap="round"/><circle cx="32" cy="18" r="3.8" fill="#152131"/><circle cx="32" cy="18" r="1.6" fill="#fff"/><circle cx="26" cy="33" r="2.8" fill="#152131"/><circle cx="42" cy="37" r="2.8" fill="#152131"/><circle cx="21" cy="46" r="3.8" fill="#152131"/><circle cx="21" cy="46" r="1.6" fill="#fff"/></symbol></svg>
<div class="app" id="app"></div><div class="toast" id="toast"></div>
<script>
'use strict';
const $=s=>document.querySelector(s),app=$('#app');
const esc=s=>String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
let token=sessionStorage.mmTok||'',state=null,view='chats',current=-1,messages=[],lastMsgKey='',net=null,map=null,newForm=1;
function fit(){const z=Math.max(0.9,Math.min(2.8,innerWidth/1440));document.documentElement.style.zoom=z;document.documentElement.style.setProperty('--z',z)}addEventListener('resize',fit);fit();
const THEMES=['','night','exp'];
function themeIdx(){const own=localStorage.mmTheme;if(own!==undefined&&own!=='follow')return +own;return state?state.device.theme:0}
function applyTheme(){document.body.className=THEMES[themeIdx()]||''}
function toast(t){const e=$('#toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2600)}
async function api(path,body){const r=await fetch('/api/'+path+(path.includes('?')?'&':'?')+'t='+encodeURIComponent(token),body?{method:'POST',body:JSON.stringify(body)}:{});
 if(r.status===401){token='';sessionStorage.mmTok='';login();throw new Error('session closed')}const j=await r.json();if(r.status>=400)throw new Error(j.error||'error');return j}
const AV=['#2f7d63','#8a6d3b','#3b6e8a','#6b4f8a','#8a3b4f','#4f8a3b','#8a5a3b','#3b8a86'];
function avatar(c){const ch=c.direct?(c.name[0]||'?'):(c.name[0]==='#'?'#':c.name[0]);return '<div class="av" style="background:'+AV[c.id%AV.length]+'">'+esc(ch.toUpperCase())+'</div>'}
function mentions(t){return esc(t).replace(/@\[([^\]]+)\]/g,'<span class="mention">@[$1]</span>')}
function sbar(){const d=state?state.device:{};return '<div class="sbar"><span class="brand"><svg><use href="#logo"/></svg>MATTERMESH WEB</span><span>'+esc(d.name||'T-Deck')+'</span>'+(token?'<span class="pill">connected</span>':'')+'<span class="sp"></span>'+
 '<span class="themes">'+[['Alpine','#225E4A'],['Night','#3ECDE6'],['Expedition','#C05028']].map((t,i)=>'<b data-th="'+i+'" class="'+(themeIdx()===i?'on':'')+'"><i style="background:'+t[1]+'"></i>'+t[0]+'</b>').join('')+'</span>'+
 (d.battery!==undefined?'<span>'+(d.wifiRssi?'ᯤ ':'')+d.battery+'%</span><span>'+esc(d.clock||'')+'</span>':'')+'</div>'}
function bindThemes(){for(const b of document.querySelectorAll('[data-th]'))b.onclick=()=>{localStorage.mmTheme=b.dataset.th;applyTheme();map=null;render()}}
// ---- login
function login(){applyTheme();app.innerHTML=sbar()+'<div class="center"><div class="login"><svg><use href="#logo"/></svg><h1>MATTERMESH OS</h1><p>Your T-Deck in the browser, on your Wi-Fi.</p>'+
 '<p style="margin:0 0 4px">Type the code shown on the T-Deck<br><small>Settings › Companion link › MatterMesh Web</small></p><div class="pin">'+[0,1,2,3].map(i=>'<input maxlength="1" inputmode="numeric" id="p'+i+'">').join('')+'</div>'+
 '<div class="err" id="perr"></div><button class="btn acc" id="go" style="width:100%;padding:12px;font-size:16px">Connect</button></div></div>';bindThemes();
 const ins=[0,1,2,3].map(i=>$('#p'+i));ins.forEach((e,i)=>{e.oninput=()=>{e.value=e.value.replace(/\D/g,'');if(e.value&&i<3)ins[i+1].focus();if(ins.every(x=>x.value))go()};e.onkeydown=k=>{if(k.key==='Backspace'&&!e.value&&i)ins[i-1].focus()}});ins[0].focus();
 async function go(){const pin=ins.map(x=>x.value).join('');const r=await fetch('/api/login',{method:'POST',body:JSON.stringify({pin})});const j=await r.json().catch(()=>({}));
  if(r.ok&&j.token){token=j.token;sessionStorage.mmTok=token;await refresh();render()}else{$('#perr').textContent=j.error||'Wrong code';ins.forEach(x=>x.value='');ins[0].focus()}}
 $('#go').onclick=go}
// ---- data
async function refresh(){state=await api('state');applyTheme();if(current<0&&state.chats.length)current=state.chats[0].id}
async function loadChat(){if(current<0)return;const j=await api('chat?id='+current);const key=j.messages.map(m=>m.token+m.meta+m.heard).join(',');const changed=key!==lastMsgKey;lastMsgKey=key;messages=j.messages;return changed}
// ---- views
function rail(){return '<nav class="rail">'+[['chats','✉','Chats'],['network','📡','Network']].map(v=>'<a data-v="'+v[0]+'" class="'+(view===v[0]?'on':'')+'"><span>'+v[1]+'</span>'+v[2]+'</a>').join('')+
 '<span class="grow"></span><a data-v="settings" class="'+(view==='settings'?'on':'')+'"><span>⚙</span>Settings</a></nav>'}
function chatsView(){const q=($('#q')||{}).value||'';const list=state.chats.filter(c=>c.name.toLowerCase().includes(q.toLowerCase()));const c=state.chats.find(x=>x.id===current);const unread=state.chats.reduce((n,x)=>n+x.unread,0);
 let h='<div class="list"><h2>Chats <small>'+state.chats.length+(state.chats.length===1?' chat':' chats')+(unread?' · '+unread+' unread':'')+'</small></h2><input class="search" id="q" placeholder="⌕ Search chats" value="'+esc(q)+'"><div class="rows">';
 for(const x of list)h+='<div class="chat'+(x.id===current?' on':'')+'" data-c="'+x.id+'">'+avatar(x)+'<div class="t"><div class="n">'+esc(x.name)+'<i>'+esc(x.age||'')+'</i></div><div class="p">'+esc(x.preview||'No messages')+'</div></div>'+(x.unread?'<span class="badge">'+x.unread+'</span>':'')+'</div>';
 h+='</div><div style="text-align:center"><button class="newbtn" id="newch">＋ New channel</button></div></div>';
 if(!c)return h+'<main class="conv"><div class="empty">No chats</div></main>';
 h+='<main class="conv"><div class="convh"><span class="nm">'+esc(c.name)+'</span><span class="pill ghost">'+(c.direct?'direct':'channel')+'</span><span class="sub">'+(c.direct?'':'scope '+esc(c.scope))+'</span><span class="sp"></span></div><div class="msgs" id="msgs">';
 if(!messages.length)h+='<div class="empty">No messages yet</div>';
 messages.forEach((m,i)=>{h+='<div class="b'+(m.mine?' me':'')+'">'+(!m.mine&&!c.direct?'<span class="s" data-info="'+i+'" title="Message info">'+esc(m.sender)+'</span>':'')+mentions(m.body)+'</div><div class="meta'+(m.mine?' me':'')+'">'+esc(m.time)+'<i>|</i>'+esc(m.meta)+
  '<span class="acts">'+(m.mine?'':'<a data-reply="'+i+'">Reply</a>')+'<a data-info="'+i+'">Info</a></span></div>'});
 h+='</div><div class="composer"><input id="txt" placeholder="Write message… (Enter sends)" maxlength="'+c.limit+'"><span class="cnt" id="cnt">0 / '+c.limit+'</span><button class="btn acc" id="send">Send</button></div></main>';
 h+='<aside class="info"><div class="lbl">'+(c.direct?'CHAT':'CHANNEL')+'</div><div class="card"><div class="kv"><span>Type</span><b>'+(c.direct?'Direct':c.name==='Public'?'Public':c.name[0]==='#'?'Hashtag':'Private')+'</b></div>'+(c.direct?'':'<div class="kv"><span>Region scope</span><b>'+esc(c.scope)+'</b></div>')+'<div class="kv"><span>Notifications</span><b>'+(c.muted?'Muted':'On')+'</b></div><div class="kv"><span>Messages</span><b>'+c.total+'</b></div></div>'+
  '<div class="lbl">ON THE T-DECK</div><div class="card" style="font-size:13px;color:var(--muted)">Messages sent here go out from the T-Deck radio, as if typed on it.</div></aside>';
 return h}
function netView(){if(!net)return '<div class="conv"><div class="empty">Loading…</div></div>';const r=net.radio,m=net.mesh,w=net.wifi,d=net.device;
 const rows=net.nodes.slice(0,12).map(n=>'<tr><td><b>'+esc(n.name)+'</b></td><td>'+(n.type===2?'Repeater':n.type===1?'Companion':'Node')+'</td><td>'+(n.hops<0?'—':n.hops===0?'direct':n.hops+' hop'+(n.hops>1?'s':''))+'</td><td>'+(n.live?n.rssi+' dBm · '+n.snr+' dB':'saved')+'</td><td>'+esc(n.age)+'</td></tr>').join('');
 return '<div class="conv"><div class="convh"><span class="nm">Network</span><span class="sub">live from the T-Deck · every 5 s</span><span class="sp"></span><span class="pill">'+(r.ready?'RX ready':'radio offline')+'</span></div><div class="grid">'+
 '<div class="card"><div class="lbl" style="margin:0">LORA</div><div class="big">'+r.freq+'</div><div style="color:var(--muted)">MHz</div><div class="kv"><span>Bandwidth</span><b>'+r.bw+' kHz</b></div><div class="kv"><span>SF / CR</span><b>SF'+r.sf+' · 4/'+r.cr+'</b></div><div class="kv"><span>TX power</span><b>'+r.power+' dBm</b></div></div>'+
 '<div class="card"><div class="lbl" style="margin:0">MESH SINCE BOOT</div><div class="kv"><span>Packets heard</span><b>'+m.received+'</b></div><div class="kv"><span>Sent</span><b>'+m.sent+'</b></div><div class="kv"><span>Contacts</span><b>'+m.contacts+'</b></div><div class="kv"><span>Heard now</span><b>'+m.live+'</b></div><div class="kv"><span>Last packet</span><b>'+m.rssi+' dBm · '+m.snr+' dB</b></div></div>'+
 '<div class="card"><div class="lbl" style="margin:0">WI-FI</div><div class="kv"><span>Network</span><b>'+esc(w.ssid)+'</b></div><div class="kv"><span>IP</span><b>'+esc(w.ip)+'</b></div><div class="kv"><span>Signal</span><b>'+w.rssi+' dBm</b></div><div class="lbl">DEVICE</div><div class="kv"><span>Battery</span><b>'+d.battery+' % · '+(d.mv/1000).toFixed(2)+' V</b></div><div class="kv"><span>Uptime</span><b>'+esc(d.uptime)+'</b></div><div class="kv"><span>Firmware</span><b>'+esc(d.version)+'</b></div></div>'+
 '<div class="card wide"><div class="lbl" style="margin:0">HEARD RECENTLY</div><table><tr><th>NAME</th><th>TYPE</th><th>ROUTE</th><th>SIGNAL</th><th>LAST</th></tr>'+rows+'</table></div>'+
 '<div class="card"><div class="lbl" style="margin:0">NODES WITH A POSITION</div><div id="nodemap" class="mapbox"></div></div></div></div>'}
function setView(){const c=state.chats.length;return '<div class="conv" style="padding:18px 26px;overflow:auto"><div style="font-size:24px;font-weight:800">Settings</div><div class="lbl" style="margin-top:16px">THEME OF THIS PAGE</div><div class="tcards">'+
 [['Alpine Light','Paper, alpine green',['#F4F1EA','#225E4A','#DAE2D9','#F4F1EA']],['Night Ridge','Night, glacier cyan',['#0D1823','#3ECDE6','#17293B','#0D1823']],['Expedition','Parchment, burnt orange',['#ECE4D3','#C05028','#F7F2E6','#485133']]].map((t,i)=>
 '<div class="tcard'+(localStorage.mmTheme==String(i)?' on':'')+'" data-th="'+i+'"><div class="tprev" style="background:'+t[2][0]+'"><div class="tb" style="background:'+t[2][3]+'"></div><div class="tm"><div class="tl"><i style="background:'+t[2][1]+'"></i><i style="background:'+t[2][2]+'"></i><i style="background:'+t[2][2]+'"></i></div><div class="tc"><i style="background:'+t[2][2]+'"></i><i style="background:'+t[2][1]+';align-self:flex-end"></i></div></div></div><div style="font-weight:800;font-size:17px;margin-top:8px">'+t[0]+'</div><div style="color:var(--muted);font-size:13px">'+t[1]+'</div></div>').join('')+'</div>'+
 '<div class="card" style="margin-top:18px;max-width:936px;display:flex;align-items:center;gap:14px;cursor:pointer" id="follow"><div style="flex:1"><b>Use the T-Deck theme</b><br><span style="color:var(--muted);font-size:13px">The page follows Settings › Themes on the device.</span></div><span class="pill'+(localStorage.mmTheme===undefined||localStorage.mmTheme==='follow'?'':' ghost')+'">'+(localStorage.mmTheme===undefined||localStorage.mmTheme==='follow'?'ON':'OFF')+'</span></div>'+
 '<div class="lbl" style="margin-top:16px">SESSION</div><div class="card" style="max-width:936px"><div class="kv"><span>T-Deck</span><b>'+esc(state.device.name)+' · '+esc(state.device.version)+'</b></div><div class="kv"><span>Chats</span><b>'+c+'</b></div><div class="kv"><span>Close the session</span><b>here, or on the T-Deck page</b></div></div><button class="btn" id="logout" style="margin-top:12px">Log out</button></div>'}
function render(){if(!token||!state)return login();applyTheme();const keep=($('#txt')||{}).value||'';const q=($('#q')||{}).value;
 app.innerHTML=sbar()+'<div class="desk">'+rail()+(view==='chats'?chatsView():view==='network'?netView():setView())+'</div>';
 bindThemes();for(const a of document.querySelectorAll('[data-v]'))a.onclick=()=>{view=a.dataset.v;map=null;render();if(view==='network')pollNet()};
 if(view==='chats'){for(const e of document.querySelectorAll('[data-c]'))e.onclick=async()=>{current=+e.dataset.c;lastMsgKey='';await loadChat();render();await refresh()};
  const qi=$('#q');if(qi){if(q!==undefined)qi.value=q;qi.oninput=()=>{const pos=qi.selectionStart;render();const n=$('#q');n.focus();n.setSelectionRange(pos,pos)}}
  const t=$('#txt');if(t){t.value=keep;const cnt=()=>$('#cnt').textContent=new TextEncoder().encode(t.value).length+' / '+t.maxLength;cnt();t.oninput=()=>{cnt();mentionCheck()};t.onkeydown=k=>{if(mentionKey(k))return;if(k.key==='Enter')send()};$('#send').onclick=send;if(document.activeElement!==$('#q'))t.focus()}
  for(const a of document.querySelectorAll('[data-reply]'))a.onclick=()=>insertPrefix('@['+messages[+a.dataset.reply].sender+'] ');
  for(const a of document.querySelectorAll('[data-info]'))a.onclick=()=>messageInfo(messages[+a.dataset.info]);
  const ms=$('#msgs');if(ms)ms.scrollTop=ms.scrollHeight;$('#newch').onclick=newChannel;if(mention.open)mentionDraw()}
 if(view==='network'&&!map)drawMap();
 if(view==='settings'){$('#follow').onclick=()=>{localStorage.mmTheme='follow';applyTheme();render()};$('#logout').onclick=async()=>{try{await api('logout',{})}catch(e){}token='';sessionStorage.mmTok='';login()}}}
// Reply as on the T-Deck: "@[name] " in front of the draft.
const bytes=s=>new TextEncoder().encode(s).length;
function insertPrefix(prefix){const t=$('#txt');if(!t)return;if(t.value.startsWith(prefix)){t.focus();return}const c=state.chats.find(x=>x.id===current);if(bytes(prefix+t.value)>c.limit){toast('Draft full: not enough space');return}t.value=prefix+t.value;t.dispatchEvent(new Event('input'));t.focus()}
async function messageInfo(m){let j;try{j=await api('info?chat='+current+'&token='+m.token)}catch(e){toast(e.message);return}
 const d=document.createElement('div');d.className='dim';const kv=(k,v)=>'<div class="kv"><span>'+k+'</span><b>'+esc(v)+'</b></div>';
 const rows=l=>l.length?'<div class="route">'+l.map((x,i)=>'<span><b>'+(i+1)+'.</b> ['+esc(x.hash)+'] '+esc(x.name||'unknown')+'</span>').join('')+'</div>':'';
 let h='<div class="modal"><h3>'+(j.mine?'Your message':esc(j.sender))+'</h3><div class="card" style="margin-top:12px">'+kv('Time',j.date||m.time)+
  (j.mine?kv('Status',j.status)+(j.direct?'':kv('Repeaters heard',j.repeaters<0?'--':j.repeaters)):kv('Hops',j.hops<0?(j.direct?'direct':'?'):j.hops)+(j.direct?'':kv('Region scope',j.scope))+kv('Route',j.route))+'</div>';
 if(!j.mine&&j.route==='flood')h+='<div class="lbl" style="margin-top:12px">PATH RX</div>'+(rows(j.path)||'<div style="color:var(--muted)">Direct reception: 0 repeaters.</div>')+'<div style="color:var(--muted);font-size:13px;margin-top:4px">Destination: this T-Deck</div>';
 if(j.mine&&!j.direct)h+='<div class="lbl" style="margin-top:12px">REPEATERS HEARD</div>'+(rows(j.heard)||'<div style="color:var(--muted)">'+(j.repeaters>0?'Names not recorded':'No repeater echoes recorded')+'</div>')+'<div style="color:var(--muted);font-size:13px;margin-top:4px">Echoes, not a delivery ACK.</div>';
 h+='<div class="lbl" style="margin-top:12px">MESSAGE</div><div class="card" style="white-space:pre-wrap">'+mentions(j.body)+'</div><div style="display:flex;gap:10px;justify-content:flex-end;margin-top:14px">'+
  (j.mine?'':'<button class="btn" id="ir">Reply</button>')+'<button class="btn acc" id="ic">Close</button></div></div>';
 d.innerHTML=h;d.onclick=e=>{if(e.target===d)d.remove()};document.body.appendChild(d);d.querySelector('#ic').onclick=()=>d.remove();
 const r=d.querySelector('#ir');if(r)r.onclick=()=>{d.remove();insertPrefix('@['+m.sender+'] ')}}
// @ in the composer: a list of names (recent writers of this chat, then contacts); Enter / click inserts "@[name] ".
let mention={open:false,q:'',start:0,names:[],sel:0,chat:-1,list:[]};
async function mentionCheck(){const t=$('#txt');const pos=t.selectionStart,before=t.value.slice(0,pos),m=before.match(/(^|\s)@([^\s@\[\]]{0,24})$/);
 if(!m){mentionClose();return}mention.q=m[2].toLowerCase();mention.start=pos-m[2].length-1;
 if(mention.chat!==current||!mention.names.length){try{mention.names=(await api('mention?chat='+current)).names;mention.chat=current}catch(e){return}}
 mention.open=true;mention.sel=0;mentionDraw()}
function mentionDraw(){let p=$('.mpop');const t=$('#txt');if(!t){mentionClose();return}const list=mention.names.filter(n=>n.toLowerCase().includes(mention.q)).slice(0,40);mention.list=list;
 if(!list.length){if(p)p.remove();return}if(!p){p=document.createElement('div');p.className='mpop';t.closest('.composer').appendChild(p)}
 p.innerHTML='<small>MENTION</small>'+list.map((n,i)=>'<div data-m="'+i+'" class="'+(i===mention.sel?'on':'')+'">'+esc(n)+'</div>').join('');
 for(const e of p.querySelectorAll('[data-m]'))e.onmousedown=ev=>{ev.preventDefault();mentionPick(+e.dataset.m)}}
function mentionPick(i){const t=$('#txt');const name=mention.list[i];if(!name)return;const pos=t.selectionStart;const tag='@['+name+'] ';t.value=t.value.slice(0,mention.start)+tag+t.value.slice(pos);const at=mention.start+tag.length;mentionClose();t.focus();t.setSelectionRange(at,at);t.dispatchEvent(new Event('input'))}
function mentionKey(k){if(!mention.open||!mention.list.length)return false;if(k.key==='ArrowDown'||k.key==='ArrowUp'){mention.sel=(mention.sel+(k.key==='ArrowDown'?1:mention.list.length-1))%mention.list.length;mentionDraw();k.preventDefault();return true}
 if(k.key==='Enter'||k.key==='Tab'){mentionPick(mention.sel);k.preventDefault();return true}if(k.key==='Escape'){mentionClose();return true}return false}
function mentionClose(){mention.open=false;const p=$('.mpop');if(p)p.remove()}
async function send(){const t=$('#txt');const text=t.value.trim();if(!text)return;try{await api('send',{chat:current,text});t.value='';await loadChat();await refresh();render()}catch(e){toast(e.message)}}
function newChannel(){const d=document.createElement('div');d.className='dim';const forms=['','Create private channel','Join private channel','Join #hashtag'];
 const draw=async()=>{let key='';if(newForm===1){try{key=(await api('key')).key}catch(e){}}
  d.innerHTML='<div class="modal"><h3>New channel</h3><span style="color:var(--muted)">Saved on the T-Deck, ready on the device too.</span><div class="tabs">'+[1,2,3].map(i=>'<span class="tab'+(i===newForm?' on':'')+'" data-f="'+i+'">'+forms[i]+'</span>').join('')+'</div>'+
  '<div class="field"><small>'+(newForm===3?'HASHTAG (a-z, 0-9, -)':'CHANNEL NAME')+'</small><input id="fn"></div>'+(newForm===3?'':'<div class="field"><small>SECRET KEY (32 HEX)</small><input id="fk" value="'+key+'" '+(newForm===1?'readonly':'')+' style="font-family:monospace"></div>')+
  '<div class="field"><small>REGION SCOPE (OPTIONAL)</small><input id="fs" placeholder="e.g. it-lig"></div><div class="err" id="ferr"></div><div style="display:flex;gap:10px;justify-content:flex-end"><button class="btn" id="fc">Cancel</button><button class="btn acc" id="fo">'+(newForm===1?'Create channel':'Join')+'</button></div></div>';
  for(const t of d.querySelectorAll('[data-f]'))t.onclick=()=>{newForm=+t.dataset.f;draw()};d.querySelector('#fc').onclick=()=>d.remove();d.querySelector('#fn').focus();
  d.querySelector('#fo').onclick=async()=>{try{const r=await api('channel',{form:newForm,name:d.querySelector('#fn').value,key:(d.querySelector('#fk')||{}).value||'',scope:d.querySelector('#fs').value});d.remove();toast(r.message);await refresh();current=r.id;lastMsgKey='';await loadChat();render()}catch(e){d.querySelector('#ferr').textContent=e.message}}};
 document.body.appendChild(d);draw()}
function drawMap(){const el=$('#nodemap');if(!el||!window.L||!net)return;map=L.map(el).setView([45,9],5);L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:17,attribution:'© OpenStreetMap contributors'}).addTo(map);
 const pts=[];for(const n of net.nodes)if(n.lat||n.lon){pts.push([n.lat,n.lon]);/* dots, names on hover: 90 labels on one map overlap */
  const acc=getComputedStyle(document.body).getPropertyValue('--acc').trim(),fg=getComputedStyle(document.body).getPropertyValue('--fg').trim();
  L.circleMarker([n.lat,n.lon],{radius:n.type===2?6:5,weight:2,color:n.type===2?acc:fg,fillColor:n.type===2?acc:'#ffffff',fillOpacity:n.type===2?.85:.9}).bindTooltip((n.type===2?'📡 ':'')+esc(n.name)+(n.live?' · heard '+esc(n.age)+' ago':''),{direction:'top',offset:[0,-6]}).addTo(map)}
 if(net.self){pts.push([net.self.lat,net.self.lon]);L.marker([net.self.lat,net.self.lon],{icon:L.divIcon({className:'',html:'<div class="mmme"></div>',iconSize:[22,22],iconAnchor:[11,11]})}).addTo(map)}
 if(pts.length)map.fitBounds(pts,{padding:[20,20],maxZoom:13});setTimeout(()=>map&&map.invalidateSize(),200)}
let netTimer=0;async function pollNet(){clearTimeout(netTimer);if(view!=='network'||!token)return;try{net=await api('network');if(view==='network'&&!document.querySelector('.leaflet-container')){render()}else if(view==='network'){/* keep the live map element: only the cards are drawn again */const keep=$('#nodemap');keep.remove();const m=map;render();const fresh=$('#nodemap');if(fresh&&m){fresh.replaceWith(keep);map=m;map.invalidateSize()}}}catch(e){}netTimer=setTimeout(pollNet,5000)}
async function tick(){if(!token)return;try{await refresh();let changed=false;if(view==='chats')changed=await loadChat();if(view==='chats'&&!mention.open&&!$('.dim')&&(changed||!document.activeElement||document.activeElement.id!=='q'))render()}catch(e){}setTimeout(tick,3000)}
(async()=>{applyTheme();if(token){try{await refresh();await loadChat();render();tick();return}catch(e){}}login();const w=setInterval(()=>{if(token){clearInterval(w);tick()}},500)})();
</script></body></html>)HTML";
