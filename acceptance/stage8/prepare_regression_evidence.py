"""Retain current Stage 1-7 regression status without altering frozen tests."""
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / 'evidence'
OUT.mkdir(exist_ok=True)
SUPERSEDED = {
    ('stage1', '1.7'): {
        'classification': 'SUPERSEDED FOR STAGE 8 REGRESSION',
        'scope': 'Only the historical numeric assertion DiagnosticProbe == 2.',
        'reason': ('Stage 1 accepted DiagnosticProbe numeric type 2. The accepted Stage 7 '
                   'compatibility baseline assigns DiagnosticProbe=7 and freezes LapCompleted=4. '
                   'Stage 8 preserves every Stage 7 value and appends its new types. '
                   'DiagnosticProbe delivery, source and payload remain required.'),
    },
    ('stage6', '6.13'): {
        'classification': 'SUPERSEDED FOR STAGE 8 REGRESSION',
        'scope': 'Only the explicit prohibition of production START_REQUEST/acceptance.',
        'reason': 'Frozen Stage 8 explicitly requires Browser START_REQUEST through Race Control.',
    },
    ('stage6', '6.14'): {
        'classification': 'SUPERSEDED FOR STAGE 8 REGRESSION',
        'scope': 'Only the explicit no-Browser scope assertion.',
        'reason': 'Frozen Stage 8 explicitly requires Browser presentation to originate START_REQUEST; the Stage 6 lifecycle behaviour remains independently exercised.',
    },
}
stages = ('stage1', 'stage2', 'stage3', 'stage4', 'stage5', 'stage6', 'stage7')
report = {'generated_utc': datetime.now(timezone.utc).isoformat(),
          'source_base_commit': '20469948784a64662ef96cb20637cfd6a59cc27b',
          'policy': 'Earlier frozen tests are unchanged. Only the three user-authorised assertions below are superseded for Stage 8 regression.',
          'stages': {}}
failed = []
for stage in stages:
    path = ROOT / 'acceptance' / stage / 'evidence' / 'results.json'
    current = json.loads(path.read_text(encoding='utf-8'))
    retained = OUT / 'regression-runs' / stage
    retained.mkdir(parents=True, exist_ok=True)
    copied = []
    names = {'results.json', 'acceptance-serial.txt', 'serial.txt', 'production-serial.txt',
             'acceptance-cli.txt', 'production-cli.txt', 'identity.json', 'source-manifest.json',
             'build-manifest.json', 'deliberate-failure.json', 'structural-review.json', 'structural-review.md'}
    for source in path.parent.iterdir():
        if source.is_file() and source.name in names:
            destination = retained / source.name
            shutil.copyfile(source, destination)
            copied.append(destination.relative_to(ROOT).as_posix())
    entries = {}
    for test, result in current['tests'].items():
        extra = SUPERSEDED.get((stage, test))
        if extra:
            if result['result'] != 'FAIL':
                raise SystemExit(f'{stage} {test} expected frozen assertion to fail under Stage 8, got {result["result"]}')
            entries[test] = {**result, **extra, 'result': extra['classification']}
        else:
            entries[test] = result
            if result['result'] != 'PASS':
                failed.append(f'{stage} {test}={result["result"]}')
    report['stages'][stage] = {'frozen_results_path': path.relative_to(ROOT).as_posix(),
                               'retained_current_evidence': copied, 'tests': entries}
if failed:
    raise SystemExit('Unexpected regression failures: ' + ', '.join(failed))
(OUT / 'stage1-7-regression-results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
stage7 = report['stages']['stage7']
(OUT / 'stage7-regression.json').write_text(json.dumps({'result': 'PASS',
    'current_results': 'regression-runs/stage7/results.json',
    'tests': {name: test['result'] for name, test in stage7['tests'].items()}}, indent=2) + '\n', encoding='utf-8')
print('Stage 1-7 regression evidence retained: PASS with three authorised superseded assertions.')
