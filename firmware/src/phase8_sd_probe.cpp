#if defined(PP_STAGE15_PHASE8_SD)

#include <Arduino.h>
#include <esp_system.h>

#include "pp/diagnostic_sd.h"
#include "pp/diagnostic_memory.h"

namespace {
pp::diagnostic::Esp32SpiSdDiagnosticStorage storage;
pp::diagnostic::InMemoryDiagnosticRecorder recorder(storage);
pp::diagnostic::ControllerEpoch epoch{};

void printMemory(const char* label) {
  Serial.printf("[PHASE8] %s heap=%u min_heap=%u max_alloc=%u psram=%u free_psram=%u flash=%u\n",
                label, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                ESP.getPsramSize(), ESP.getFreePsram(), ESP.getFlashChipSize());
}

pp::diagnostic::RecordView makeRecord(uint32_t index, uint8_t* payload) {
  static pp::diagnostic::RecordEnvelope envelope{};
  static char text[64]{};
  envelope = {};
  envelope.controllerEpoch = epoch;
  envelope.session.value = 1;
  envelope.client.value = 1 + (index % 2);
  envelope.kind = pp::diagnostic::EventKind::ResourceHealth;
  envelope.timestamp = micros();
  envelope.traceId = 0x15000000ULL + index;
  envelope.payloadType = 1;
  snprintf(text, sizeof(text), "phase8-record-%lu", static_cast<unsigned long>(index));
  memcpy(payload, text, strlen(text));
  pp::diagnostic::RecordView view{};
  view.envelope = envelope;
  view.payload = payload;
  view.payloadBytes = strlen(text);
  return view;
}

void printStorage(const char* label) {
  const auto status = storage.status();
  Serial.printf("[PHASE8] %s health=%u backend=%u durable=%u card=%llu total=%llu used=%llu count=%u path=%s\n",
                label, unsigned(status.health), unsigned(status.backend), status.durable ? 1U : 0U,
                static_cast<unsigned long long>(storage.cardBytes()),
                static_cast<unsigned long long>(storage.totalBytes()),
                static_cast<unsigned long long>(storage.usedBytes()),
                static_cast<unsigned>(storage.count()), storage.path());
}
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[BUILD] STAGE15 PHASE8 ESP32-8048S070 SD PROBE");
  Serial.printf("[PHASE8] chip=%s revision=%d sdk=%s\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getSdkVersion());
  printMemory("boot");

  epoch.value = (static_cast<uint64_t>(micros()) << 16) ^ esp_random();
  if (!epoch.valid()) epoch.value = 1;
  const auto beginResult = storage.begin(epoch);
  Serial.printf("[PHASE8] begin result=%u epoch=%llu\n", unsigned(beginResult), static_cast<unsigned long long>(epoch.value));
  printStorage("begin");
  if (beginResult != pp::diagnostic::StorageResult::Accepted) return;

  pp::diagnostic::SessionStatus session{};
  pp::diagnostic::SessionStartRequest startRequest{};
  startRequest.controllerEpoch = epoch;
  startRequest.client.value = 1;
  const auto startResult = recorder.start(startRequest, session);
  Serial.printf("[PHASE8] session_start result=%u lifecycle=%u storage=%u volatile=%u\n",
                unsigned(startResult), unsigned(session.lifecycle), unsigned(session.storage), session.evidenceVolatile ? 1U : 0U);

  uint8_t payload[128]{};
  for (uint32_t i = 0; i < 10; ++i) {
    auto record = makeRecord(i, payload);
    const uint32_t before = micros();
    uint64_t sequence = 0;
    const auto result = recorder.tryAppend(record, sequence);
    const uint32_t elapsed = micros() - before;
    Serial.printf("[PHASE8] append index=%lu result=%u seq=%llu us=%lu count=%u dropped=%llu\n",
                  static_cast<unsigned long>(i), unsigned(result), static_cast<unsigned long long>(sequence),
                  static_cast<unsigned long>(elapsed), static_cast<unsigned>(storage.count()),
                  static_cast<unsigned long long>(session.droppedRecords));
    if ((i == 2) || (i == 9)) printMemory(i == 2 ? "active" : "heavy");
  }

  pp::diagnostic::RecordEnvelope envelope{}; size_t length = 0; uint8_t readPayload[128]{};
  const uint32_t readStart = micros();
  const bool readOk = storage.read(0, envelope, readPayload, sizeof(readPayload), length);
  Serial.printf("[PHASE8] read_active ok=%u bytes=%u us=%lu seq=%llu\n", readOk ? 1U : 0U,
                static_cast<unsigned>(length), static_cast<unsigned long>(micros() - readStart),
                static_cast<unsigned long long>(envelope.sequence));

  storage.failNextAppend(pp::diagnostic::StorageResult::WriteFailed);
  auto failureRecord = makeRecord(99, payload); uint64_t failureSequence = 0;
  const auto failure = recorder.tryAppend(failureRecord, failureSequence);
  recorder.status(session);
  Serial.printf("[PHASE8] injected_append result=%u health=%u fault=%u incomplete=%u dropped=%llu\n",
                unsigned(failure), unsigned(session.storage), unsigned(session.lastFailure),
                session.evidenceIncomplete ? 1U : 0U, static_cast<unsigned long long>(session.droppedRecords));

  recorder.stop(session);
  Serial.printf("[PHASE8] finalize lifecycle=%u result=%u\n", unsigned(session.lifecycle), unsigned(storage.close()));
  printStorage("finalized");

  pp::diagnostic::Esp32SpiSdDiagnosticStorage reconstructed;
  const auto recoverOpen = reconstructed.recoverExisting(epoch);
  const auto recovered = reconstructed.recover();
  Serial.printf("[PHASE8] reconstruct open=%u recover=%u count=%u finalized=%u\n",
                unsigned(recoverOpen), unsigned(recovered), static_cast<unsigned>(reconstructed.count()),
                reconstructed.finalized() ? 1U : 0U);
  const uint32_t retrievalStart = micros();
  const bool reconstructedRead = reconstructed.read(9, envelope, readPayload, sizeof(readPayload), length);
  Serial.printf("[PHASE8] reconstruct_read ok=%u bytes=%u us=%lu seq=%llu\n", reconstructedRead ? 1U : 0U,
                static_cast<unsigned>(length), static_cast<unsigned long>(micros() - retrievalStart),
                static_cast<unsigned long long>(envelope.sequence));
  printMemory("end");
}

void loop() {
  delay(1000);
}

#endif // PP_STAGE15_PHASE8_SD
