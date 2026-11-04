from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
core = (root / 'firmware/include/pp/core.h').read_text(encoding='utf-8')
session = (root / 'firmware/include/pp/session_definition.h').read_text(encoding='utf-8')
race_control = (root / 'firmware/include/pp/race_control.h').read_text(encoding='utf-8')
main = (root / 'firmware/src/main.cpp').read_text(encoding='utf-8')
checks = {
    'provider boundary exists': 'class DetectorProvider' in core and 'DetectorProvider* provider_' in core,
    'provider injection is mandatory': 'defaultProvider' not in core and 'InputModule(Bus& bus,Bus::Endpoint endpoint):' not in core,
    'simulated provider retained': 'class SimulatedDetectorProvider final' in core and 'SimulatedDetector detectors_[MaxDetectors]' in core,
    'two source identities/statuses': 'detectorCount() const override' in core and 'DetectorStatus statuses_[MaxDetectors]' in core,
    'role-neutral bus path preserved': 'e.type=Type::InputEvent' in core and 'e.input=state.identity' in core and 'bus_.publish(endpoint,backlog_[head_])' in core,
    'production injects simulated provider explicitly': 'pp::SimulatedDetectorProvider simulatedDetectorProvider' in main and 'input(bus,bus.attach(pp::Role::Input),simulatedDetectorProvider)' in main,
    'session assignments drive required detector readiness': 'requiredDetectorCapabilitiesHealthy' in session and 'setup.entries[requiredIndex].startFinish.input' in session and 'status.healthy()' in session,
    'Race Control remains the readiness authority': 'requiredCapabilityAvailable_=false' in race_control and 'StartReadiness::CapabilityUnavailable' in race_control,
    'production refreshes readiness from provider each loop': 'refreshRequiredDetectorReadiness()' in main and 'raceControl.tick(systemTime())' in main and 'setRequiredCapabilityAvailable(true)' not in main,
}
for name, passed in checks.items():
    print(('PASS ' if passed else 'FAIL ') + name)
if not all(checks.values()):
    sys.exit(1)
print('WP7.1/WP7.2 detector-provider/readiness structural review PASS')
