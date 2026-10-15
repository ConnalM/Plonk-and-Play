from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html_source = root / "browser" / "normal_browser.html"
page_inc = root / "include" / "pp" / "normal_browser_page.inc"
html = html_source.read_text(encoding="utf-8")
# Older checked-in Browser copies contain a small set of double-encoded
# punctuation glyphs (for example ``â€¦``).  Repair those presentation bytes
# in the generation input so semantic transformations remain stable across
# source copies; this does not change Browser authority or behaviour.
html = (html
        .replace("\u00e2\u20ac\u00a6", "…")
        .replace("\u00e2\u20ac\u201d", "—")
        .replace("\u00e2\u20ac\u00ba", "›")
        .replace("\u00c2\u00b7", "·"))
if ')PPHTML"' in html:
    raise RuntimeError("normal_browser.html contains the raw-string terminator")

def replace_once(old, new, label):
    global html
    old_count = html.count(old)
    if old_count == 1:
        html = html.replace(old, new, 1)
        return
    # Some accepted presentation corrections are already present in the
    # checked-in source. Treat that as idempotent rather than attempting to
    # apply the same transformation twice.
    if html.count(new) == 1:
        return
    raise RuntimeError(f"normal Browser integration anchor {label} expected once or already applied, found old={old_count} new={html.count(new)}")

def replace_in_function(start, end, old, new, expected, label):
    global html
    a = html.find(start)
    b = html.find(end, a + len(start))
    if a < 0 or b < 0:
        raise RuntimeError(f"normal Browser integration function boundary {label} not found")
    section = html[a:b]
    count = section.count(old)
    if count != expected:
        if section.count(new) == expected:
            return
        raise RuntimeError(f"normal Browser integration anchor {label} expected {expected} or already applied, found old={count} new={section.count(new)}")
    section = section.replace(old, new)
    html = html[:a] + section + html[b:]

# Existing approved normal-Browser authority/presentation corrections.
replace_once("<b>Racer ${e.mugId||e.index+1}</b>", "<b>Racer ${e.mugId??'—'}</b>", "proposal racer identity")
if "All stop when winner finishes" not in html:
    replace_once("<option value=\"0\">All stop immediately</option>", "<option value=\"0\">All stop when winner finishes</option>", "lap finish wording")
if "if(e.gapKind==='TIME')" not in html:
    # The GAP formatter's fallback glyph has changed representation across
    # checked-in/generated Browser copies. Anchor the correction to the
    # function boundary instead of one byte-sensitive glyph spelling.
    gap_start = html.find("function gap(e){")
    gap_end = html.find("function race(){", gap_start + 1)
    if gap_start < 0 or gap_end < 0:
        raise RuntimeError("normal Browser integration GAP formatter boundary not found")
    gap_function = "function gap(e){if(e.gapKind==='TIME')return`+${(+e.gapTimeUs/1e6).toFixed(2)} s`;if(e.gapKind==='LAPS')return`+${+e.gapLaps} L`;return'—'}"
    html = html[:gap_start] + gap_function + html[gap_end:]
if "Racer ${e.raceEntryId}" in html:
    replace_in_function("function race(){", "function paused(){", "Racer ${e.raceEntryId}", "Racer ${e.mugId??'—'}", 2, "active race racer identities")
if "<b>Racer ${e.raceEntryId}</b>" in html[html.find("function renderResults(r){"):html.find("async function results(){")]:
    replace_in_function("function renderResults(r){", "async function results(){", "<b>Racer ${e.raceEntryId}</b>", "<b>Racer ${e.mugId??'—'}</b>", 1, "results racer identity")
if "<b>Racer ${e.raceEntryId}</b>" in html[html.find("async function practice(){"):html.find("async function details(){")]:
    replace_in_function("async function practice(){", "async function details(){", "<b>Racer ${e.raceEntryId}</b>", "<b>Racer ${e.mugId??'—'}</b>", 1, "practice summary racer identity")
if "async function req(path,extra={},setup=false){if(busy)return;busy=true;render();let id=corr++" in html:
    replace_once("async function req(path,extra={},setup=false){if(busy)return;busy=true;render();let id=corr++", "async function req(path,extra={},setup=false){if(busy)return;busy=true;if(setup){document.querySelectorAll('#fields input,#fields select,.modes button').forEach(x=>x.disabled=true);note('Updating race setup…')}else render();let id=corr++", "setup pending presentation")
# The source page now owns the complete view dispatch, including Results,
# Details, History and presentation-only navigation placeholders.

# Normal-product Race Director UX. /context is authoritative for this Browser's
# role; /bootstrap is the existing legitimate request for an unowned Director.
replace_once("function home(){", "function canControl(){return C?.isMaster}async function makeDirector(){await takeControl();render()}function home(){", "Race Director helper")
replace_in_function("function header(){", "function canControl(){", "$('#status').textContent=P?.startable?'READY':'NOT READY';$('#reason').textContent=P?.startable?'':readyText()", "$('#status').textContent=P?.startable?(canControl()?'READY':'RACE DIRECTOR REQUIRED'):'NOT READY';$('#reason').textContent=P?.startable?'':readyText()", 1, "header Race Director status")
replace_in_function("function home(){", "function mb(", "<button class=\"btn primary big\" id=\"start\">${busy?'STARTING…':'START'}</button>", "${canControl()?`<button class=\"btn primary big\" id=\"start\">${busy?'STARTING…':'START'}</button>`:'<button class=\"btn primary big\" id=\"makeDirector\">MAKE RACE DIRECTOR</button>'}", 1, "home Race Director action")
replace_in_function("function home(){", "function mb(", "$('#start').disabled=busy||setupRunning||!!setupDesired||!C?.isMaster||!P.startable;$('#start').onclick=()=>req('/request/start',{});", "if($('#start')){$('#start').disabled=busy||setupRunning||!!setupDesired||!P.startable;$('#start').onclick=()=>req('/request/start',{})}if($('#makeDirector'))$('#makeDirector').onclick=makeDirector;", 1, "home Race Director binding")
# Already integrated in normal_browser.html; no additional setup transform required.
# Already integrated in normal_browser.html; no additional setup transform required.

# Developer drawer. S and P are reused from the normal Browser; additional
# diagnostics are requested only while the drawer is open.
dev_css = '.devtab{position:fixed;right:0;top:45%;z-index:40;border-radius:7px 0 0 7px;background:var(--y);color:#111;border:0;padding:10px 7px;font-weight:950;cursor:pointer;writing-mode:vertical-rl}.devdrawer{position:fixed;right:0;top:0;bottom:0;width:390px;max-width:95vw;background:#0b1522;border-left:1px solid var(--l);z-index:50;overflow:auto;padding:14px;box-shadow:-12px 0 28px #0008}.devhead{position:sticky;top:0;background:#0b1522;display:flex;align-items:center;gap:10px;padding:6px 0 12px;z-index:2}.devhead b{flex:1}.live{color:var(--y);font-size:12px;letter-spacing:.12em}.devsec{border-top:1px solid var(--l);padding:12px 0}.devsec h3{font-size:12px;letter-spacing:.12em;color:var(--m);margin:0 0 9px}.devkv{display:grid;grid-template-columns:1fr 1.25fr;gap:5px 10px;font-size:13px}.devkv span:nth-child(odd){color:var(--m)}.devlane{background:var(--p);border:1px solid var(--l);border-radius:8px;padding:9px;margin:7px 0;font-size:13px}.devlane b{color:var(--y)}.devfeedback{min-height:18px;color:var(--m);font-size:12px;margin-top:7px}.devmore{font-size:12px;color:var(--m)}body.devopen .s{margin-right:390px}@media(max-width:900px){body.devopen .s{margin-right:auto}.devdrawer{width:min(390px,95vw)}}'
replace_once("</style>", dev_css + "</style>", "Developer drawer CSS")
dev_html = '<button id="devTab" class="devtab">DEV</button><aside id="devDrawer" class="devdrawer hide" aria-label="Developer diagnostics"><div class="devhead"><b>DEVELOPER</b><span class="live">LIVE</span><button class="link" id="devClose">×</button></div><section class="devsec"><h3>P&amp;P STATE</h3><div id="devState" class="devkv"></div></section><section class="devsec"><h3>LANES</h3><div id="devLanes"></div></section><section class="devsec"><h3>TEST INPUTS</h3><div>SIMULATED PASSAGE</div><div class="actions"><button class="btn" id="devL1">PASS L1</button><button class="btn" id="devL2">PASS L2</button></div><div id="devFixture" class="devfeedback"></div></section><section class="devsec"><h3>EVENTS / HEALTH</h3><div id="devFact" class="devlane">Latest fact —</div><div id="devHealth" class="devkv"></div><details class="devmore"><summary>MORE</summary><div id="devHealthMore" class="devkv"></div></details></section></aside>'
replace_once("<script>", dev_html + "<script>", "Developer drawer markup")
dev_js = '''let devOpen=false,devFact=null,devNotice=null,devHealth=null,devHealthTick=0;function devVal(v){return v===undefined||v===null||v===''?'—':v}function devGap(e){if(e.gapKind==='TIME')return`+${(+e.gapTimeUs/1e6).toFixed(2)} s`;if(e.gapKind==='LAPS')return`+${+e.gapLaps} L`;return'—'}function devPairs(x){return x.map(([a,b])=>`<span>${a}</span><b>${devVal(b)}</b>`).join('')}function renderDev(){if(!devOpen)return;let sp=S?.startPresentation||{},fd=S?.finishDisplay||{};$('#devState').innerHTML=devPairs([['Lifecycle',S?.lifecycle],['Active mode',S?.sessionMode],['Proposed mode',P?.mode],['Proposal revision',P?.proposalRevision],['Startable',P?.startable?'YES':'NO'],['Readiness',P?.readinessContext?.code||P?.readiness],['Start phase',sp.phase],['Red lamps',sp.redLightsLit===undefined?'—':`${sp.redLightsLit} / ${sp.redLightCount}`],['Start active',sp.active?'YES':'NO'],['Remaining',S?.sessionMode==='ENDURANCE'?clock(S.remainingDuration):'—'],['Duration expired',S?.durationExpired?'YES':'NO'],['Overtime',S?.overtime?sec(S.overtime):'—'],['Finish Display',fd.active?'ACTIVE':'OFF'],['Result',S?.resultSealed?(S?.resultValid?'SEALED · VALID':'SEALED · INVALID'):'OPEN'],['Persistence',S?.persistenceFault?'FAULT':S?.persistencePending?'PENDING':'OK'],['History sequence',S?.historySequence||'—'],['Race Director',C?.isMaster?'THIS BROWSER':C?.hasMaster?'OTHER':'NONE']]);let entries=S?.lifecycle&&S.lifecycle!=='READY'?(S.entries||[]):[];$('#devLanes').innerHTML=entries.length?entries.map(e=>`<div class="devlane"><b>L${e.lane} · Racer ${e.mugId??'—'}</b><div class="devkv">${devPairs([['Laps',e.classifiedLaps],['Position',e.position||'—'],['GAP',devGap(e)],['Last',sec(e.lastLapTime)],['Best',sec(e.bestLapTime)],['Timing origin',e.waitingForTimingOrigin?'WAITING':e.hasLap?'SET':'—']])}</div></div>`).join(''):(P?.entries||[]).map(e=>`<div class="devlane"><b>L${e.lane} · Racer ${e.mugId??'—'}</b><div class="devkv">${devPairs([['Input device',e.inputDevice],['Capability',e.inputCapability]])}</div></div>`).join('');if(devFact)$('#devFact').textContent='Latest fact · '+(devFact.type||'NONE')+(devFact.type==='LAP_COMPLETED'?` · Entry ${devFact.raceEntryId} · Lap ${devFact.lapNumber} · ${sec(devFact.lapTime)}`:'');if(devHealth){$('#devHealth').innerHTML=devPairs([['Health',devHealth.ok===false?'FAULT':'OK'],['Wi-Fi',devHealth.wifiConnected===false?'Disconnected':'Connected'],['HTTP',devHealth.serverReady===false?'Stopped':'Running'],['Requests',`${devHealth.requestCount??'—'} · Errors ${devHealth.requestErrors??'—'}`],['Last',`${devHealth.lastRoute??'—'} · ${devHealth.lastDurationMs??'—'} ms`],['Storage',S?.persistenceFault?'FAULT':S?.persistencePending?'PENDING':'OK'],['Noticeboard',devNotice?.revision],['Proposal rev',devNotice?.proposalRevision]]);$('#devHealthMore').innerHTML=devPairs([['Reconnects',devHealth.reconnectAttempts],['Slow requests',devHealth.slowRequests],['Peak handlers',devHealth.peakHandlers],['Server starts',devHealth.serverStarts],['Server stops',devHealth.serverStops]])}}async function devPoll(){if(!devOpen)return;try{let [f,n]=await Promise.all([G('/fact'),G('/noticeboard')]);devFact=f;devNotice=n;if(++devHealthTick%2===1)devHealth=await G('/health');renderDev()}catch(e){if(devOpen)$('#devHealth').innerHTML='<span>Diagnostics</span><b>Unavailable</b>'}}function setDev(open){devOpen=open;document.body.classList.toggle('devopen',open);$('#devDrawer').classList.toggle('hide',!open);$('#devTab').classList.toggle('hide',open);if(open){renderDev();devPoll()}}async function devPass(lane){let r=await fetch('/fixture',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'lane'+lane})});let text='';try{text=await r.text()}catch(e){}$('#devFixture').textContent=r.ok?`L${lane} passage accepted`:r.status===404?'Test inputs unavailable in this build':`Rejected · ${text||r.status}`;if(devOpen)renderDev()}$('#devTab').onclick=()=>setDev(true);$('#devClose').onclick=()=>setDev(false);$('#devL1').onclick=()=>devPass(1);$('#devL2').onclick=()=>devPass(2);setInterval(devPoll,1000);'''
replace_once("</script>", dev_js + "</script>", "Developer drawer script")

checks = {"authoritative racer identity":"Racer ${e.mugId??'—'}","authoritative GAP kinds":"if(e.gapKind==='TIME')","authoritative GO green state":"green?'green on'","Details navigation":"V='details';render()","History navigation":"V='history';render()","duration pending presentation":"Updating race setup…","frozen lap finish wording":"All stop when winner finishes","Race Director request":"fetch('/request/take-control'","Race Director ownership":"C?.isMaster","Developer drawer":"id=\"devDrawer\"","fixture passage":"fetch('/fixture'","closed Developer polling guard":"if(!devOpen)return","Developer state reuse":"let sp=S?.startPresentation||{}"}
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
