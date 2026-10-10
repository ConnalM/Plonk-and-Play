from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html_source = root / "browser" / "normal_browser.html"
page_inc = root / "include" / "pp" / "normal_browser_page.inc"
html = html_source.read_text(encoding="utf-8")
if ')PPHTML"' in html:
    raise RuntimeError("normal_browser.html contains the raw-string terminator")

# Presentation-only integration corrections. normal_browser.html remains the
# source page; these substitutions adapt it to the current authoritative P&P
# readback without introducing Browser sporting/configuration authority.
def replace_once(old, new, label):
    global html
    if html.count(old) != 1:
        raise RuntimeError(f"normal Browser integration anchor {label} expected once, found {html.count(old)}")
    html = html.replace(old, new, 1)

replace_once(
    "<b>Racer ${e.mugId||e.index+1}</b>",
    "<b>Racer ${e.mugId??'—'}</b>",
    "proposal racer identity")
replace_once(
    "<option value=\"0\">All stop immediately</option>",
    "<option value=\"0\">All stop when winner finishes</option>",
    "lap finish wording")
replace_once(
    "g=`<i class=\"lamp green ${phase==='GO'?'on':''}\"></i>`,label=S.finishDisplay?.active?'FINISHED':phase==='RESUME_COUNTDOWN'?'RESUMING':phase==='COUNTDOWN'?'STARTING':phase==='GO'?'GO':S.lifecycle",
    "g=`<i class=\"lamp green ${p.active&&phase==='GO'?'on':''}\"></i>`,label=S.finishDisplay?.active?'FINISHED':phase==='RESUME_COUNTDOWN'?'RESUMING':phase==='COUNTDOWN'?'STARTING':p.active&&phase==='GO'?'GO':S.lifecycle",
    "GO lamp lifetime")
replace_once(
    "function gap(e){return +e.lapsBehind?`+${e.lapsBehind} L`:'—'}",
    "function gap(e){if(e.gapKind==='TIME')return`+${(+e.gapTimeUs/1e6).toFixed(2)} s`;if(e.gapKind==='LAPS')return`+${+e.gapLaps} L`;return'—'}",
    "authoritative GAP formatter")
replace_once(
    "Racer ${e.raceEntryId}",
    "Racer ${e.mugId??'—'}",
    "practice active racer identity")
replace_once(
    "Racer ${e.raceEntryId}",
    "Racer ${e.mugId??'—'}",
    "race active racer identity")
replace_once(
    "<b>Racer ${e.raceEntryId}</b>",
    "<b>Racer ${e.mugId??'—'}</b>",
    "results racer identity")
replace_once(
    "<b>Racer ${e.raceEntryId}</b>",
    "<b>Racer ${e.mugId??'—'}</b>",
    "practice summary racer identity")
replace_once(
    "$('#details').onclick=details;$('#history').onclick=history",
    "$('#details').onclick=()=>{V='details';render()};$('#history').onclick=()=>{V='history';render()}",
    "completed navigation selection")
replace_once(
    "async function details(){let d=await G('/details');$('#extra').innerHTML='<h2>Details</h2>'+(d.entries||[]).map(e=>`<p><b>Lane ${e.lane}</b> · ${(e.records||[]).map(x=>'Lap '+x.lapNumber+' '+sec(x.lapTime)).join(' · ')||'No valid laps'}</p>`).join('')}async function history(){let h=await G('/history');$('#extra').innerHTML='<h2>History</h2>'+(h.entries||[]).map(x=>`<p>Race ${x.sequence} · ${name(x.mode)} · ${x.mode==='ENDURANCE'?x.durationMinutes+' min':x.lapTarget+' laps'}</p>`).join('')}",
    "async function details(){let d=await G('/details');A.innerHTML=`<section class=\"hero\"><div class=\"eye\">DETAILS</div><h1>Completed Session</h1></section><section class=\"card\">${(d.entries||[]).map(e=>`<p><b>Racer ${e.mugId??'—'} · Lane ${e.lane}</b> · ${(e.records||[]).map(x=>'Lap '+x.lapNumber+' '+sec(x.lapTime)).join(' · ')||'No valid laps'}</p>`).join('')}<div class=\"actions\"><button class=\"btn\" id=\"backResults\">RESULTS</button><button class=\"btn\" id=\"history\">HISTORY</button></div></section>`;$('#backResults').onclick=()=>{V='results';render()};$('#history').onclick=()=>{V='history';render()}}async function history(){let h=await G('/history');A.innerHTML=`<section class=\"hero\"><div class=\"eye\">HISTORY</div><h1>Completed Sessions</h1></section><section class=\"card\">${(h.entries||[]).map(x=>`<p><b>Race ${x.sequence} · ${name(x.mode)}</b> · ${x.mode==='ENDURANCE'?x.durationMinutes+' min':x.lapTarget+' laps'}${(x.entries||[]).length?' · '+x.entries.map(e=>'Racer '+(e.mugId??'—')+' / Lane '+e.lane).join(' · '):''}</p>`).join('')}<div class=\"actions\"><button class=\"btn\" id=\"backResults\">RESULTS</button></div></section>`;$('#backResults').onclick=()=>{V='results';render()}}",
    "details and history presentation")
replace_once(
    "async function req(path,extra={},setup=false){if(busy)return;busy=true;render();let id=corr++",
    "async function req(path,extra={},setup=false){if(busy)return;busy=true;if(setup){document.querySelectorAll('#fields input,#fields select,.modes button').forEach(x=>x.disabled=true);note('Updating race setup…')}else render();let id=corr++",
    "setup pending presentation")
replace_once(
    "if(V==='practice')return practice();if(V==='results')return results();if(V==='setup')return setup();home()",
    "if(V==='practice')return practice();if(V==='details')return details();if(V==='history')return history();if(V==='results')return results();if(V==='setup')return setup();home()",
    "legitimate completed view persistence")
replace_once(
    "if(S.practiceSummaryAvailable)V='practice';else if(S.lifecycle==='FINISHED'&&!S.finishDisplay?.active)V='results';render()",
    "if(S.practiceSummaryAvailable&&V!=='details'&&V!=='history')V='practice';else if(S.lifecycle==='FINISHED'&&!S.finishDisplay?.active&&!['details','history'].includes(V))V='results';render()",
    "poll navigation preservation")

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

# main.cpp deliberately keeps including "pp/browser_interface.h". GCC searches
# -iquote directories before normal -I directories for quoted includes, so the
# generated BrowserInterface is selected. Dependencies continue to resolve from
# the real project /include/pp tree; no P&P headers are copied or forked.
project_pp = root / "include" / "pp"
env.Append(CCFLAGS=["-iquote", str(generated_root)])
env.Prepend(CPPPATH=[str(generated_root), str(project_pp)])
print("Embedded normal Browser; compiler -iquote selects:", generated_interface)
