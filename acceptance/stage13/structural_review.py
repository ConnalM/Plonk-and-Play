from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[2]
core=(root/'firmware/include/pp/core.h').read_text()
sc=(root/'firmware/include/pp/session_definition.h').read_text()
rc=(root/'firmware/include/pp/race_control.h').read_text()
re=(root/'firmware/include/pp/race_engine.h').read_text()
br=(root/'firmware/include/pp/browser_interface.h').read_text()
pi=(root/'firmware/platformio.ini').read_text()
main=(root/'firmware/src/main.cpp').read_text()
checks={
 'append_types': all(x in core for x in ['FinishSettlement=18','FinishSettled=19','FalseStart=20','HistoryStored=21','StorageFault=22']),
 'stage13_envs': '[env:stage13acceptance]' in pi and '[env:stage13demo]' in pi,
 'start_configuration': all(x in sc for x in ['redLightCount','startSignal','startTiming']),
 'committed_go_schedule': all(x in rc for x in ['DefaultFinalDelayMinUs','DefaultFinalDelayMaxUs','const Time scheduledGo=now+Time(reds)*interval+delay','scheduled.type=Type::GoScheduled','scheduled.relevantTime=scheduledGo']),
 'false_start_capability': 'falseStartCapability' in sc and 'falseStartPolicy' in re,
 'false_start_bus_fact': 'Type::FalseStart' in core and 'Type::FalseStart' in re,
 'destructive_routes': all(x in br for x in ['/request/restart-race','/request/end-race']),
 'records_reconstruct_and_clear': all(x in br for x in ['/records','/request/clear-history','/request/clear-history/confirm','/request/clear-all-records/confirm']) and 'TrackRecordStore' in br,
 'record_admin_bus_authority': all(x in rc for x in ['ClearHistory','ClearLane1Records','ClearAllRecords','request.probe!=1']) and 'history_->clear()' in rc,
 'settlement_gate': 'pauseSettled_' in rc and 'LifecycleNotAbandonable' in rc,
 'destructive_confirmation_gate': 'ConfirmationRequired' in rc and 'request.probe!=1' in rc,
 'normal_bus_authority': 'case Type::FalseStart:' in core and 'case Type::HistoryStored:' in core,
 'lap_target_visible_and_persisted': 'lapTarget' in br and 'lapTarget' in re and 'result_.lapTarget' in re,
 'stage13_demo_defaults_three_laps': 'defined(PP_STAGE13_DEMO)' in main and 'proposedRaceSetup.lapTarget=3' in main,
 'demo_simulated_laps_excluded_from_records': 'recordEligible_=false' in re and 'PP_STAGE13_DEMO' in re,
}
print(json.dumps(checks,indent=2)); sys.exit(0 if all(checks.values()) else 1)
