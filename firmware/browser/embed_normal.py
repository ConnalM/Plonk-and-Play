from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html_source = root / "browser" / "normal_browser.html"
page_inc = root / "include" / "pp" / "normal_browser_page.inc"
html = html_source.read_text(encoding="utf-8")
if ')PPHTML"' in html:
    raise RuntimeError("normal_browser.html contains the raw-string terminator")

def replace_once(old, new, label):
    global html
    if html.count(old) != 1:
        raise RuntimeError(f"normal Browser integration anchor {label} expected once, found {html.count(old)}")
    html = html.replace(old, new, 1)

def replace_in_function(start, end, old, new, expected, label):
    global html
    a = html.find(start)
    b = html.find(end, a + len(start))
    if a < 0 or b < 0:
        raise RuntimeError(f"normal Browser integration function boundary {label} not found")
    section = html[a:b]
    count = section.count(old)
    if count != expected:
        raise RuntimeError(f"normal Browser integration anchor {label} expected {expected}, found {count}")
    section = section.replace(old, new)
    html = html[:a] + section + html[b:]

# Existing approved normal-Browser authority/presentation corrections.
replace_once("<b>Racer ${e.mugId||e.index+1}</b>", "<b>Racer ${e.mugId??'—'}</b>", "proposal racer identity")
replace_once("<option value=\"0\">All stop immediately</option>", "<option value=\"0\">All stop when winner finishes</option>", "lap finish wording")
replace_once("function gap(e){return +e.lapsBehind?`+${e.lapsBehind} L`:'—'}", "function gap(e){if(e.gapKind==='TIME')return`+${(+e.gapTimeUs/1e6).toFixed(2)} s`;if(e.gapKind==='LAPS')return`+${+e.gapLaps} L`;return'—'}", "authoritative GAP formatter")
replace_in_function("function race(){", "function paused(){", "Racer ${e.raceEntryId}", "Racer ${e.mugId??'—'}", 2, "active race racer identities")
replace_in_function("function renderResults(r){", "async function results(){", "<b>Racer ${e.raceEntryId}</b>", "<b>Racer ${e.mugId??'—'}</b>", 1, "results racer identity")
replace_in_function("async function practice(){", "async function details(){", "<b>Racer ${e.raceEntryId}</b>", "<b>Racer ${e.mugId??'—'}</b>", 1, "practice summary racer identity")
replace_once("async function req(path,extra={},setup=false){if(busy)return;busy=true;render();let id=corr++", "async function req(path,extra={},setup=false){if(busy)return;busy=true;if(setup){document.querySelectorAll('#fields input,#fields select,.modes button').forEach(x=>x.disabled=true);note('Updating race setup…')}else render();let id=corr++", "setup pending presentation")
replace_once("if(V==='practice')return practice();if(V==='results')return results();if(V==='setup')return setup();home()", "if(V==='practice')return practice();if(V==='details')return details();if(V==='history')return history();if(V==='results')return results();if(V==='setup')return setup();home()", "legitimate completed view persistence")

# Normal-product Race Director UX. /context is authoritative for this Browser's
# role; /bootstrap is the existing legitimate request for an unowned Director.
replace_once("function home(){", "function canControl(){return C?.role==='Race Director'}async function makeDirector(){let r=await fetch('/bootstrap',{method:'POST'});C=await G('/context');if(!r.ok)note(C?.role==='Race Director'?'Race Director authority confirmed.':'Race Director is already assigned.');render()}function home(){", "Race Director helper")
replace_in_function("function header(){", "function canControl(){", "$('#status').textContent=P?.startable?'READY':'NOT READY';$('#reason').textContent=P?.startable?'':readyText()", "$('#status').textContent=P?.startable?(canControl()?'READY':'RACE DIRECTOR REQUIRED'):'NOT READY';$('#reason').textContent=P?.startable?'':readyText()", 1, "header Race Director status")
replace_in_function("function home(){", "function mb(", "<button class=\"btn primary big\" id=\"start\">${busy?'STARTING…':'START'}</button>", "${canControl()?`<button class=\"btn primary big\" id=\"start\">${busy?'STARTING…':'START'}</button>`:'<button class=\"btn primary big\" id=\"makeDirector\">MAKE RACE DIRECTOR</button>'}", 1, "home Race Director action")
replace_in_function("function home(){", "function mb(", "$('#start').disabled=busy||!C?.hasMaster||!P.startable;$('#start').onclick=()=>req('/request/start',{},true);", "if($('#start')){$('#start').disabled=busy||!P.startable;$('#start').onclick=()=>req('/request/start',{},true)}if($('#makeDirector'))$('#makeDirector').onclick=makeDirector;", 1, "home Race Director binding")
replace_in_function("function setup(){", "function fields(){", "<button class=\"btn primary\" id=\"start\">START</button>", "${canControl()?'<button class=\"btn primary\" id=\"start\">START</button>':'<button class=\"btn primary\" id=\"makeDirector\">MAKE RACE DIRECTOR</button>'}", 1, "setup Race Director action")
replace_in_function("function setup(){", "function fields(){", "$('#start').onclick=()=>req('/request/start',{},true);fields()", "if($('#start'))$('#start').onclick=()=>req('/request/start',{},true);if($('#makeDirector'))$('#makeDirector').onclick=makeDirector;fields()", 1, "setup Race Director binding")

# Developer drawer. S and P are reused from the normal Browser; additional
# diagnostics are requested only while the drawer is open.
dev_css = '.devtab{position:fixed;right:0;top:45%;z-index:40;border-radius:7px 0 0 7px;background:var(--y);color:#111;border:0;padding:10px 7px;font-weight:950;cursor:pointer;writing-mode:vertical-rl}.devdrawer{position:fixed;right:0;top:0;bottom:0;width:390px;max-width:95vw;background:#0b1522;border-left:1px solid var(--l);z-index:50;overflow:auto;padding:14px;box-shadow:-12px 0 28px #0008}.devhead{position:sticky;top:0;background:#0b1522;display:flex;align-items:center;gap:10px;padding:6px 0 12px;z-index:2}.devhead b{flex:1}.live{color:var(--y);font-size:12px;letter-spacing:.12em}.devsec{border-top:1px solid var(--l);padding:12px 0}.devsec h3{font-size:12px;letter-spacing:.12em;color:var(--m);margin:0 0 9px}.devkv{display:grid;grid-template-columns:1fr 1.25fr;gap:5px 10px;font-size:13px}.devkv span:nth-child(odd){color:var(--m)}.devlane{background:var(--p);border:1px solid var(--l);border-radius:8px;padding:9px;margin:7px 0;font-size:13px}.devlane b{color:var(--y)}.devfeedback{min-height:18px;color:var(--m);font-size:12px;margin-top:7px}.devmore{font-size:12px;color:var(--m)}body.devopen .s{margin-right:390px}@media(max-width:900px){body.devopen .s{margin-right:auto}.devdrawer{width:min(390px,95vw)}}'
replace_once("</style>", dev_css + "</style>", "Developer drawer CSS")
dev_html = '<button id="devTab" class="devtab">DEV</button><aside id="devDrawer" class="devdrawer hide" aria-label="Developer diagnostics"><div class="devhead"><b>DEVELOPER</b><span class="live">LIVE</span><button class="link" id="devClose">×</button></div><section class="devsec"><h3>P&amp;P STATE</h3><div id="devState" class="devkv"></div></section><section class="devsec"><h3>LANES</h3><div id="devLanes"></div></section><section class="devsec"><h3>TEST INPUTS</h3><div>SIMULATED PASSAGE</div><div class="actions"><button class="btn" id="devL1">PASS L1</button><button class="btn" id="devL2">PASS L2</button></div><div id="devFixture" class="devfeedback"></div></section><section class="devsec"><h3>EVENTS / HEALTH</h3><div id="devFact" class="devlane">Latest fact —</div><div id="devHealth" class="devkv"></div><details class="devmore"><summary>MORE</summary><div id="devHealthMore" class="devkv"></div></details></section></aside>'
replace_once("<script>", dev_html + "<script>", "Developer drawer markup")
dev_js = '''let devOpen=false,devFact=null,devNotice=null,devHealth=null,devHealthTick=0;function devVal(v){return v===undefined||v===null||v===''?'—':v}function devGap(e){if(e.gapKind==='TIME')return`+${(+e.gapTimeUs/1e6).toFixed(2)} s`;if(e.gapKind==='LAPS')return`+${+e.gapLaps} L`;return'—'}function devPairs(x){return x.map(([a,b])=>`<span>${a}</span><b>${devVal(b)}</b>`).join('')}function renderDev(){if(!devOpen)return;let sp=S?.startPresentation||{},fd=S?.finishDisplay||{};$('#devState').innerHTML=devPairs([['Lifecycle',S?.lifecycle],['Active mode',S?.sessionMode],['Proposed mode',P?.mode],['Proposal revision',P?.proposalRevision],['Startable',P?.startable?'YES':'NO'],['Readiness',P?.readinessContext?.code||P?.readiness],['Start phase',sp.phase],['Red lamps',sp.redLightsLit===undefined?'—':`${sp.redLightsLit} / ${sp.redLightCount}`],['Start active',sp.active?'YES':'NO'],['Remaining',S?.sessionMode==='ENDURANCE'?clock(S.remainingDuration):'—'],['Duration expired',S?.durationExpired?'YES':'NO'],['Overtime',S?.overtime?sec(S.overtime):'—'],['Finish Display',fd.active?'ACTIVE':'OFF'],['Result',S?.resultSealed?(S?.resultValid?'SEALED · VALID':'SEALED · INVALID'):'OPEN'],['Persistence',S?.persistenceFault?'FAULT':S?.persistencePending?'PENDING':'OK'],['History sequence',S?.historySequence||'—'],['Race Director',C?.role==='Race Director'?'THIS BROWSER':C?.hasMaster?'OTHER':'NONE']]);let entries=S?.lifecycle&&S.lifecycle!=='READY'?(S.entries||[]):[];$('#devLanes').innerHTML=entries.length?entries.map(e=>`<div class="devlane"><b>L${e.lane} · Racer ${e.mugId??'—'}</b><div class="devkv">${devPairs([['Laps',e.classifiedLaps],['Position',e.position||'—'],['GAP',devGap(e)],['Last',sec(e.lastLapTime)],['Best',sec(e.bestLapTime)],['Timing origin',e.waitingForTimingOrigin?'WAITING':e.hasLap?'SET':'—']])}</div></div>`).join(''):(P?.entries||[]).map(e=>`<div class="devlane"><b>L${e.lane} · Racer ${e.mugId??'—'}</b><div class="devkv">${devPairs([['Input device',e.inputDevice],['Capability',e.inputCapability]])}</div></div>`).join('');if(devFact)$('#devFact').textContent='Latest fact · '+(devFact.type||'NONE')+(devFact.type==='LAP_COMPLETED'?` · Entry ${devFact.raceEntryId} · Lap ${devFact.lapNumber} · ${sec(devFact.lapTime)}`:'');if(devHealth){$('#devHealth').innerHTML=devPairs([['Health',devHealth.ok===false?'FAULT':'OK'],['Wi-Fi',devHealth.wifiConnected===false?'Disconnected':'Connected'],['HTTP',devHealth.serverReady===false?'Stopped':'Running'],['Requests',`${devHealth.requestCount??'—'} · Errors ${devHealth.requestErrors??'—'}`],['Last',`${devHealth.lastRoute??'—'} · ${devHealth.lastDurationMs??'—'} ms`],['Storage',S?.persistenceFault?'FAULT':S?.persistencePending?'PENDING':'OK'],['Noticeboard',devNotice?.revision],['Proposal rev',devNotice?.proposalRevision]]);$('#devHealthMore').innerHTML=devPairs([['Reconnects',devHealth.reconnectAttempts],['Slow requests',devHealth.slowRequests],['Peak handlers',devHealth.peakHandlers],['Server starts',devHealth.serverStarts],['Server stops',devHealth.serverStops]])}}async function devPoll(){if(!devOpen)return;try{let [f,n]=await Promise.all([G('/fact'),G('/noticeboard')]);devFact=f;devNotice=n;if(++devHealthTick%2===1)devHealth=await G('/health');renderDev()}catch(e){if(devOpen)$('#devHealth').innerHTML='<span>Diagnostics</span><b>Unavailable</b>'}}function setDev(open){devOpen=open;document.body.classList.toggle('devopen',open);$('#devDrawer').classList.toggle('hide',!open);$('#devTab').classList.toggle('hide',open);if(open){renderDev();devPoll()}}async function devPass(lane){let r=await fetch('/fixture',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'lane'+lane})});let text='';try{text=await r.text()}catch(e){}$('#devFixture').textContent=r.ok?`L${lane} passage accepted`:r.status===404?'Test inputs unavailable in this build':`Rejected · ${text||r.status}`;if(devOpen)renderDev()}$('#devTab').onclick=()=>setDev(true);$('#devClose').onclick=()=>setDev(false);$('#devL1').onclick=()=>devPass(1);$('#devL2').onclick=()=>devPass(2);setInterval(devPoll,1000);'''
replace_once("</script>", dev_js + "</script>", "Developer drawer script")

checks = {"authoritative racer identity":"Racer ${e.mugId??'—'}","authoritative GAP kinds":"if(e.gapKind==='TIME')","authoritative GO green state":"p.green?'green on'","Details navigation":"V='details';render()","History navigation":"V='history';render()","duration pending presentation":"Updating race setup…","frozen lap finish wording":"All stop when winner finishes","Race Director request":"fetch('/bootstrap',{method:'POST'})","Race Director ownership":"C?.role==='Race Director'","Developer drawer":"id=\"devDrawer\"","fixture passage":"fetch('/fixture'","closed Developer polling guard":"if(!devOpen)return","Developer state reuse":"let sp=S?.startPresentation||{}"}
for label, marker in checks.items():
    if marker not in html:
        raise RuntimeError(f"normal Browser generated-page verification failed: {label}")
if "Racer ${e.raceEntryId}" in html or "<b>Racer ${e.raceEntryId}</b>" in html:
    raise RuntimeError("normal Browser generated-page verification failed: raceEntryId still presented as Racer")
if "devPass(lane){let r=await fetch('/fixture'" not in html:
    raise RuntimeError("normal Browser generated-page verification failed: Developer passage does not use fixture boundary")
page_inc.write_text('static const char kNormalBrowserHtml[] = R"PPHTML(' + html + ')PPHTML";\n', encoding="utf-8")

# Preserve the checked-in Development Browser. Build a generated copy of
# BrowserInterface with one additional presentation-only /normal route.
source = root / "include" / "pp" / "browser_interface.h"
generated_root = root / ".pio" / "generated_normal_browser"
generated_pp = generated_root / "pp"
generated_pp.mkdir(parents=True, exist_ok=True)
text = source.read_text(encoding="utf-8")
text = text.replace('#include "system_time.h"', '#include "system_time.h"\n#include "normal_browser_page.h"', 1)
needle = '  static esp_err_t health(httpd_req_t* request) {'
route = '''  static esp_err_t normalPage(httpd_req_t* request) {\n    RequestTrace trace(instance(), "/normal");\n    client(request);\n    httpd_resp_set_type(request, "text/html; charset=utf-8");\n    httpd_resp_set_hdr(request, "Cache-Control", "no-store");\n    const esp_err_t sent = httpd_resp_send(request, kNormalBrowserHtml, HTTPD_RESP_USE_STRLEN);\n    trace.complete(sent);\n    return sent;\n  }\n'''
if needle not in text:
    raise RuntimeError("BrowserInterface health route anchor not found")
text = text.replace(needle, route + needle, 1)
text = text.replace('config.max_uri_handlers = 48;', 'config.max_uri_handlers = 49;', 1)
anchor = '{"/", HTTP_GET, page, nullptr},'
if anchor not in text:
    raise RuntimeError("BrowserInterface root route anchor not found")
text = text.replace(anchor, anchor + '\n      {"/normal", HTTP_GET, normalPage, nullptr},', 1)
generated_interface = generated_pp / "browser_interface.h"
generated_interface.write_text(text, encoding="utf-8")
project_pp = root / "include" / "pp"
env.Append(CCFLAGS=["-iquote", str(generated_root)])
env.Prepend(CPPPATH=[str(generated_root), str(project_pp)])
print("Embedded normal Browser; compiler -iquote selects:", generated_interface)
