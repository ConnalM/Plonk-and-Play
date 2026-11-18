from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
browser = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")

match = re.search(r"config\.max_open_sockets\s*=\s*(\d+)\s*;", browser)
assert match, "Browser HTTP socket limit must be explicit"
requested = int(match.group(1))
assert requested == 7, f"expected the bounded production request of 7, got {requested}"

for name in ("sdkconfig", "sdkconfig.xip"):
    sdkconfig = ROOT / "firmware/wp7_idf_arduino_dbuf" / name
    text = sdkconfig.read_text(encoding="utf-8")
    sockets = re.search(r"^CONFIG_LWIP_MAX_SOCKETS=(\d+)\s*$", text, re.M)
    assert sockets, f"{name} must define CONFIG_LWIP_MAX_SOCKETS"
    compiled = int(sockets.group(1))
    available = compiled - 3
    assert requested <= available, (
        f"Browser requests {requested} sockets but {name} permits only "
        f"{available} after the three internal HTTP-server sockets"
    )

print("WP8_HTTP_SOCKET_LIMIT PASS")
