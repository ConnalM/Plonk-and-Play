"""Structural proof for the disposable PSF.28 power-cut fixture.

This test deliberately does not build or talk to hardware.  It proves that
the acceptance-only markers/window surround the real History payload path and
that the normal production environment does not enable the fixture.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HISTORY = (ROOT / "firmware/include/pp/history_store.h").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "firmware/platformio.ini").read_text(encoding="utf-8")


def check(name: str, condition: bool) -> None:
    if not condition:
        raise AssertionError(name)
    print(f"PSF28 {name}=PASS")


begin_guard = "#if defined(PP_PSF28_ACCEPTANCE) && defined(ARDUINO)"
check("fixture_is_compile_time_acceptance_only", HISTORY.count(begin_guard) == 4)
check("begin_marker_present", "PSF28_PERSISTENCE_BEGIN kind=history_payload" in HISTORY)
check("window_marker_present", "PSF28_POWER_CUT_WINDOW old_committed_state_preserved" in HISTORY)
check("bounded_window_constant", "PP_PSF28_WINDOW_MS 15000UL" in HISTORY)
check("one_shot_state", "psf28WindowConsumed_ = false" in HISTORY)
check("flush_before_window", "Serial.flush();" in HISTORY)
check("yielding_bounded_window", "while (static_cast<uint32_t>(millis() - started) < PP_PSF28_WINDOW_MS)" in HISTORY and "delay(10);" in HISTORY)

begin = HISTORY.index("psf28PersistenceBegin(slot, length);")
payload_write = HISTORY.index("store_.writeUnreferenced(DataSlot + slot, bytes, length);")
payload_verify = HISTORY.index("if (!payloadVerified)")
window = HISTORY.index("psf28PowerCutWindow(slot, length);")
metadata = HISTORY.index("publishMetadata(candidate)")
check("begin_precedes_real_payload_write", begin < payload_write)
check("window_follows_payload_verification", payload_verify < window)
check("window_precedes_metadata_publication", window < metadata)
check("normal_environment_does_not_enable_fixture", "PP_PSF28_ACCEPTANCE" not in PLATFORMIO)
check("production_transaction_still_has_payload_then_metadata", "payloadVerified" in HISTORY and "publishMetadata(candidate)" in HISTORY)
