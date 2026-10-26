#include "pp/persistent_storage.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main() {
  pp::RaceEngineModule::CompletedRaceResult source{};
  source.sealed = true;
  source.valid = true;
  source.formatVersion = pp::RaceEngineModule::ResultFormatVersion;
  source.mode = pp::SessionMode::LapRace;
  source.behaviour = pp::LapFinishBehaviour::Immediate;
  source.entryCount = 2;
  source.lapTarget = 2;
  for (uint8_t i = 0; i < source.entryCount; ++i) {
    auto& entry = source.entries[i];
    entry.raceEntryId = i + 1;
    entry.mugId = 91 + i;
    entry.lane = i + 1;
    entry.laps = i ? 1 : 2;
    entry.classifiedLaps = entry.laps;
    entry.completed = i == 0;
    entry.detailComplete = true;
    entry.detail.prepare(2);
    for (uint32_t j = 0; j < 2; ++j) {
      pp::RaceEngineModule::LapRecord lap{};
      lap.lapNumber = j + 1;
      lap.startTime = 1000000u * j;
      lap.finishTime = lap.startTime + 2000000u + j;
      lap.lapTime = 2000000u + j;
      lap.racingFinishTime = lap.finishTime;
      lap.valid = true;
      assert(entry.detail.append(lap));
    }
    entry.records = entry.detail.records;
    entry.recordCount = entry.detail.recordCount;
  }

  std::vector<uint8_t> payload(pp::VersionedResultCodec::MaxBytes);
  size_t length = 0;
  assert(pp::VersionedResultCodec::encode(source, payload.data(), payload.size(), length));
  payload.resize(length);

  std::vector<uint8_t> reconstructed;
  char line[96]{};
  for (size_t offset = 0; offset < payload.size(); offset += 16u) {
    const size_t written = pp::VersionedHistoryStore::formatPayloadHexLine(
        payload.data(), payload.size(), offset, line, sizeof(line));
    assert(written > 0);
    const std::string text(line, written - 1); // remove newline
    const std::string marker = " bytes=";
    const size_t markerPos = text.find(marker);
    assert(markerPos != std::string::npos);
    const size_t hexStart = markerPos + marker.size();
    assert(text.size() - hexStart == ((payload.size() - offset) < 16u ? (payload.size() - offset) : 16u) * 2u);
    for (size_t i = hexStart; i < text.size(); i += 2u) {
      unsigned value = 0;
      assert(std::sscanf(text.substr(i, 2).c_str(), "%02x", &value) == 1);
      reconstructed.push_back(static_cast<uint8_t>(value));
    }
  }
  assert(reconstructed == payload);
  pp::RaceEngineModule::CompletedRaceResult decoded{};
  assert(pp::VersionedResultCodec::decode(reconstructed.data(), reconstructed.size(), decoded));
  std::puts("PSF payload_dump PASS lossless_hex_roundtrip=1 codec_roundtrip=1");
  return 0;
}
