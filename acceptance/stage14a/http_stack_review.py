"""Check that Browser HTTP handlers keep large working buffers off the httpd stack."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
source = (root / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
persistence = (root / "firmware/include/pp/persistent_storage.h").read_text(encoding="utf-8")
history_store = (root / "firmware/include/pp/history_store.h").read_text(encoding="utf-8")


def region(start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


def heap_result(text: str) -> bool:
    return ("malloc(sizeof(RaceEngineModule::CompletedRaceResult))" in text or
            "calloc(1,sizeof(RaceEngineModule::CompletedRaceResult))" in text)

checks = {}
stack = re.search(r"config\.stack_size\s*=\s*(\d+)", source)
checks["httpd_stack_configured"] = bool(stack and int(stack.group(1)) >= 8192)
checks["state_buffer_heap"] = "malloc(StateJsonCapacity)" in region("static esp_err_t state", "static esp_err_t resultsRoute") and "free(json)" in region("static esp_err_t state", "static esp_err_t resultsRoute")
results = region("static esp_err_t resultsRoute", "static esp_err_t detailsRoute")
checks["results_result_and_json_heap"] = heap_result(results) and "malloc(ResultsJsonCapacity)" in results and "free(loaded)" in results and "free(json)" in results
checks["results_reaches_lazy_history_boundary"] = (
    "instance()->displayResult(*loaded,sequence,found)" in results and
    "history_->loadNewest(0,reinterpret_cast<uint8_t*>(&loaded)" in source
)
details = region("static esp_err_t detailsRoute", "static esp_err_t historyRoute")
checks["details_result_and_chunk_heap"] = heap_result(details) and "malloc(320)" in details and "free(loaded)" in details and "free(chunk)" in details
history = region("static esp_err_t historyRoute", "static esp_err_t recordsRoute")
checks["history_result_and_chunked_bounded"] = heap_result(history) and "char chunk[1536]" in history and "free(stored)" in history and "httpd_resp_send_chunk" in history
records = region("static esp_err_t recordsRoute", "static esp_err_t notice")
checks["records_buffer_heap"] = "malloc(RecordsJsonCapacity)" in records and "free(json)" in records
checks["idle_state_route_has_no_large_result_local"] = "CompletedRaceResult loaded{}" not in region("static esp_err_t state", "static esp_err_t resultsRoute")
load_newest = persistence[persistence.index("bool loadNewest"):persistence.index("uint8_t count()", persistence.index("bool loadNewest"))]
checks["persistent_decode_destination_heap"] = ("malloc(encodedCapacity)" in load_newest and
    "VersionedResultCodec::decode(encoded,encodedLength,*reinterpret_cast<RaceEngineModule::CompletedRaceResult*>(bytes))" in load_newest and
    "CompletedRaceResult decoded{}" not in load_newest)

# SlotHistoryStore::ensure() is reached lazily by Results/History requests.
# Its bounded metadata workspace must therefore be object-owned rather than an
# automatic HTTPD-task local.  Keep this check close to the HTTP stack checks so
# a future metadata-capacity change cannot silently recreate the S3 overflow.
ensure_start = history_store.index("bool ensure()")
ensure_end = history_store.index("uint8_t freeSlot()", ensure_start)
ensure = history_store[ensure_start:ensure_end]
checks["history_reconstruction_workspace_is_owned"] = (
    "ReconstructionWorkspace* reconstructionWorkspace_" in history_store and
    "new (std::nothrow) ReconstructionWorkspace()" in ensure and
    "uint8_t a[EncodedMetadataBytes]" not in ensure and
    "MetadataState va{}" not in ensure and
    "uint8_t legacy[LegacyMetadataBytes]" not in ensure
)
checks["history_operations_are_nonblocking_serialized"] = (
    "std::atomic_flag operationBusy_" in history_store and
    "test_and_set" in history_store and
    "OperationGuard guard(operationBusy_)" in history_store
)

failed = [name for name, passed in checks.items() if not passed]
if failed:
    print("Stage 14A HTTP stack review FAIL:", ", ".join(failed))
    sys.exit(1)
print("Stage 14A HTTP stack review PASS:", ", ".join(checks))

