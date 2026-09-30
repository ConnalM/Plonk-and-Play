# 1.13 structural inspection

Project: Plonk & Play. Repository: ConnalM/Plonk-and-Play.
Frozen test/source baseline: ac6c73bce1fbfad374eee7409b974663647ce65a.
Build identity is in build-manifest.json; exact modified source is retained under
source/ and hashed in source-manifest.json. The baseline build and hashes are
retained separately in baseline-build.txt and baseline-build-hashes.txt.

Baseline result: FAIL for shared System Time service. The baseline's time wrapper
was private to main.cpp's anonymous namespace. This was an implementation-boundary
defect, not an ambiguity in the accepted requirement. The corrected public service
uses exactly the same ESP32 monotonic clock and boot-scoped microsecond semantics.

Review basis: frozen test 1.13; Design Constitution sections 2, 4, 5 and 7;
System Architecture sections 7, 13, 13.1, 13.4, 13.5 and 14; System Lifecycle
sections 3, 4, 8 and 10; Message Contract sections 1-6; Stage 1 scope in First
Implementation Behaviour 16.2 and Development Workflow's Stage 1 handoff.

Detailed final findings and source references are in structural-review.json.
The second translation unit is a build-time and runtime witness that the time
service can be used outside diagnostics/main. Configuration observations capture
the actual RAM value after the real lifecycle/Memory bus exchange. Persistence
tests use the same pp-stage1 NVS namespace in disposable simulation and reset
the ESP32; they do not substitute a fake Memory implementation.

Serial acceptance observations are test instrumentation, not a new product
interface or authoritative P&P State. The acceptance build's quiet phase disables
production diagnostics before boot and uses the independent observer to inspect
the outcome. The production build contains no acceptance commands or fixtures.
No spec contradiction or product decision was identified for this Stage 1 scope.
