from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
control=(root/'firmware/include/pp/race_control.h').read_text(encoding='utf-8')
notice=(root/'firmware/include/pp/noticeboard.h').read_text(encoding='utf-8')
browser=(root/'firmware/include/pp/browser_interface.h').read_text(encoding='utf-8')
normal=(root/'firmware/browser/normal_browser.html').read_text(encoding='utf-8')
embed=(root/'firmware/browser/embed_normal.py').read_text(encoding='utf-8')
display=(root/'docs/COMMON_DISPLAY_SPEC.md').read_text(encoding='utf-8')
checks={
 'five_physical_positions': "n=5" in normal and "Array.from({length:n" in normal and "<i class=\"lamp green" not in normal,
 'authoritative_green_state': 'startLightsGreen' in control and 'startLightsGreen' in notice and 'green\\":%s' in browser and 'p.green?' in normal,
 'one_second_authority': 'goPresentationUntil_=go_+1000000ULL' in control and 'now<goPresentationUntil_' in control,
 'normal_end_race_control': 'id=\"endRace\"' in normal and "destruct('/request/end-race','/request/end-race/confirm','End race')" in normal,
 'history_sequence_boundary': 'loadSequence' in browser and 'historySequenceQuery' in browser and '?sequence=${selectedHistorySequence}' in normal,
 'history_selection_stable': 'historyItem' in normal and 'selectedHistorySequence' in normal and '&&!selectedHistorySequence' in normal,
 'finished_results_header': "$('#status').textContent='RESULTS'" in normal,
 'display_spec': 'separate sixth green lamp' in display.lower() and 'exactly one second' in display.lower(),
 'generated_page_guard': 'authoritative GO green state' in embed,
}
failed=[k for k,v in checks.items() if not v]
if failed:
 print('Stage 14C presentation corrections FAIL:',', '.join(failed));sys.exit(1)
print('Stage 14C presentation corrections PASS:',', '.join(checks))
