"""Check that Browser HTTP handlers keep large working buffers off the httpd stack."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
source = (root / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
persistence = (root / "firmware/include/pp/persistent_storage.h").read_text(encoding="utf-8")


def region(start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


checks = {}
stack = re.search(r"config\.stack_size\s*=\s*(\d+)", source)
checks["httpd_stack_configured"] = bool(stack and int(stack.group(1)) >= 8192)
checks["state_buffer_heap"] = "malloc(StateJsonCapacity)" in region("static esp_err_t state", "static esp_err_t resultsRoute") and "free(json)" in region("static esp_err_t state", "static esp_err_t resultsRoute")
results = region("static esp_err_t resultsRoute", "static esp_err_t detailsRoute")
checks["results_result_and_json_heap"] = "malloc(sizeof(RaceEngineModule::CompletedRaceResult))" in results and "malloc(ResultsJsonCapacity)" in results and "free(loaded)" in results and "free(json)" in results
details = region("static esp_err_t detailsRoute", "static esp_err_t historyRoute")
checks["details_result_and_chunk_heap"] = "malloc(sizeof(RaceEngineModule::CompletedRaceResult))" in details and "malloc(320)" in details and "free(loaded)" in details and "free(chunk)" in details
history = region("static esp_err_t historyRoute", "static esp_err_t recordsRoute")
checks["history_result_and_json_heap"] = "malloc(sizeof(RaceEngineModule::CompletedRaceResult))" in history and "HistoryJsonCapacity" in history and "free(stored)" in history and "free(json)" in history
records = region("static esp_err_t recordsRoute", "static esp_err_t notice")
checks["records_buffer_heap"] = "malloc(RecordsJsonCapacity)" in records and "free(json)" in records
checks["idle_state_route_has_no_large_result_local"] = "CompletedRaceResult loaded{}" not in region("static esp_err_t state", "static esp_err_t resultsRoute")
load_newest = persistence[persistence.index("bool loadNewest"):persistence.index("uint8_t count()", persistence.index("bool loadNewest"))]
checks["persistent_decode_destination_heap"] = "malloc(sizeof(RaceEngineModule::CompletedRaceResult))" in load_newest and "CompletedRaceResult decoded{}" not in load_newest

failed = [name for name, passed in checks.items() if not passed]
if failed:
    print("Stage 14A HTTP stack review FAIL:", ", ".join(failed))
    sys.exit(1)
print("Stage 14A HTTP stack review PASS:", ", ".join(checks))
