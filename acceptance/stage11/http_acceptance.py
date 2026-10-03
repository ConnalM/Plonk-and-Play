"""HTTP-level Stage 11 authority/reconstruction probe.
Uses two independent cookie jars against the real Browser Interface."""
import json, sys, time
from urllib.request import Request, build_opener, HTTPCookieProcessor
from urllib.error import HTTPError
from http.cookiejar import CookieJar

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:9080"

def client():
    jar = CookieJar(); return build_opener(HTTPCookieProcessor(jar)), jar

def call(opener, path, method="GET", body=None):
    data = None if body is None else json.dumps(body).encode()
    req = Request(BASE + path, data=data, method=method, headers={"Content-Type":"application/json"})
    try:
        with opener.open(req, timeout=5) as r:
            raw = r.read().decode(); return r.status, (json.loads(raw) if raw else None)
    except HTTPError as e:
        raw = e.read().decode(); return e.code, (json.loads(raw) if raw else None)

a, jar_a = client(); b, jar_b = client()
status, context_a = call(a, "/context")
assert status == 200 and context_a["role"] == "Spectator" and not context_a["hasMaster"]
status, context_b = call(b, "/context")
assert status == 200 and context_b["role"] == "Spectator"
status, boot = call(a, "/bootstrap", "POST")
assert status == 200 and boot["bootstrap"] is True and len(list(jar_a)) == 1
assert len(list(jar_b)) == 1
assert call(b, "/context")[1]["role"] == "Spectator"
assert call(a, "/context")[1]["role"] == "Race Director"
assert call(b, "/bootstrap", "POST")[0] == 409
for path in ("/request/pause", "/request/honour-restart", "/request/grid-restart"):
    status, _ = call(b, path, "POST", {"correlationId": 77, "role":"Race Director", "clientContext":"RaceDirectorSmug"})
    assert status == 202
    time.sleep(.05)
    status, result = call(b, "/request-result?correlationId=77")
    assert status in (200, 204)
    if status == 200: assert result["result"] == "REJECTED"
print("HTTP Stage 11 authority/reconstruction probe PASS")
