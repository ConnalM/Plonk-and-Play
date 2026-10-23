from pathlib import Path
import argparse
import hashlib
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(description='Verify final image embeds current generated normal Browser')
ap.add_argument('--binary', type=Path, required=True)
ap.add_argument('--page', type=Path, default=ROOT / 'firmware/include/pp/normal_browser_page.inc')
a = ap.parse_args()

if not a.binary.is_file():
    raise SystemExit(f'firmware image missing: {a.binary}')
if not a.page.is_file():
    raise SystemExit(f'generated Browser include missing: {a.page}')
inc = a.page.read_text(encoding='utf-8')
m = re.search(r'R"PPHTML\(([\s\S]*)\)PPHTML";', inc)
if not m:
    raise SystemExit('generated Browser payload marker missing')
payload = m.group(1).encode('utf-8')
binary = a.binary.read_bytes()
if payload not in binary:
    raise SystemExit('FAIL: final image does not contain the current generated normal Browser payload')
text = payload.decode('utf-8')
required = {
    'active-pause-only': "S.lifecycle==='RACING'&&!S.finishDisplay?.active" in text and 'id="pause"' in text and 'id="endRace"' not in text,
    'paused-competitive-actions': "S.lifecycle==='PAUSED'" in text and 'id="resume"' in text and 'id="restart"' in text and 'id="abandon"' in text,
    'paused-practice-end': 'id="endPractice"' in text and "practice?'<button" in text,
    'resume-duration-guard': "phase==='RESUME_COUNTDOWN'" in text and 'S.remainingDuration' in text,
    'lap-results-columns': 'LANE/RACER' in text and 'PROGRESS' in text and '<div>TIME</div>' in text and '<div>GAP</div>' in text,
    'lap-results-authoritative-time': "e.finishDurationUs?sec(e.finishDurationUs):" in text and 'e.completed?sec(e.completionTime)' not in text,
    'lap-results-stopped-progress-time': "finishDurationUs?sec(finishDurationUs)" not in text or "e.finishDurationUs?sec(e.finishDurationUs):" in text,
    'practice-summary-actions': 'id="again">PRACTICE AGAIN' in text and 'id="homePractice">HOME' in text and '/request/practice-again' in text,
}
failed = [k for k, ok in required.items() if not ok]
if failed:
    raise SystemExit('FAIL: current embedded normal Browser is missing: ' + ', '.join(failed))
offset = binary.find(payload)
print(f'Browser binary provenance PASS: {a.binary}')
print(f'payload_bytes={len(payload)} offset=0x{offset:x} image_bytes={len(binary)} sha256={hashlib.sha256(binary).hexdigest().upper()}')
print('markers=' + ','.join(required))
