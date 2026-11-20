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
            out.append(("aux", "stage results" if i == 5 else "event results", aux_fits(), page.locator(".aux h2").inner_text()))
            page.locator("[data-nav=stage]").click()
            if i == 5:
                page.locator("[data-action=nextStage]").click()
    browser.close()

print(json.dumps(out, indent=2))
failed = [row for row in out if not row[2]]
print(f"TOTAL {len(out)} PASS {len(out)-len(failed)} FAIL {len(failed)}")
raise SystemExit(bool(failed))
