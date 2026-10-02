import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / 'evidence'
raw = (OUT / 'serial.txt').read_text(encoding='utf-8')
rows = {row['test']: row for row in (dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in re.findall(r'ACC S10 ([^\r\n]*)', raw)) if 'test' in row}
http = json.loads((OUT / 'http-results.json').read_text(encoding='utf-8'))
structural = json.loads((OUT / 'structural-review.json').read_text(encoding='utf-8'))
regressions = json.loads((OUT / 'regressions.json').read_text(encoding='utf-8'))

def yes(test, field):
 return rows.get(test, {}).get(field) == '1'

tests = {}
def check(name, ok, stimulus, expected, evidence):
 tests[name] = {'predetermined_stimulus': stimulus, 'expected_result': expected,
                'actual_result': {'serial': rows.get(name, {}), 'http': http.get(name, {})},
                'result': 'PASS' if ok else 'FAIL', 'raw_evidence': evidence}

check('10.1', yes('10.1','passive') and yes('10.1','denied') and http['10.1']['passive_no_master'], 'Three passive cookie clients and pre-bootstrap START', 'No Master and permission rejection', 'serial.txt; http-results.json')
check('10.2', yes('10.2','bootstrap') and yes('10.2','repeat') and http['10.2']['one_time_bootstrap'], 'Master cookie POST /bootstrap then repeats', 'Only first caller retains Master', 'serial.txt; http-results.json')
check('10.3', yes('10.3','claimed') and http['10.3']['claims_denied'], 'Spectator sends authority claims in JSON/header/query', 'Resolved context stays Spectator', 'serial.txt; http-results.json')
check('10.4', yes('10.4','submitted') and yes('10.4','accepted') and yes('10.4','definition') and int(rows['10.4']['go']) > 0 and http['10.4']['master_start'], 'Master POST /request/start', 'Race Control accepts after Session Definition commit', 'serial.txt; http-results.json')
check('10.5', yes('10.5','submitted') and yes('10.5','denied') and http['10.5']['spectator_denied'], 'Spectator START during active race', 'Private rejection with unchanged authoritative race', 'serial.txt; http-results.json')
check('10.6', yes('10.6','private') and http['10.6']['private_results'], 'Equal visible correlation IDs from two identities', 'Owner-private results', 'serial.txt; http-results.json')
check('10.7', yes('10.7','two_lane_current') and http['10.7']['separate_clients'], 'Join viewers at current two-lane state', 'Current State includes both entries', 'serial.txt; http-results.json')
check('10.8', yes('10.8','current_without_replay') and http['10.8']['state_reconstruction'], 'Join after Facts', 'Noticeboard current State sufficient', 'serial.txt; http-results.json')
check('10.9', yes('10.9','master_absent_input_progress'), 'Stop Master activity during Input crossings', 'Race continues with no promotion', 'serial.txt')
check('10.10', yes('10.10','pressure_isolated') and int(rows['10.10']['max_backlog']) <= 1, 'Do not consume presentation while Input/Race Engine progresses', 'Input and State remain correct', 'serial.txt')
check('10.11', yes('10.11','refresh_reopen') and http['10.11']['cookie_retained'], 'Reuse Master cookie after reconnect', 'Master restored without bootstrap', 'serial.txt; http-results.json')
check('10.12', yes('10.12','restart_retains') and http['10.12']['retained_authority'], 'Recreate Browser Interface with retained Master store', 'Viewer first remains Spectator', 'serial.txt; http-results.json')
check('10.13', yes('10.13','lost_identity_no_promotion') and http['10.13']['lost_identity_no_promotion'], 'Use fresh unmatched cookie after retained binding', 'No automatic assignment', 'serial.txt; http-results.json')
check('10.14', yes('10.14','reconnect_current') and http['10.14']['state_reconstruction'], 'Reconnect retained identities', 'Current Noticeboard State reconstructed', 'serial.txt; http-results.json')
check('10.15', yes('10.15','race_control_validity'), 'Master START after lifecycle change', 'Race Control rejects invalid lifecycle', 'serial.txt')
check('10.16', all(f['result'] == 'PASS' for f in structural['findings']), 'Static authority/boundary review', 'Browser interface boundary only', 'structural-review.json; structural-review.md')
check('10.17', regressions['overall'] == 'PASS', 'Run Stage 1–9 regression campaign', 'Only authorised narrow supersessions', 'regressions.json')
check('10.T', rows.get('10.T',{}).get('wrong') == 'FAIL' and rows.get('10.T',{}).get('restored') == 'PASS', 'Deliberate wrong expected result then restore', 'Evaluator detects failure then passes correct expectation', 'serial.txt; deliberate-failure.json')
(OUT / 'results.json').write_text(json.dumps({'tests': tests}, indent=2) + '\n', encoding='utf-8')
for name, test in tests.items(): print(name, test['result'])
sys.exit(0 if all(test['result'] == 'PASS' for test in tests.values()) else 1)
