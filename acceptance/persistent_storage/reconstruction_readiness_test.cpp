#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "pp/history_store.h"

using namespace pp;

class TestStore final : public Store {
public:
  bool online = true;
  bool readFailed = false;
  unsigned readCalls = 0;
  unsigned writeCalls = 0;
  std::vector<std::vector<uint8_t>> slots = std::vector<std::vector<uint8_t>>(220);

  bool available() const override { return online; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    ++readCalls;
    if (!online || readFailed || slot >= slots.size() || slots[slot].size() != size)
      return false;
    std::memcpy(data, slots[slot].data(), size);
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    ++writeCalls;
    if (!online || slot >= slots.size() || !data) return false;
    slots[slot].assign(data, data + size);
    return true;
  }
  bool canWrite(size_t) const override { return online; }
};

static bool check(const char* name, bool value) {
  std::cout << name << '=' << (value ? "PASS" : "FAIL") << '\n';
  return value;
}

int main() {
  bool ok = true;

  // A physical mount/boundary object does not eagerly scan History. The first
  // logical operation performs the lazy namespace reconstruction.
  TestStore store;
  SlotHistoryStore history(store);
  ok &= check("history_not_scanned_before_first_use", store.readCalls == 0);
  const uint8_t empty = history.count();
  ok &= check("history_reconstructs_on_first_use", empty == 0 && store.readCalls > 0);

  // An append cannot bypass reconstruction: metadata reads precede the first
  // payload/commit writes on a fresh logical consumer.
  TestStore appendStore;
  SlotHistoryStore appendHistory(appendStore);
  const uint8_t payload[] = {'r', 'a', 'c', 'e'};
  uint32_t sequence = 0;
  const bool appended = appendHistory.append(payload, sizeof(payload), sequence);
  ok &= check("append_reconstructs_before_publish", appended && sequence == 1 &&
              appendStore.readCalls > 0 && appendStore.writeCalls > 0);

  // An unavailable physical boundary cannot be presented as reconstructed
  // durable History and cannot publish a new record.
  TestStore unavailable;
  unavailable.online = false;
  SlotHistoryStore unavailableHistory(unavailable);
  uint32_t rejectedSequence = 0;
  const bool rejectedAppend = !unavailableHistory.append(payload, sizeof(payload), rejectedSequence);
  ok &= check("unavailable_history_not_durable", unavailableHistory.count() == 0 &&
              rejectedAppend && rejectedSequence == 0 && unavailable.writeCalls == 0);

  // A mounted boundary with an unreadable metadata read cannot expose a
  // reconstructed record through the History interface.
  TestStore unreadable;
  unreadable.readFailed = true;
  SlotHistoryStore unreadableHistory(unreadable);
  uint8_t recovered[8]{};
  size_t recoveredBytes = 0;
  uint32_t recoveredSequence = 0;
  ok &= check("history_read_failure_not_served", unreadableHistory.count() == 0 &&
              !unreadableHistory.loadNewest(0, recovered, sizeof(recovered), recoveredBytes,
                                             recoveredSequence) && recoveredBytes == 0 &&
              recoveredSequence == 0);

  return ok ? 0 : 1;
}
