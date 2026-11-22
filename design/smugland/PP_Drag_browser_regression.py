"""Chromium regression for Gina's Drag visual prototype.

Run: python PP_Drag_browser_regression.py
Requires playwright and Chromium (set CHROMIUM_PATH if using system Chromium).
The HTML and this suite must be in the same directory. No firmware or real timing.
"""
import hashlib
import os
from pathlib import Path
from playwright.sync_api import sync_playwright

html_path = Path(__file__).with_name("PP_Drag_review.html")
raw = html_path.read_bytes()
html = raw.decode("utf-8")
print("Git blob:", hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest())
results = []

def check(scope, name, fn):
    try:
        value = bool(fn())
        results.append((scope, name, value))
    except Exception as exc:
        results.append((scope, name, False))
        print(f"ERROR {scope} {name}: {exc}")

def signal(page, action):
    page.select_option("#signal", action)
    page.locator("#apply").click()

def phase(page):
    return page.locator("#main h1").first.inner_text()

def fits(page):
    return page.evaluate("document.documentElement.scrollWidth <= innerWidth + 1")

def aux_fits(page):
    return page.locator(".aux").evaluate(
        "e => e.offsetWidth===800 && e.offsetHeight===480 && "
        "e.scrollWidth<=e.clientWidth && e.scrollHeight<=e.clientHeight"
    )

with sync_playwright() as pw:
    kw = {"headless": True, "args": ["--no-sandbox"]}
    if os.getenv("CHROMIUM_PATH"):
        kw["executable_path"] = os.environ["CHROMIUM_PATH"]
    browser = pw.chromium.launch(**kw)

    for width, height, label in [(1440,900,"desktop"),(768,1024,"tablet"),(390,844,"mobile")]:
        page = browser.new_page(viewport={"width":width,"height":height})
        errs = []
        page.on("pageerror",lambda e: errs.append(str(e)))
        page.set_content(html)
        page.select_option("#size", label)
        check(label,"setup",lambda: phase(page)=="Drag")
        check(label,"setup fits",lambda: fits(page))
        check(label,"Sportsman visible",lambda: page.locator("#treeType").input_value()=="Sportsman")
        check(label,"optional unknown course",lambda: page.locator("#course").input_value()=="")
        page.locator("[data-action=stage]").click()
        check(label,"waiting prompt",lambda: phase(page)=="MOVE FORWARD TO STAGE")
        signal(page,"block")
        check(label,"roll back and STAGE lamp",lambda: phase(page)=="ROLL BACK" and page.locator('[aria-label="STAGE lit"]').count()==1)
        signal(page,"clear")
        check(label,"clear ready",lambda: phase(page)=="READY")
        signal(page,"block")
        check(label,"pre-tree reblock not red",lambda: phase(page)=="ROLL BACK" and page.locator(".redHero").count()==0)
        signal(page,"clear")
        page.locator("[data-action=start]").click()
        signal(page,"amber")
        check(label,"first amber",lambda: page.locator('[aria-label="AMBER 1 lit"]').count()==1)
        signal(page,"amber")
        signal(page,"amber")
        signal(page,"go")
        check(label,"GO",lambda: phase(page)=="GO" and page.locator('[aria-label="GREEN lit"]').count()==1)
        signal(page,"cross")
        check(label,"reaction and ET start",lambda: "0.23 s" in page.locator("#main").inner_text() and "0.00 s" in page.locator("#main").inner_text())
        signal(page,"finish")
        check(label,"valid ET prominent",lambda: page.locator(".bigET").inner_text()=="6.84 s" and page.locator(".redHero").count()==0)
        check(label,"no fabricated speed",lambda: "AVG SPEED —" in page.locator("#main").inner_text())
        check(label,"REU unavailable",lambda: "TRAP SPEED — REU required" in page.locator("#main").inner_text())
        check(label,"result fits",lambda: fits(page))
        page.locator("[data-nav=aux]").click()
        check(label,"aux exact frame",lambda: aux_fits(page))
        check(label,"aux tree read-only",lambda: page.locator(".aux .treeLine").count()==7 and page.locator(".aux [data-action]").count()==0)
        check(label,"aux not faked PRE-STAGE",lambda: page.locator(".aux .treeLine.dormant").count()==1)
        page.locator("[data-nav=results]").click()
        check(label,"one fixture result",lambda: page.locator("tbody tr").count()==1)
        check(label,"results fit",lambda: fits(page))
        page.locator("[data-nav=setup]").click()
        check(label,"setup locked on finished",lambda: page.locator("#course").count()==0 and page.locator("[data-action=new]").count()==1)
        check(label,"no page exceptions",lambda: not errs)
        page.close()

    page = browser.new_page(viewport={"width":1440,"height":900})
    errs=[]
    page.on("pageerror",lambda e: errs.append(str(e)))
    page.set_content(html)
    page.select_option("#treeType","Pro")
    page.fill("#course","10")
    page.locator("#course").press("Tab")
    page.locator("[data-action=stage]").click()
    signal(page,"block")
    signal(page,"clear")
    page.locator("[data-action=start]").click()
    signal(page,"amber")
    check("Pro","three ambers",lambda: all(page.locator(f'[aria-label="AMBER {i} lit"]').count()==1 for i in (1,2,3)))
    signal(page,"cross")
    check("Pro","red before green",lambda: page.locator(".redHero").count()==1 and "-0.08 s" in page.locator("#main").inner_text())
    signal(page,"finish")
    check("Pro","red result with actual ET",lambda: page.locator(".redHero").count()==1 and page.locator(".bigET").inner_text()=="6.71 s")
    check("Pro","whole run speed, not trap speed",lambda: "AVG SPEED 3.3 mph" in page.locator("#main").inner_text() and "TRAP SPEED — REU required" in page.locator("#main").inner_text())
    page.locator("[data-nav=aux]").click()
    check("Pro","red auxiliary",lambda: aux_fits(page) and page.locator(".aux .redHero").inner_text()=="RED LIGHT")
    page.locator("[data-nav=run]").count() # navigation back via Race/setup and RETURN TO RESULT
    page.locator("[data-nav=setup]").click()
    page.locator("[data-action=return]").click()
    page.locator("[data-action=again]").click()
    check("Pro","race again resets and retains earlier history",lambda: phase(page)=="MOVE FORWARD TO STAGE")
    page.locator("[data-nav=results]").click()
    check("Pro","fixture history retained",lambda: page.locator("tbody tr").count()==1)
    check("Pro","no browser errors",lambda: not errs)
    page.close()

    page=browser.new_page(viewport={"width":390,"height":844})
    page.set_content(html)
    page.select_option("#role","mug")
    check("authority","view-only stage disabled",lambda: page.locator("[data-action=stage]").is_disabled())
    page.select_option("#role","director")
    page.locator("[data-action=stage]").click()
    signal(page,"block")
    signal(page,"clear")
    page.locator("[data-action=start]").click()
    signal(page,"amber");signal(page,"amber");signal(page,"amber");signal(page,"go");signal(page,"cross");signal(page,"finishMissing")
    check("unknown ET","unavailable result no speed",lambda: "ET unavailable" in page.locator("#main").inner_text() and "AVG SPEED —" in page.locator("#main").inner_text())
    browser.close()

failed=[row for row in results if not row[2]]
for scope,name,passed in results:
    print(f"{'PASS' if passed else 'FAIL'} | {scope} | {name}")
print(f"TOTAL {len(results)} PASS {len(results)-len(failed)} FAIL {len(failed)}")
raise SystemExit(bool(failed))
