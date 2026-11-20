"""Representative Chromium regression for the Timed Stage design fixture.
Run beside PP_Timed_Stage_review.html with Python Playwright installed.
Set CHROMIUM_PATH to use a system Chromium, otherwise Playwright's browser.
No network access, live controller, or production storage is required.
"""
from pathlib import Path
from playwright.sync_api import sync_playwright
import hashlib
import json
import os

html_path = Path(__file__).with_name("PP_Timed_Stage_review.html")
raw = html_path.read_bytes()
print("Git blob:", hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest())
html = raw.decode("utf-8")
out = []

with sync_playwright() as p:
    options = {"headless": True, "args": ["--no-sandbox"]}
    if os.environ.get("CHROMIUM_PATH"):
        options["executable_path"] = os.environ["CHROMIUM_PATH"]
    browser = p.chromium.launch(**options)
    for width, height, size in [(1440, 900, "desktop"), (768, 1024, "tablet"), (390, 844, "mobile")]:
        page = browser.new_page(viewport={"width": width, "height": height})
        errors = []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.set_content(html)
        page.select_option("#size", size)

        def check(name, fn):
            try:
                out.append((size, name, bool(fn())))
            except Exception as error:
                out.append((size, name, False, str(error)))

        def fit():
            return page.evaluate("document.documentElement.scrollWidth<=innerWidth+1")

        check("setup renders", lambda: page.locator("h1").inner_text() == "Timed Stage")
        check("setup fits viewport", fit)
        page.locator("summary").click()
        page.select_option("#attemptOrder", "racer")
        page.fill("#hold", "1")
        page.locator("#hold").press("Tab")
        check("advanced values retained", lambda: page.locator("#attemptOrder").input_value() == "racer" and page.locator("#hold").input_value() == "1")
        page.locator("[data-action=ready]").click()
        check("ready", lambda: page.locator(".status").inner_text() == "READY")
        check("ready fits", fit)
        page.locator("[data-action=countdown]").click()
        check("start sequence", lambda: page.locator(".lamp.red").count() == 3)
        page.select_option("#fixture", "armed")
        check("GO zero", lambda: page.locator(".clock").inner_text() == "00:00" and page.locator(".lamp.go").count() == 5)
        page.select_option("#fixture", "split")
        check("split precise", lambda: page.locator(".clock").inner_text() == "00:23.45")
        page.wait_for_timeout(1200)
        check("split expires", lambda: page.locator(".clock").inner_text() == "00:48")
        page.locator("[data-nav=setup]").click()
        check("setup locked", lambda: page.locator("h1").inner_text() == "Event in progress")
        page.locator("[data-action=returnRun]").click()
        check("return preserves run", lambda: page.locator(".clock").inner_text() == "00:48")
        page.select_option("#role", "mug")
        page.select_option("#fixture", "ready")
        check("role cannot start", lambda: page.locator("[data-action=countdown]").is_disabled())
        page.select_option("#role", "director")
        check("authority restored", lambda: page.locator("[data-action=countdown]").is_enabled())
        page.select_option("#fixture", "dnf")
        page.locator("[data-action=retry]").click()
        check("retry", lambda: page.locator(".status").inner_text() == "RETRY · READY")
        page.select_option("#fixture", "finish")
        page.locator("[data-action=review]").click()
        check("finish no retry", lambda: page.locator("[data-action=retry]").count() == 0)
        check("run review fits", fit)
        page.locator("[data-nav=aux]").click()
        check("aux 800x480", lambda: page.locator(".aux").evaluate("(e)=>e.offsetWidth===800&&e.offsetHeight===480"))
        check("aux no clipping", lambda: page.locator(".aux").evaluate("(e)=>e.scrollHeight<=e.clientHeight&&e.scrollWidth<=e.clientWidth"))
        check("aux page fits", fit)
        page.locator("[data-nav=stage]").click()
        check("results fits", fit)
        page.locator("[data-nav=matrix]").click()
        check("matrix fits", fit)
        check("details present", lambda: page.locator("details").count() == 1)
        check("no JS errors", lambda: not errors)
        page.close()

    page = browser.new_page(viewport={"width": 1440, "height": 900})
    page.set_content(html)
    page.locator("[data-action=ready]").click()
    page.locator("[data-nav=aux]").click()
    def aux_fits():
        return page.locator(".aux").evaluate("(e)=>e.clientWidth===800&&e.clientHeight===480&&e.scrollHeight<=480&&e.scrollWidth<=800")
    for state in ["ready", "countdown", "armed", "running", "split", "afterSplit", "finish", "false", "dnf", "retry"]:
        page.select_option("#fixture", state)
        out.append(("aux", state, aux_fits(), page.locator(".aux .status").inner_text()))
    for i in range(9):
        page.select_option("#fixture", "finish")
        page.locator("[data-nav=setup]").click()
        page.locator("[data-action=returnRun]").click()
        page.locator("[data-action=next]").click()
        if i in (5, 8):
            page.locator("[data-nav=aux]").click()
            expected = "Stage classification" if i == 5 else "Event results"
            out.append(("aux", expected, aux_fits() and page.locator(".aux h2").inner_text() == expected, page.locator(".aux h2").inner_text()))
            page.locator("[data-nav=stage]").click()
            if i == 5:
                page.locator("[data-action=nextStage]").click()

    # Targeted final sporting decisions, with real DOM interactions and action injection.
    page = browser.new_page(viewport={"width":390,"height":844})
    page.set_content(html)
    def check_target(name, value):
        out.append(("targeted", name, bool(value)))
    def action(a):
        page.locator(f"[data-action={a}]").click()
    def fixture(v):
        page.select_option("#fixture",v)
    def state():
        return page.evaluate("({cursor,loadedSlot,stageIndex,snapshot,falseCount,completed,active,route})")
    def inject(a):
        page.evaluate("a=>{let x=document.createElement('button');x.dataset.action=a;document.body.append(x);x.click();x.remove()}",a)
    page.locator("summary").click()
    page.select_option("#eventOrder","mug")
    check_target("MUG order disables attempt selector",page.locator("#attemptOrder").is_disabled())
    page.select_option("#eventOrder","stage")
    check_target("Stage order restores attempt selector",page.locator("#attemptOrder").is_enabled())
    page.select_option("#dnf","retry")
    check_target("Retry requires fallback field",page.locator("#fallback").is_visible())
    page.select_option("#fallback","slow")
    page.fill("#fixedDnf","150")
    page.locator("#fixedDnf").press("Tab")
    check_target("Fixed DNF fallback configured",page.locator("#fixedDnf").input_value()=="150")
    page.select_option("#falsePolicy","penalty")
    action("ready")
    action("countdown")
    fixture("false")
    check_target("Premature crossing holds timer zero",page.locator(".clock").inner_text()=="00:00")
    fixture("armed")
    check_target("Cannot skip confirm and restart",state()["snapshot"]=="false")
    action("restartCountdown")
    check_target("Confirmation returns countdown",state()["snapshot"]=="countdown")
    fixture("false")
    action("restartCountdown")
    fixture("armed")
    fixture("finish")
    action("review")
    check_target("Repeated penalty accumulation", "01:22.34" in page.locator("#main").inner_text() and "10.00 s" in page.locator("#main").inner_text())
    inject("retry")
    check_target("Forged retry rejected on valid finish",state()["snapshot"]=="finish")
    action("next")
    fixture("dnf")
    action("retry")
    fixture("dnf")
    action("retry")
    fixture("dnf")
    action("review")
    action("next")
    check_target("Repeated retries count one slot",len(state()["completed"])==2 and state()["completed"][1]["dnfRule"]=="slow")
    for i in range(4):
        fixture("finish")
        action("review")
        action("next")
    check_target("Stage boundary reached",state()["cursor"]==6 and state()["loadedSlot"]==5)
    inject("next")
    fixture("finish")
    inject("next")
    check_target("Duplicate acceptance blocked at boundary",len(state()["completed"])==6 and state()["cursor"]==6)
    page.locator("[data-nav=aux]").click()
    check_target("Stage classification heading",page.locator(".aux h2").inner_text()=="Stage classification")
    check_target("V21 auxiliary source bezel",page.locator(".bezel").evaluate("e=>{let s=getComputedStyle(e);return s.borderTopWidth==='5px'&&s.borderTopColor==='rgb(48, 52, 57)'&&s.borderTopLeftRadius==='9px'}") and page.locator(".aux").evaluate("e=>getComputedStyle(e).backgroundColor==='rgb(9, 21, 33)'"))
    page.locator("[data-nav=stage]").click()
    action("nextStage")
    page.locator("[data-nav=aux]").click()
    check_target("Next stage returns timing",page.locator(".aux .status").inner_text()=="READY")
    page.locator("[data-nav=setup]").click()
    action("returnRun")
    for i in range(3):
        fixture("finish")
        action("review")
        action("next")
    page.locator("[data-nav=aux]").click()
    check_target("Event classification heading",page.locator(".aux h2").inner_text()=="Event results")
    page.locator("[data-nav=stage]").click()
    check_target("Tie, best-run and provisional examples",all(x in page.locator("#main").inner_text() for x in ["Best Run: DNF","no direct comparison","Gap to leader","No completed comparator"]))
    page.reload()
    page.set_content(html)
    page.locator("summary").click()
    page.select_option("#falsePolicy","dnf")
    action("ready")
    action("countdown")
    fixture("false")
    check_target("DNF policy terminates immediately",state()["snapshot"]=="dnf")
    fixture("finish")
    check_target("DNF cannot become valid via stale fixture",state()["snapshot"]=="dnf")

    browser.close()

print(json.dumps(out, indent=2))
failed = [row for row in out if not row[2]]
print(f"TOTAL {len(out)} PASS {len(out)-len(failed)} FAIL {len(failed)}")
raise SystemExit(bool(failed))
