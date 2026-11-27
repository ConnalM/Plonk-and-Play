#pragma once

#include <initializer_list>

#include "core.h"

namespace pp::core_runtime {

// The real Core runtime and its host contract share this subscription list.
// It contains only the existing production message routes; no test fixture,
// detector simulation or presentation adapter is introduced here.
struct RuntimeEndpoints {
  Bus::Endpoint lifecycle;
  Bus::Endpoint memory;
  Bus::Endpoint input;
  Bus::Endpoint raceControl;
  Bus::Endpoint raceEngine;
  Bus::Endpoint presentation;
};

inline bool subscribeRuntime(Bus& bus, const RuntimeEndpoints& endpoints) {
  bool subscribed = true;
  const auto add = [&](Bus::Endpoint endpoint, Type type) {
    subscribed = bus.subscribe(endpoint, type) && subscribed;
  };

  add(endpoints.memory, Type::LoadConfiguration);
  add(endpoints.lifecycle, Type::ConfigurationLoaded);
  add(endpoints.input, Type::SessionOperation);
  add(endpoints.input, Type::FinishSettlement);
  add(endpoints.raceControl, Type::StartRequest);
  add(endpoints.raceControl, Type::SetupRequest);
  add(endpoints.raceControl, Type::SessionOperationRequest);
  add(endpoints.raceControl, Type::PauseSettled);
  add(endpoints.raceControl, Type::RaceIntegrityFault);
  add(endpoints.raceControl, Type::CompetitionComplete);
  add(endpoints.raceEngine, Type::InputEvent);
  add(endpoints.raceEngine, Type::GoScheduled);
  add(endpoints.raceEngine, Type::SessionOperation);
  add(endpoints.raceEngine, Type::InputSettlement);
  add(endpoints.raceEngine, Type::FinishSettled);
  add(endpoints.raceEngine, Type::EnduranceExpired);
  add(endpoints.raceEngine, Type::RaceIntegrityFault);
  for (const Type type : {Type::NoticeboardChanged, Type::GoScheduled,
                          Type::LapCompleted, Type::CompetitionComplete,
                          Type::RequestResult, Type::Paused,
                          Type::RestartScheduled, Type::Resumed,
                          Type::FalseStart, Type::HistoryStored,
                          Type::StorageFault, Type::EnduranceExpired}) {
    add(endpoints.presentation, type);
  }
  return subscribed;
}

}  // namespace pp::core_runtime
