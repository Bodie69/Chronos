#pragma once

// Static HTML/CSS for the web dashboard, served from flash. All dynamic data is fetched by
// the pages' own JavaScript from the /api/* endpoints in web_dashboard.cpp.

static const char kStyleCss[] PROGMEM = R"CSS(
:root{--bg:#0d1117;--card:#161b22;--line:#30363d;--text:#e6edf3;--muted:#8b949e;--accent:#2f81f7;--ok:#3fb950;--bad:#f85149}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
a{color:var(--accent)}
.wrap{max-width:960px;margin:0 auto;padding:16px}
.center{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:20px;margin-bottom:16px}
.brand{font-weight:800;letter-spacing:.2em;font-size:40px;margin:0;text-align:center}
.brand span{color:var(--accent)}
.sub{color:var(--muted);text-align:center;margin:4px 0 20px}
.clock{font-size:56px;font-weight:700;text-align:center;font-variant-numeric:tabular-nums;color:#3fb950}
.date{text-align:center;color:var(--muted);margin-bottom:16px}
.labs{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:16px 0}
.lab{border:1px solid var(--line);border-radius:10px;padding:12px;text-align:center;font-weight:700;color:#58a6ff}
.btn{display:inline-block;border:0;border-radius:8px;padding:10px 18px;background:var(--accent);color:#fff;font-weight:600;cursor:pointer;text-decoration:none;font-size:15px}
.btn.sec{background:#30363d}
.btn.sm{padding:6px 12px;font-size:13px}
.btn.block{display:block;width:100%;text-align:center}
input,select{background:#0d1117;color:var(--text);border:1px solid var(--line);border-radius:8px;padding:8px 10px;font-size:15px}
label{display:block;color:var(--muted);font-size:13px;margin:12px 0 4px}
.full{width:100%}
.err{color:var(--bad);text-align:center;margin-top:10px}
.msg{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#238636;color:#fff;padding:10px 18px;border-radius:8px;display:none}
.msg.bad{background:#da3633}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px}
header h1{margin:0;font-size:22px;letter-spacing:.15em}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.alarm{border:1px solid var(--line);border-radius:10px;padding:10px 12px;margin-bottom:8px;display:grid;grid-template-columns:34px 56px 110px 1fr;gap:8px;align-items:center}
.alarm.off{opacity:.6}
.alarm .num{color:var(--muted);font-weight:700}
.alarm .more{grid-column:1/-1;display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.days{display:flex;gap:3px}
.day{width:30px;height:30px;border-radius:6px;border:1px solid var(--line);background:#0d1117;color:var(--muted);font-size:12px;font-weight:700;cursor:pointer;padding:0}
.day.on{background:#1f6feb;color:#fff;border-color:#1f6feb}
.sw{position:relative;width:46px;height:26px;display:inline-block}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:#30363d;border-radius:26px;transition:.2s;cursor:pointer}
.sw span:before{content:"";position:absolute;width:20px;height:20px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.sw input:checked+span{background:var(--ok)}
.sw input:checked+span:before{transform:translateX(20px)}
.status{color:var(--muted);font-size:13px}
@media(max-width:600px){.alarm{grid-template-columns:30px 52px 1fr}.alarm .lbl{grid-column:1/-1}.clock{font-size:44px}}
.app{display:flex;min-height:100vh}
.side{width:236px;flex:0 0 236px;background:var(--card);border-right:1px solid var(--line);padding:16px 10px;display:flex;flex-direction:column;gap:2px;position:sticky;top:0;height:100vh;overflow-y:auto}
.logo{font-weight:800;letter-spacing:.2em;font-size:20px;padding:4px 10px 10px}
.logo span{color:var(--accent)}
.grp{color:var(--muted);font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.1em;padding:14px 10px 4px}
.nav{display:flex;align-items:center;gap:10px;padding:9px 10px;border-radius:8px;color:var(--text);text-decoration:none;font-weight:600}
.nav:hover{background:#21262d}
.nav.on{background:rgba(47,129,247,.16);color:#58a6ff}
.nav i{width:18px;text-align:center;font-style:normal}
.side .foot{margin-top:auto;padding-top:12px}
.main{flex:1;min-width:0;padding:16px 24px;max-width:1000px}
.menu{display:none;background:#21262d;border:1px solid var(--line);color:var(--text);border-radius:8px;width:40px;height:40px;font-size:20px;cursor:pointer}
.scrim{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:15}
.view{display:none}.view.on{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.stat{border:1px solid var(--line);border-radius:10px;padding:12px}
.stat small{display:block;color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em}
.stat b{display:block;font-size:22px;font-variant-numeric:tabular-nums}
.good{color:var(--ok)}.warn{color:#d29922}.bad{color:var(--bad)}
input[type=range]{width:100%;accent-color:var(--accent);padding:0}
.seg{display:inline-flex;border:1px solid var(--line);border-radius:8px;overflow:hidden}
.seg button{background:#0d1117;color:var(--text);border:0;padding:8px 16px;font-weight:600;cursor:pointer}
.seg button.on{background:var(--accent);color:#fff}
.note{color:var(--muted);font-size:13px;margin:8px 0 0}
@media(max-width:800px){.side{position:fixed;left:0;top:0;z-index:20;transform:translateX(-100%);transition:transform .2s}
.side.open{transform:none}.scrim.open{display:block}.menu{display:inline-block}.main{padding:12px 16px}}
)CSS";

static const char kWelcomeHtml[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Chronos</title><link rel="stylesheet" href="/style.css"></head>
<body><div class="center"><div style="width:100%;max-width:440px">
<div class="card">
<h1 class="brand">CHRON<span>O</span>S</h1>
<p class="sub">Dual Comlab Clock &amp; Bell System</p>
<div class="clock" id="clock">--:--</div>
<div class="date" id="date">&nbsp;</div>
<div class="labs"><div class="lab" id="lab0">COMLAB 1</div><div class="lab" id="lab1">COMLAB 2</div></div>
<a class="btn block" href="/login">Login to Dashboard</a>
<p class="status" id="st" style="text-align:center;margin:12px 0 0"></p>
</div></div></div>
<script>
async function tick(){try{const r=await fetch('/api/info');const j=await r.json();
clock.textContent=j.time;date.textContent=j.date;lab0.textContent=j.labs[0];lab1.textContent=j.labs[1];
st.textContent='Time source: '+(j.gps?'GPS (locked)':(j.rtc?'RTC':'not set'));}catch(e){}}
tick();setInterval(tick,1000);
</script></body></html>)HTML";

static const char kLoginHtml[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Chronos Login</title><link rel="stylesheet" href="/style.css"></head>
<body><div class="center"><div style="width:100%;max-width:380px">
<div class="card">
<h1 class="brand" style="font-size:30px">CHRON<span>O</span>S</h1>
<p class="sub">Sign in to manage alarms</p>
<form method="post" action="/login">
<label for="u">Username</label><input class="full" id="u" name="username" autocomplete="username" required>
<label for="p">Password</label><input class="full" id="p" name="password" type="password" autocomplete="current-password" required>
<div style="height:16px"></div>
<button class="btn block" type="submit">Login</button>
</form>
<div class="err" id="err"></div>
<p style="text-align:center;margin:14px 0 0"><a href="/">&larr; Back to welcome page</a></p>
</div></div></div>
<script>if(location.search.indexOf('e=1')>=0)err.textContent='Incorrect username or password';</script>
</body></html>)HTML";

static const char kDashboardHtml[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Chronos Dashboard</title><link rel="stylesheet" href="/style.css"></head>
<body><div class="app">
<nav class="side" id="side">
<div class="logo">CHRON<span>O</span>S</div>
<div class="grp">Alarms</div>
<a class="nav" href="#a0"><i>&#9200;</i><span><span id="nl0">Comlab 1</span> Alarms</span></a>
<a class="nav" href="#a1"><i>&#9200;</i><span><span id="nl1">Comlab 2</span> Alarms</span></a>
<div class="grp">System</div>
<a class="nav" href="#time"><i>&#128339;</i>Time &amp; Date</a>
<a class="nav" href="#diag"><i>&#128295;</i>Diagnostics</a>
<a class="nav" href="#settings"><i>&#9881;</i>Settings</a>
<div class="foot"><a class="nav" href="/logout"><i>&#8617;</i>Logout</a></div>
</nav>
<div class="scrim" id="scrim"></div>
<main class="main">
<header><div class="row"><button class="menu" id="menuBtn" aria-label="Menu">&#9776;</button><h1 id="title">Alarms</h1></div><span class="status" id="now"></span></header>

<section class="view" id="v-alarms"><div class="card"><div id="list"></div></div></section>

<section class="view" id="v-time">
<div class="card"><h3 style="margin-top:0">Timezone Offset</h3>
<div class="row"><select id="tz"></select><button class="btn sm" onclick="saveTz()">Save</button></div>
<p class="note">Applied to GPS/RTC time (both are UTC).</p></div>
<div class="card"><h3 style="margin-top:0">12h / 24h Format</h3>
<div class="seg"><button id="f12" onclick="saveFmt(1)">12-Hour</button><button id="f24" onclick="saveFmt(0)">24-Hour</button></div></div>
<div class="card"><h3 style="margin-top:0">Manual Set Time</h3>
<div class="row"><input type="date" id="md"><input type="time" id="mt"><button class="btn sm sec" onclick="useDevice()">Use this device's time</button><button class="btn sm" onclick="setTime()">Set</button></div>
<p class="note" id="mnote">Written to the RTC. While GPS is locked, GPS time takes over again.</p></div>
</section>

<section class="view" id="v-diag">
<div class="card"><h3 style="margin-top:0">Speakers</h3>
<div class="row"><button class="btn" onclick="beep(0)">Test Speaker 1 (<span id="bl0">Comlab 1</span>)</button><button class="btn" onclick="beep(1)">Test Speaker 2 (<span id="bl1">Comlab 2</span>)</button></div>
<label for="vo">Volume <span id="vv"></span></label><input type="range" id="vo" min="0" max="100" step="5">
<p class="note">Applies to both speakers, including while a tone is playing. The PAM8403 board's own volume knob (if it has one) sets the maximum.</p></div>
<div class="card"><h3 style="margin-top:0">GPS Signal Monitor</h3>
<div class="grid"><div class="stat"><small>Fix</small><b id="gFix">--</b></div><div class="stat"><small>Satellites</small><b id="gSats">--</b></div><div class="stat"><small>Signal (HDOP)</small><b id="gHdop">--</b></div><div class="stat"><small>RTC</small><b id="gRtc">--</b></div></div>
<p class="note">HDOP: lower is better (under 2 is good). NO DATA means the module isn't talking (wiring/power).</p></div>
<div class="card"><h3 style="margin-top:0">Brightness <span class="status" id="bv"></span></h3>
<input type="range" id="br" min="1" max="255"></div>
<div class="card"><h3 style="margin-top:0">System</h3>
<div class="grid"><div class="stat"><small>Uptime</small><b id="sUp">--</b></div><div class="stat"><small>Wi-Fi clients</small><b id="sCl">--</b></div><div class="stat"><small>Free heap</small><b id="sHeap">--</b></div><div class="stat"><small>Wi-Fi RAM (DMA)</small><b id="sDma">--</b></div></div></div>
</section>

<section class="view" id="v-settings">
<div class="card"><h3 style="margin-top:0">Lab Names</h3>
<p class="status" style="margin-top:0">Shown on each panel's footer. Max 10 characters (8 or fewer keeps clear of the status dots).</p>
<div class="row" style="margin-bottom:8px"><input id="n0" maxlength="10" style="flex:1;min-width:140px"><button class="btn sm" onclick="saveName(0)">Save</button></div>
<div class="row"><input id="n1" maxlength="10" style="flex:1;min-width:140px"><button class="btn sm" onclick="saveName(1)">Save</button></div></div>
<div class="card"><h3 style="margin-top:0">Change Password</h3>
<div class="row"><input id="pc" type="password" placeholder="Current password" style="flex:1;min-width:140px">
<input id="pn" type="password" placeholder="New password (4+ chars)" style="flex:1;min-width:140px">
<button class="btn sm" onclick="changePw()">Update</button></div></div>
</section>
</main></div>
<div class="msg" id="msg"></div>
<script>
const DAYS=['S','M','T','W','T','F','S'];
const CHIMES=['Westminster','High-Freq Bell','Standard Pulse','Double Chime'];
const DURS=[[0,'Until dismissed'],[5,'5 s'],[10,'10 s'],[15,'15 s'],[30,'30 s']];
let S=null,D=null,tab=0,view='',timeFilled=false,brDrag=false,voDrag=false;
const $=id=>document.getElementById(id);
function toast(t,bad){msg.textContent=t;msg.className='msg'+(bad?' bad':'');msg.style.display='block';clearTimeout(toast.t);toast.t=setTimeout(()=>msg.style.display='none',2200)}
async function post(url,data){const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)});
if(r.status==401){location='/login';throw 0}const t=await r.text();if(!r.ok){toast(t||'Error',1);throw 0}return t}
const p2=n=>String(n).padStart(2,'0');
function esc(s){return s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function tabName(i){return S.labs[i]}

// ---- navigation (sidebar mirrors the panel's button menu) ----
function side(open){$('side').classList.toggle('open',open);scrim.classList.toggle('open',open)}
menuBtn.onclick=()=>side(true);scrim.onclick=()=>side(false);
function route(){const h=location.hash.slice(1)||'a0';const m=/^a([01])$/.exec(h);
view=m?'alarms':(['time','diag','settings'].includes(h)?h:'alarms');if(m)tab=+m[1];
document.querySelectorAll('.view').forEach(v=>v.classList.toggle('on',v.id=='v-'+view));
document.querySelectorAll('.nav').forEach(a=>a.classList.toggle('on',a.getAttribute('href')=='#'+(view=='alarms'?'a'+tab:view)));
title.textContent=view=='alarms'?(S?tabName(tab):'')+' Alarms':{time:'Time & Date',diag:'Diagnostics',settings:'Settings'}[view];
if(view=='alarms'&&S)renderList();if(view=='time')timeFilled=false;side(false);poll()}
addEventListener('hashchange',route);

// ---- alarms ----
async function load(){const r=await fetch('/api/state');if(r.status==401){location='/login';return}S=await r.json();
n0.value=S.labs[0];n1.value=S.labs[1];labels();route()}
function labels(){for(const i of [0,1]){$('nl'+i).textContent=S.labs[i];$('bl'+i).textContent=S.labs[i]}}
function renderList(){list.innerHTML='';S.alarms[tab].forEach((a,i)=>{const d=document.createElement('div');d.className='alarm'+(a.en?'':' off');
d.innerHTML=`<span class="num">#${i+1}</span><label class="sw" style="margin:0"><input type="checkbox" class="en" ${a.en?'checked':''}><span></span></label>
<input type="time" class="tm" value="${p2(a.h)}:${p2(a.m)}"><input class="lbl" maxlength="15" value="${esc(a.l)}" placeholder="Label">
<div class="more"><div class="days">${DAYS.map((x,b)=>`<button type="button" class="day${a.d>>b&1?' on':''}" data-b="${b}">${x}</button>`).join('')}</div>
<select class="dur">${DURS.map(o=>`<option value="${o[0]}"${o[0]==a.dur?' selected':''}>${o[1]}</option>`).join('')}</select>
<select class="ch">${CHIMES.map((c,k)=>`<option value="${k}"${k==a.c?' selected':''}>${c}</option>`).join('')}</select>
<button class="btn sm sv">Save</button></div>`;
d.querySelectorAll('.day').forEach(b=>b.onclick=()=>b.classList.toggle('on'));
d.querySelector('.en').onchange=e=>d.classList.toggle('off',!e.target.checked);
d.querySelector('.sv').onclick=()=>saveAlarm(i,d);list.appendChild(d)})}
async function saveAlarm(i,d){const t=d.querySelector('.tm').value.split(':');let mask=0;d.querySelectorAll('.day.on').forEach(b=>mask|=1<<b.dataset.b);
const en=d.querySelector('.en').checked;if(en&&!mask){toast('Pick at least one day',1);return}
const a={en:en?1:0,h:+t[0],m:+t[1],d:mask,dur:+d.querySelector('.dur').value,c:+d.querySelector('.ch').value,l:d.querySelector('.lbl').value.trim()||'ALARM'};
try{await post('/api/alarm',{t:tab,s:i+1,...a});S.alarms[tab][i]=a;toast(tabName(tab)+' alarm #'+(i+1)+' saved')}catch(e){}}

// ---- time & date ----
const tzName=m=>'UTC'+(m<0?'-':'+')+p2(Math.floor(Math.abs(m)/60))+':'+p2(Math.abs(m)%60);
function fillTz(cur){const opts=[];for(let m=-720;m<=840;m+=30)opts.push(m);if(!opts.includes(cur)){opts.push(cur);opts.sort((a,b)=>a-b)}
tz.innerHTML=opts.map(m=>`<option value="${m}"${m==cur?' selected':''}>${tzName(m)}</option>`).join('')}
async function saveTz(){try{await post('/api/timecfg',{tz:tz.value});toast('Timezone saved');timeFilled=false;poll()}catch(e){}}
async function saveFmt(h12){try{await post('/api/timecfg',{h12});D.h12=!!h12;fmtBtns();toast((h12?'12':'24')+'-hour format saved')}catch(e){}}
function fmtBtns(){f12.classList.toggle('on',D.h12);f24.classList.toggle('on',!D.h12)}
function useDevice(){const n=new Date();md.value=n.getFullYear()+'-'+p2(n.getMonth()+1)+'-'+p2(n.getDate());mt.value=p2(n.getHours())+':'+p2(n.getMinutes())}
async function setTime(){if(!md.value||!mt.value){toast('Pick a date and time',1);return}const d=md.value.split('-'),t=mt.value.split(':');
try{await post('/api/settime',{y:+d[0],mo:+d[1],d:+d[2],h:+t[0],mi:+t[1]});toast('Time set');timeFilled=false;poll()}catch(e){}}

// ---- diagnostics ----
async function beep(i){try{await post('/api/beep',{lab:i});toast('Speaker '+(i+1)+' playing')}catch(e){}}
vo.oninput=()=>{voDrag=true;vv.textContent=vo.value+'%';clearTimeout(vo.t);vo.t=setTimeout(sendVo,150)};
vo.onchange=()=>{clearTimeout(vo.t);sendVo();voDrag=false};
async function sendVo(){try{await post('/api/volume',{v:vo.value})}catch(e){}}
br.oninput=()=>{brDrag=true;bv.textContent=br.value;clearTimeout(br.t);br.t=setTimeout(sendBr,150)};
br.onchange=()=>{clearTimeout(br.t);sendBr();brDrag=false};
async function sendBr(){try{await post('/api/brightness',{b:br.value})}catch(e){}}
function up(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+'d ':'')+h+'h '+p2(m)+'m'}
const kb=n=>(n/1024).toFixed(1)+' KB';
function setTxt(id,t,c){const e=$(id);e.textContent=t;e.className=c||''}

// ---- live data, polled only while Diagnostics or Time & Date is open ----
async function poll(){clearTimeout(poll.t);if(view!='diag'&&view!='time')return;
try{const r=await fetch('/api/diag');if(r.status==401){location='/login';return}D=await r.json();
if(view=='diag'){setTxt('gFix',D.fix,D.fix=='LOCKED'?'good':(D.fix=='NO FIX'?'warn':'bad'));setTxt('gSats',D.sats);
setTxt('gHdop',D.hdop==null?'--':D.hdop.toFixed(1),D.hdop==null?'':(D.hdop<2?'good':(D.hdop<5?'warn':'bad')));
setTxt('gRtc',D.rtc?'OK':'MISSING',D.rtc?'good':'bad');if(!brDrag){br.value=D.bright;bv.textContent=D.bright}if(!voDrag){vo.value=D.vol;vv.textContent=D.vol+'%'}
setTxt('sUp',up(D.up));setTxt('sCl',D.clients);setTxt('sHeap',kb(D.heap));setTxt('sDma',kb(D.dma)+' (min '+kb(D.dmaMin)+')',D.dmaMin<4096?'warn':'')}
if(view=='time'){fmtBtns();if(!timeFilled){fillTz(D.tz);md.value=D.y+'-'+p2(D.mo)+'-'+p2(D.d);mt.value=p2(D.h)+':'+p2(D.mi);timeFilled=true}
mnote.textContent=D.rtc?'Written to the RTC. While GPS is locked, GPS time takes over again.':'No RTC detected, manual time cannot be stored.'}}catch(e){}
poll.t=setTimeout(poll,view=='diag'?1000:5000)}

// ---- settings ----
async function saveName(i){const v=$('n'+i).value.trim();if(!v){toast('Name cannot be empty',1);return}
try{await post('/api/labname',{lab:i,name:v});S.labs[i]=v;labels();route();toast('Lab name saved')}catch(e){}}
async function changePw(){try{await post('/api/password',{cur:pc.value,new:pn.value});pc.value=pn.value='';toast('Password updated')}catch(e){}}

async function clock(){try{const j=await (await fetch('/api/info')).json();now.textContent=j.date+'  '+j.time}catch(e){}}
load();clock();setInterval(clock,1000);
</script></body></html>)HTML";
