from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
source = root / "browser" / "normal_browser.html"
target = root / "include" / "pp" / "normal_browser_page.inc"
html = source.read_text(encoding="utf-8")
if ')PPHTML"' in html:
    raise RuntimeError("normal_browser.html contains the raw-string terminator")
target.write_text('static const char kNormalBrowserHtml[] = R"PPHTML(' + html + ')PPHTML";\n', encoding="utf-8")
print("Embedded normal Browser:", source)
