#include "pp/taster_presentation_preferences.h"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {
class TestStore final : public pp::Store {
public:
  static constexpr size_t Slots = 216;
  bool online = true;
  bool failWrite = false;
  bool tornWrite = false;
  bool present[Slots]{};
  uint8_t data[Slots][pp::TasterPresentationPreferencesMemory::RecordSize]{};

  bool available() const override { return online; }
  bool read(unsigned slot, uint8_t* out, size_t size) override {
    if (slot >= Slots || !present[slot] || size > sizeof(data[slot])) return false;
    memcpy(out, data[slot], size);
    return true;
  }
  bool write(unsigned slot, const uint8_t* in, size_t size) override {
    if (!online || failWrite || slot >= Slots || size > sizeof(data[slot])) return false;
    present[slot] = true;
    const size_t copied = tornWrite ? size / 2 : size;
    memcpy(data[slot], in, copied);
    return !tornWrite;
  }
};

pp::TasterPresentationPreferences preferences(const char* lane1, const char* lane2,
                                               pp::TasterCarGraphic graphic1,
                                               pp::TasterCarGraphic graphic2) {
  pp::TasterPresentationPreferences value{};
  strncpy(value.lane1Label, lane1, sizeof(value.lane1Label) - 1);
  strncpy(value.lane2Label, lane2, sizeof(value.lane2Label) - 1);
  value.lane1Graphic = graphic1;
  value.lane2Graphic = graphic2;
  return value;
}
}

int main() {
  TestStore store;

  // Existing race configuration occupies its own namespace.  C1 migration is
  // an absent display-preferences record, not a rewrite of this data.
  pp::Memory raceSettings(store);
  pp::Configuration existing{};
  existing.laps = 943;
  existing.durationMinutes = 17;
  existing.lane1MugId = 7;
  existing.lane2MugId = 8;
  assert(raceSettings.save(existing));

  pp::TasterPresentationPreferencesMemory storage(store);
  pp::TasterPresentationPreferences loaded{};
  assert(storage.load(loaded) == pp::LoadStatus::DefaultsMissing);
  assert(strcmp(loaded.lane1Label, "LANE 1") == 0);
  assert(strcmp(loaded.lane2Label, "LANE 2") == 0);
  assert(loaded.lane1Graphic == pp::TasterCarGraphic::GreenMgb);
  assert(loaded.lane2Graphic == pp::TasterCarGraphic::RedEtype);

  const auto first = preferences("Blue team", "Red team", pp::TasterCarGraphic::RedEtype,
                                 pp::TasterCarGraphic::GreenMgb);
  assert(storage.save(first));
  pp::TasterPresentationPreferencesMemory reboot(store);
  assert(reboot.load(loaded) == pp::LoadStatus::Remembered);
  assert(strcmp(loaded.lane1Label, "Blue team") == 0);
  assert(loaded.lane1Graphic == pp::TasterCarGraphic::RedEtype);
  assert(loaded.lane2Graphic == pp::TasterCarGraphic::GreenMgb);

  // Invalid labels and interrupted writes do not displace the last committed
  // preferences.  A corrupt newer copy similarly falls back to the old copy.
  auto invalid = first;
  invalid.lane1Graphic = static_cast<pp::TasterCarGraphic>(99);
  assert(!reboot.save(invalid));
  const auto second = preferences("Ã…sa", "Guest", pp::TasterCarGraphic::GreenMgb,
                                  pp::TasterCarGraphic::RedEtype);
  store.tornWrite = true;
  assert(!reboot.save(second));
  store.tornWrite = false;
  pp::TasterPresentationPreferencesMemory afterTorn(store);
  assert(afterTorn.load(loaded) == pp::LoadStatus::Remembered);
  assert(strcmp(loaded.lane1Label, "Blue team") == 0);
  assert(afterTorn.save(second));
  const unsigned newest = afterTorn.generation() % 2 ? pp::TasterPresentationPreferencesMemory::FirstSlot
                                                       : pp::TasterPresentationPreferencesMemory::SecondSlot;
  store.data[newest][20] ^= 1;
  pp::TasterPresentationPreferencesMemory corruptNewest(store);
  assert(corruptNewest.load(loaded) == pp::LoadStatus::Remembered);
  assert(strcmp(loaded.lane1Label, "Blue team") == 0);

  // Factory display restore is display-only: race settings and racer identity
  // remain byte-for-byte recoverable from their existing namespace.
  assert(corruptNewest.restoreFactory());
  pp::TasterPresentationPreferencesMemory restored(store);
  assert(restored.load(loaded) == pp::LoadStatus::Remembered);
  assert(strcmp(loaded.lane1Label, "LANE 1") == 0);
  pp::Memory raceReboot(store);
  pp::Configuration recovered{};
  assert(raceReboot.load(recovered) == pp::LoadStatus::Remembered);
  assert(recovered.laps == 943 && recovered.durationMinutes == 17);
  assert(recovered.lane1MugId == 7 && recovered.lane2MugId == 8);

  // The controller boundary permits only a ready controlling Browser.
  pp::TasterPresentationPreferencesController controller(restored);
  assert(controller.update(false, true, first) == pp::TasterPresentationPreferencesResult::PermissionDenied);
  assert(controller.update(true, false, first) == pp::TasterPresentationPreferencesResult::LifecycleNotReady);
  assert(controller.update(true, true, first) == pp::TasterPresentationPreferencesResult::Accepted);
  assert(controller.restoreFactory(false, true) == pp::TasterPresentationPreferencesResult::PermissionDenied);
  assert(controller.restoreFactory(true, true) == pp::TasterPresentationPreferencesResult::Accepted);

  TestStore missing;
  missing.online = false;
  pp::TasterPresentationPreferencesMemory unavailable(missing);
  assert(unavailable.load(loaded) == pp::LoadStatus::DefaultsStorageError);
  std::cout << "Taster presentation preferences host PASS\n";
}
