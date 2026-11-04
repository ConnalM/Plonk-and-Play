from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
core = (root / 'firmware/include/pp/core.h').read_text(encoding='utf-8')
main = (root / 'firmware/src/main.cpp').read_text(encoding='utf-8')
checks = {
    'provider boundary exists': 'class DetectorProvider' in core and 'DetectorProvider* provider_' in core,
    'simulated provider retained': 'class SimulatedDetectorProvider final' in core and 'SimulatedDetector detectors_[MaxDetectors]' in core,
    'two source identities/statuses': 'detectorCount() const override' in core and 'DetectorStatus statuses_[MaxDetectors]' in core,
    'role-neutral bus path preserved': 'e.type=Type::InputEvent' in core and 'e.input=state.identity' in core and 'bus_.publish(endpoint,backlog_[head_])' in core,
    'production injects simulated provider explicitly': 'pp::SimulatedDetectorProvider simulatedDetectorProvider' in main and 'input(bus,bus.attach(pp::Role::Input),simulatedDetectorProvider)' in main,
}
for name, passed in checks.items():
    print(('PASS ' if passed else 'FAIL ') + name)
if not all(checks.values()):
    sys.exit(1)
print('WP7.1 detector-provider structural review PASS')
