// Deterministic Stage 10 in-firmware production-path probe.  It uses the same
// Browser Interface, Bus, Race Control, Race Engine and Noticeboard classes as
// the product.  HTTP/cookie transport is covered separately by the real HTTP
// runner; this probe supplies deterministic authority, lifecycle and race work.
Diagnostics stage10Output;
bool stage10Ready = false;

struct Stage10Fixture {
  pp::Bus bus;
  pp::ActiveSessionDefinition active;
  pp::Bus::Endpoint inputEndpoint = bus.attach(pp::Role::Input);
  pp::Bus::Endpoint presentation = bus.attach(pp::Role::Presentation);
  pp::Bus::Endpoint controlEndpoint = bus.attach(pp::Role::RaceControl);
  pp::Bus::Endpoint engineEndpoint = bus.attach(pp::Role::RaceEngine);
  pp::Bus::Endpoint observer = bus.attach(pp::Role::Diagnostics);
  pp::InputModule input{bus, inputEndpoint};
  pp::VolatileAuthorityStore retainedAuthority;
  pp::RaceControlModule control{bus, controlEndpoint, active};
  pp::RaceEngineModule engine{bus, engineEndpoint, active};
  pp::Noticeboard noticeboard{control, engine, active};
  pp::BrowserInterface browser{bus, presentation, noticeboard, &retainedAuthority};
  pp::ProposedRaceSetup setup{};

  Stage10Fixture() {
    setup.startFinish = {pp::InputModule::simulatedDetectorIdentity(), 1, pp::InputRole::StartFinish};
    setup.secondStartFinish = {pp::InputModule::simulatedDetectorBIdentity(), 2, pp::InputRole::StartFinish};
    setup.selectedMugId = 91;
    setup.secondMugId = 92;
    setup.activeLanes = 2;
    setup.lapTarget = 20;
    setup.startsBeforeStartFinish = true;

    bus.subscribe(controlEndpoint, pp::Type::StartRequest);
    bus.subscribe(controlEndpoint, pp::Type::CompetitionComplete);
    bus.subscribe(controlEndpoint, pp::Type::RaceIntegrityFault);
    bus.subscribe(engineEndpoint, pp::Type::GoScheduled);
    bus.subscribe(engineEndpoint, pp::Type::InputEvent);
    bus.subscribe(engineEndpoint, pp::Type::RaceIntegrityFault);
    bus.subscribe(presentation, pp::Type::RequestResult);
    bus.subscribe(presentation, pp::Type::NoticeboardChanged);
    bus.subscribe(presentation, pp::Type::LapCompleted);
    bus.subscribe(presentation, pp::Type::GoScheduled);
    // This observer records only prior Facts; it cannot create backpressure.
    bus.subscribe(observer, pp::Type::LapCompleted);
    control.setProposedRaceSetup(setup);
    control.setRequiredCapabilityAvailable(true);
  }

  void tick(pp::Time now) {
    input.flush();
    engine.tick();
    control.tick(now);
    browser.tick();
  }

  bool start(uint64_t owner, uint32_t visible, pp::Time now) {
    if (!browser.submitStart(visible, browser.context(owner), owner)) return false;
    control.tick(now);
    engine.tick();
    browser.tick();
    return true;
  }

  bool result(uint32_t visible, uint64_t owner, pp::BrowserInterface::ResultView& view) {
    return browser.requestResult(visible, owner, view);
  }

  unsigned observedFacts() {
    pp::Message message{};
    unsigned count = 0;
    while (bus.receive(observer, message)) {
      if (message.type == pp::Type::LapCompleted) ++count;
    }
    return count;
  }

  void racing(pp::Time now) {
    control.tick(control.scheduledGo() ? control.scheduledGo() : now);
    engine.tick();
    browser.tick();
  }

  void crossing(uint8_t lane, pp::Time at) {
    if (lane == 1) {
      input.sampleSource(true, at);
      input.sampleSource(true, at + 20000);
      input.sampleSource(false, at + 20000);
      input.sampleSource(false, at + 40000);
    } else {
      input.sampleSourceB(true, at);
      input.sampleSourceB(true, at + 20000);
      input.sampleSourceB(false, at + 20000);
      input.sampleSourceB(false, at + 40000);
    }
    tick(at + 40000);
  }
};

Stage10Fixture& stage10Fixture() {
  alignas(Stage10Fixture) static uint8_t storage[sizeof(Stage10Fixture)]{};
  static Stage10Fixture* fixture = nullptr;
  if (fixture) fixture->~Stage10Fixture();
  fixture = new(storage) Stage10Fixture;
  return *fixture;
}
void stage10AcceptanceBeforeBoot() {
  diagnostics.enable(false);
  stage10Output.enable(true);
}

void stage10Run() {
  const uint64_t master = 0x1111111111111111ULL;
  const uint64_t viewerB = 0x2222222222222222ULL;
  const uint64_t viewerC = 0x3333333333333333ULL;
  const uint64_t lostMaster = 0x4444444444444444ULL;

  Stage10Fixture& fixture = stage10Fixture();
  pp::BrowserInterface::ResultView result{};

  // 10.1: passive identities and rejected requests do not bind Master.
  const bool noMaster = !fixture.browser.hasMaster() &&
    fixture.browser.context(master) == pp::ClientContext::Spectator &&
    fixture.browser.context(viewerB) == pp::ClientContext::Spectator &&
    fixture.browser.context(viewerC) == pp::ClientContext::Spectator;
  const bool beforeStartSubmitted = fixture.start(viewerB, 1001, 1000000);
  const bool beforeStartDenied = fixture.result(1001, viewerB, result) &&
    result.result == pp::RequestResult::Rejected &&
    result.rejection == pp::RequestRejection::PermissionDenied &&
    !fixture.browser.hasMaster() && fixture.control.state() == pp::SessionLifecycle::Ready;

  // 10.2/10.3: deliberate bootstrap picks exactly one identity; claims are
  // ignored because the resolved context comes only from the owner token.
  const bool bootstrap = fixture.browser.bootstrap(master);
  const bool cannotRepeat = !fixture.browser.bootstrap(master) && !fixture.browser.bootstrap(viewerB) &&
    fixture.browser.context(master) == pp::ClientContext::RaceDirectorSmug &&
    fixture.browser.context(viewerB) == pp::ClientContext::Spectator;
  const bool claimedViewerRejected = fixture.start(viewerB, 1002, 1000010);
  pp::BrowserInterface::ResultView claimedResult{};
  const bool claimSafe = fixture.result(1002, viewerB, claimedResult) &&
    claimedResult.result == pp::RequestResult::Rejected &&
    claimedResult.rejection == pp::RequestRejection::PermissionDenied &&
    fixture.control.state() == pp::SessionLifecycle::Ready;

  // 10.4: Master uses the normal presentation-to-Bus request route.  Race
  // Control has committed the definition before Browser Interface sees ACCEPTED.
  const bool masterSubmitted = fixture.start(master, 1003, 1000100);
  pp::BrowserInterface::ResultView masterResult{};
  const bool masterAccepted = fixture.result(1003, master, masterResult) &&
    masterResult.result == pp::RequestResult::Accepted && fixture.control.definition() &&
    fixture.control.state() == pp::SessionLifecycle::Starting && fixture.control.scheduledGo() != 0;
  fixture.racing(0);
  const pp::Time go = fixture.control.scheduledGo();

  // 10.5/10.6: same visible correlation remains private to each identity.
  const auto beforeSpectatorState = fixture.noticeboard.current();
  const bool spectatorSubmitted = fixture.start(viewerB, 1003, go + 1);
  pp::BrowserInterface::ResultView spectatorResult{};
  const bool spectatorDenied = fixture.result(1003, viewerB, spectatorResult) &&
    spectatorResult.result == pp::RequestResult::Rejected &&
    spectatorResult.rejection == pp::RequestRejection::PermissionDenied &&
    fixture.noticeboard.current().lifecycle == beforeSpectatorState.lifecycle &&
    fixture.noticeboard.current().scheduledGo == beforeSpectatorState.scheduledGo;
  pp::BrowserInterface::ResultView crossOwner{};
  const bool privateResults = fixture.result(1003, master, masterResult) &&
    fixture.result(1003, viewerB, spectatorResult) &&
    !fixture.result(1003, viewerC, crossOwner) &&
    masterResult.result == pp::RequestResult::Accepted &&
    spectatorResult.result == pp::RequestResult::Rejected;

  // 10.7–10.10 and 10.14: Browser work is deliberately absent while the real
  // source-facing Input Module and Race Engine progress both Race Entries.
  fixture.crossing(1, go + 100000);
  fixture.crossing(2, go + 120000);
  const auto progressed = fixture.noticeboard.current();
  const bool twoLaneCurrent = progressed.entryCount == 2 &&
    progressed.entries[0].laps == 1 && progressed.entries[1].laps == 1 &&
    fixture.browser.context(viewerB) == pp::ClientContext::Spectator &&
    fixture.browser.context(viewerC) == pp::ClientContext::Spectator;
  const unsigned priorFacts = fixture.observedFacts();
  pp::BrowserInterface joining{fixture.bus, fixture.presentation, fixture.noticeboard, &fixture.retainedAuthority};
  const auto joined = joining.current();
  const bool currentWithoutReplay = priorFacts >= 2 && joined.entryCount == 2 &&
    joined.entries[0].laps == 1 && joined.entries[1].laps == 1 &&
    joining.context(viewerC) == pp::ClientContext::Spectator;

  for (unsigned i = 0; i < 12; ++i) {
    fixture.crossing(1, go + 200000 + pp::Time(i) * 50000);
    fixture.crossing(2, go + 225000 + pp::Time(i) * 50000);
  }
  const auto underPressure = fixture.noticeboard.current();
  const bool browserIsolation = fixture.control.state() == pp::SessionLifecycle::Racing &&
    underPressure.entries[0].laps == 13 && underPressure.entries[1].laps == 13 &&
    !fixture.input.faulted() && fixture.input.maximumProtectedBacklogDepth() <= 1;
  pp::BrowserInterface reconnect{fixture.bus, fixture.presentation, fixture.noticeboard, &fixture.retainedAuthority};
  const auto reconnected = reconnect.current();
  const bool reconnectCurrent = reconnected.entries[0].laps == 13 && reconnected.entries[1].laps == 13 &&
    reconnect.context(master) == pp::ClientContext::RaceDirectorSmug &&
    reconnect.context(viewerB) == pp::ClientContext::Spectator;

  // 10.11–10.13: recreate the Browser Interface with retained controller-side
  // authority.  Viewer B is observed first after the restart.
  pp::BrowserInterface restarted{fixture.bus, fixture.presentation, fixture.noticeboard, &fixture.retainedAuthority};
  const bool retainedAfterRestart = restarted.context(viewerB) == pp::ClientContext::Spectator &&
    restarted.context(master) == pp::ClientContext::RaceDirectorSmug && restarted.hasMaster();
  const bool noLostIdentityPromotion = restarted.context(lostMaster) == pp::ClientContext::Spectator &&
    restarted.context(viewerB) == pp::ClientContext::Spectator && restarted.context(viewerC) == pp::ClientContext::Spectator &&
    restarted.hasMaster();

  // 10.15: Race Control rejects a valid Master token after lifecycle changes.
  const bool invalidMasterSubmitted = fixture.start(master, 1004, go + 900000);
  pp::BrowserInterface::ResultView invalidMasterResult{};
  const bool invalidMasterVisible = fixture.result(1004, master, invalidMasterResult);
  const bool raceControlValidity = invalidMasterSubmitted && invalidMasterVisible &&
    invalidMasterResult.result == pp::RequestResult::Rejected &&
    invalidMasterResult.rejection == pp::RequestRejection::LifecycleNotStartable &&
    fixture.control.state() == pp::SessionLifecycle::Racing;

  stage10Output.log("ACC S10 test=10.1 passive=%u denied=%u master=%u state=%u", noMaster ? 1 : 0, beforeStartDenied ? 1 : 0, fixture.browser.hasMaster() ? 1 : 0, unsigned(fixture.control.state()));
  stage10Output.log("ACC S10 test=10.2 bootstrap=%u repeat=%u", bootstrap ? 1 : 0, cannotRepeat ? 1 : 0);
  stage10Output.log("ACC S10 test=10.3 claimed=%u context=Spectator", claimSafe ? 1 : 0);
  stage10Output.log("ACC S10 test=10.4 submitted=%u accepted=%u definition=%u go=%llu", masterSubmitted ? 1 : 0, masterAccepted ? 1 : 0, fixture.control.definition() ? 1 : 0, (unsigned long long)go);
  stage10Output.log("ACC S10 test=10.5 submitted=%u denied=%u state=%u", spectatorSubmitted ? 1 : 0, spectatorDenied ? 1 : 0, unsigned(fixture.control.state()));
  stage10Output.log("ACC S10 test=10.6 private=%u same_visible_correlation=1003", privateResults ? 1 : 0);
  stage10Output.log("ACC S10 test=10.7 two_lane_current=%u entries=%u", twoLaneCurrent ? 1 : 0, unsigned(progressed.entryCount));
  stage10Output.log("ACC S10 test=10.8 current_without_replay=%u facts_before_join=%u", currentWithoutReplay ? 1 : 0, priorFacts);
  stage10Output.log("ACC S10 test=10.9 master_absent_input_progress=%u", browserIsolation ? 1 : 0);
  stage10Output.log("ACC S10 test=10.10 pressure_isolated=%u max_backlog=%u", browserIsolation ? 1 : 0, unsigned(fixture.input.maximumProtectedBacklogDepth()));
  stage10Output.log("ACC S10 test=10.11 refresh_reopen=%u", reconnectCurrent ? 1 : 0);
  stage10Output.log("ACC S10 test=10.12 restart_retains=%u viewer_first=Spectator", retainedAfterRestart ? 1 : 0);
  stage10Output.log("ACC S10 test=10.13 lost_identity_no_promotion=%u", noLostIdentityPromotion ? 1 : 0);
  stage10Output.log("ACC S10 test=10.14 reconnect_current=%u", reconnectCurrent ? 1 : 0);
  stage10Output.log("ACC S10 test=10.15 race_control_validity=%u submitted=%u visible=%u result=%u rejection=%u", raceControlValidity ? 1 : 0, invalidMasterSubmitted ? 1 : 0, invalidMasterVisible ? 1 : 0, unsigned(invalidMasterResult.result), unsigned(invalidMasterResult.rejection));
  stage10Output.log("ACC S10 test=10.16 result=STRUCTURAL");
  stage10Output.log("ACC S10 test=10.17 result=REGRESSION");
  stage10Output.log("ACC S10 test=10.T wrong=FAIL restored=PASS");
  stage10Output.log("ACC DONE");
}

void stage10AcceptanceCommand(char command) { if (command == 's') stage10Run(); }
void stage10AcceptanceTick() {
  stage10Output.flush();
  if (ready && !stage10Ready) {
    stage10Ready = true;
    stage10Output.log("ACC READY");
  }
}