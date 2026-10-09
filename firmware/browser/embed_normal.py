from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html_source = root / "browser" / "normal_browser.html"
page_inc = root / "include" / "pp" / "normal_browser_page.inc"
html = html_source.read_text(encoding="utf-8")
if ')PPHTML"' in html:
    raise RuntimeError("normal_browser.html contains the raw-string terminator")
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
# -iquote directories before normal -I include directories for quoted includes,
# so this forces the generated integration header to be the dependency actually
# compiled without modifying/replacing the checked-in Development Browser.
# CPPPATH remains present so dependencies of the generated header resolve using
# the normal project include tree.
env.Append(CCFLAGS=["-iquote", str(generated_root)])
env.Prepend(CPPPATH=[str(generated_root)])
print("Embedded normal Browser; compiler -iquote selects:", generated_interface)
