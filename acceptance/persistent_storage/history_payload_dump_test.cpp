#include "pp/persistent_storage.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static uint32_t fnv32(const uint8_t* bytes, size_t length) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}

static std::vector<uint8_t> readHex(const char* path) {
  std::ifstream file(path);
  assert(file.good());
  std::string hex((std::istreambuf_iterator<char>(file)), {});
  std::vector<uint8_t> bytes;
  std::string digits;
  for (const char c : hex) {
    if (std::isxdigit(static_cast<unsigned char>(c))) digits.push_back(c);
  }
  assert((digits.size() % 2u) == 0u);
  for (size_t i = 0; i < digits.size(); i += 2u) {
    unsigned value = 0;
    assert(std::sscanf(digits.substr(i, 2).c_str(), "%02x", &value) == 1);
    bytes.push_back(static_cast<uint8_t>(value));
  }
  return bytes;
}

static pp::RaceEngineModule::CompletedRaceResult sampleFor(pp::LapFinishBehaviour behaviour) {
  pp::RaceEngineModule::CompletedRaceResult result{};
  result.sealed = true; result.valid = true; result.formatVersion = pp::RaceEngineModule::ResultFormatVersion;
  result.mode = pp::SessionMode::LapRace; result.behaviour = behaviour; result.entryCount = 2; result.lapTarget = 2;
  for (uint8_t i = 0; i < 2; ++i) {
    auto& entry = result.entries[i]; entry.raceEntryId = i + 1; entry.mugId = 91 + i; entry.lane = i + 1;
    entry.laps = entry.classifiedLaps = 2; entry.rank = i + 1; entry.completed = true; entry.detailComplete = true;
    assert(entry.detail.prepare(2));
    for (uint32_t j = 0; j < 2; ++j) { pp::RaceEngineModule::LapRecord lap{}; lap.lapNumber = j + 1; lap.startTime = j * 1000000u; lap.finishTime = lap.startTime + 2000000u; lap.lapTime = 2000000u; lap.racingFinishTime = lap.finishTime; lap.valid = true; assert(entry.detail.append(lap)); }
    entry.records = entry.detail.records; entry.recordCount = entry.detail.recordCount;
  }
  return result;
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const auto payload = readHex(argv[1]);
  assert(payload.size() == 338);
  const uint32_t storedChecksum = uint32_t(payload[payload.size() - 4u]) |
                                  (uint32_t(payload[payload.size() - 3u]) << 8) |
                                  (uint32_t(payload[payload.size() - 2u]) << 16) |
                                  (uint32_t(payload[payload.size() - 1u]) << 24);
  assert(fnv32(payload.data(), payload.size() - 4u) == storedChecksum);
  pp::RaceEngineModule::CompletedRaceResult decoded{};
  assert(pp::VersionedResultCodec::decode(payload.data(), payload.size(), decoded));
  assert(decoded.behaviour == pp::LapFinishBehaviour::CompleteFullRaceDistance);
  assert(decoded.entryCount == 2 && decoded.entries[0].recordCount == 2 && decoded.entries[1].recordCount == 2);
  assert(decoded.entries[0].completed && decoded.entries[1].completed);
  assert(decoded.entries[0].completionTime > 0 && decoded.entries[1].completionTime > 0);
  assert(decoded.entries[0].records[0].lapNumber == 1 && decoded.entries[0].records[1].lapNumber == 2);
  assert(decoded.entries[1].records[0].lapNumber == 1 && decoded.entries[1].records[1].lapNumber == 2);
  for (const auto& entry : decoded.entries) {
    for (uint32_t i = 0; i < entry.recordCount; ++i) {
      assert(entry.records[i].valid);
      assert(entry.records[i].lapTime > 0);
    }
  }
  for (const auto behaviour : {pp::LapFinishBehaviour::Immediate, pp::LapFinishBehaviour::CompleteCurrentLap, pp::LapFinishBehaviour::CompleteFullRaceDistance}) {
    const auto source = sampleFor(behaviour); uint8_t encoded[pp::VersionedResultCodec::MaxBytes]{}; size_t length = 0;
    assert(pp::VersionedResultCodec::encode(source, encoded, sizeof(encoded), length));
    pp::RaceEngineModule::CompletedRaceResult roundTrip{};
    assert(pp::VersionedResultCodec::decode(encoded, length, roundTrip));
    assert(roundTrip.behaviour == behaviour);
  }
  auto invalid = sampleFor(pp::LapFinishBehaviour::CompleteFullRaceDistance); uint8_t encoded[pp::VersionedResultCodec::MaxBytes]{}; size_t length = 0;
  assert(pp::VersionedResultCodec::encode(invalid, encoded, sizeof(encoded), length));
  encoded[16] = 3; // wire behaviour byte; keep the envelope valid below.
  const uint32_t checksum = fnv32(encoded, length - 4u);
  encoded[length - 4u] = static_cast<uint8_t>(checksum);
  encoded[length - 3u] = static_cast<uint8_t>(checksum >> 8);
  encoded[length - 2u] = static_cast<uint8_t>(checksum >> 16);
  encoded[length - 1u] = static_cast<uint8_t>(checksum >> 24);
  assert(!pp::VersionedResultCodec::decode(encoded, length, invalid));
  std::puts("PSF history_decode PASS sequence9_full_race_and_enum_domain=1 invalid_behaviour_rejected=1");
  return 0;
}
