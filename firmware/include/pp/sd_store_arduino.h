#pragma once

// WP2A: a thin SD-backed implementation of the existing product Store seam.
// SlotHistoryStore owns the metadata commit/index boundary; each individual
// Store write is staged and verified before it is atomically published.

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "core.h"
#include "race_engine.h"

namespace pp {

class Esp32SpiSdStore final : public Store {
public:
  static constexpr int CsPin = 10;
  static constexpr int MosiPin = 11;
  static constexpr int SckPin = 12;
  static constexpr int MisoPin = 13;
  static constexpr uint32_t SpiFrequency = 20000000U;

  bool begin(const char* root = "/pp-p1") {
    root_ = root ? root : "/pp-p1";
    Serial.printf("[P8-P1] SD mount begin cs=%d mosi=%d sck=%d miso=%d\n",
                  CsPin, MosiPin, SckPin, MisoPin);
    spi_.begin(SckPin, MisoPin, MosiPin, CsPin);
    mounted_ = SD.begin(CsPin, spi_, SpiFrequency);
    if (!mounted_) {
      Serial.println("[P8-P1] SD mount FAILED");
      return false;
    }
    SD.mkdir(root_);
    Serial.printf("[P8-P1] SD mount OK card=%llu total=%llu used=%llu\n",
                  static_cast<unsigned long long>(SD.cardSize()),
                  static_cast<unsigned long long>(SD.totalBytes()),
                  static_cast<unsigned long long>(SD.usedBytes()));
    return true;
  }

  bool available() const override { return mounted_; }

  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (!mounted_ || !data) return false;
    File file = SD.open(path(slot), FILE_READ);
    if (!file) return false;
    const size_t actual = file.size();
    if (actual != size) {
      memset(data, 0, size);
      file.close();
      return true;
    }
    const size_t got = file.read(data, size);
    file.close();
    return got == size;
  }

  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (!mounted_ || !data) return false;
    const String target = path(slot);
    String temporary = target;
    temporary += ".tmp";
    SD.remove(temporary);
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) return false;
    const size_t written = file.write(data, size);
    file.flush();
    file.close();
    if (written != size) {
      SD.remove(temporary);
      return false;
    }

    // Read back in bounded chunks before publication. This keeps the Store
    // seam independent of the final SD streaming/recovery implementation and
    // prevents a short/corrupt payload from becoming visible at target.
    File verify = SD.open(temporary, FILE_READ);
    if (!verify || verify.size() != size) {
      if (verify) verify.close();
      SD.remove(temporary);
      return false;
    }
    uint8_t buffer[128];
    size_t offset = 0;
    bool matches = true;
    while (offset < size) {
      const size_t want = (size - offset) < sizeof(buffer) ? (size - offset) : sizeof(buffer);
      if (verify.read(buffer, want) != want || memcmp(buffer, data + offset, want) != 0) {
        matches = false;
        break;
      }
      offset += want;
    }
    verify.close();
    if (!matches) {
      SD.remove(temporary);
      return false;
    }

    // ESP-IDF VFS rename is the publication boundary and leaves the prior
    // target intact if the rename cannot be completed.
    if (!SD.rename(temporary, target)) {
      SD.remove(temporary);
      return false;
    }
    return true;
  }

private:
  String root_ = "/pp-p1";
  SPIClass spi_{FSPI};
  bool mounted_ = false;

  String path(unsigned slot) const {
    String value(root_);
    value += "/slot-";
    value += String(slot);
    value += ".bin";
    return value;
  }
};

// Production active-detail boundary.  Race Engine appends only to this small
// queue; SD I/O is drained from the controller loop and finalized after the
// race, so lap acceptance never waits on the card.
class SdActiveDetailStore final : public ActiveDetailStore {
public:
  explicit SdActiveDetailStore(Esp32SpiSdStore& store):store_(store){}
  static constexpr size_t QueueCapacity = 16;
  bool begin(uint32_t sessionId,uint8_t entries) override {
    abort(); session_=sessionId; entries_=entries; complete_=mounted(); finalized_=false; dropped_=0; queueSize_=0; memset(counts_,0,sizeof(counts_));
    return complete_;
  }
  bool append(uint8_t entry,const LapRecord& record) override {
    if(!complete_||finalized_||entry>=entries_)return false;
    if(queueSize_>=QueueCapacity){complete_=false;++dropped_;return false;}
    queue_[queueSize_].entry=entry; queue_[queueSize_].record=record; ++queueSize_; ++counts_[entry]; return true;
  }
  void drain() override {
    while(queueSize_){Item item=queue_[0];for(size_t i=1;i<queueSize_;++i)queue_[i-1]=queue_[i];--queueSize_;
      if(!writeRecord(item.entry,item.record)){complete_=false;++dropped_;}}
  }
  bool finalize() override {
    drain(); if(!complete_||!mounted())return false;
    for(uint8_t i=0;i<entries_;++i){File f=SD.open(path(i),FILE_READ);if(!f||f.size()!=HeaderBytes+size_t(counts_[i])*RecordBytes){if(f)f.close();complete_=false;return false;}f.close();}
    finalized_=true; return true;
  }
  void abort() override { if(mounted()){for(uint8_t i=0;i<PP_MAX_ENTRIES;++i)SD.remove(path(i));} queueSize_=0; finalized_=false; complete_=true; dropped_=0; session_=0; entries_=0; for(auto&c:counts_)c=0; }
  uint8_t detailEntryCount() const override { return entries_; }
  uint32_t detailRecordCount(uint8_t entry) const override { return entry<entries_?counts_[entry]:0; }
  bool readDetail(uint8_t entry,uint32_t index,LapRecord& out) const override {
    if(!finalized_||entry>=entries_||index>=counts_[entry])return false; File f=SD.open(path(entry),FILE_READ);if(!f)return false;const bool ok=f.seek(HeaderBytes+size_t(index)*RecordBytes)&&readEncoded(f,out);f.close();return ok;
  }
  bool detailComplete() const override { return complete_&&finalized_; }
  uint32_t dropped() const override { return dropped_; }
  bool mounted() const { return store_.available(); }
private:
  struct Item { uint8_t entry=0; LapRecord record{}; };
  static constexpr size_t HeaderBytes=16,RecordBytes=40;
  Esp32SpiSdStore& store_; uint32_t session_=0,dropped_=0;uint8_t entries_=0;uint32_t counts_[PP_MAX_ENTRIES]{};Item queue_[QueueCapacity]{};size_t queueSize_=0;bool complete_=true,finalized_=false;
  String path(uint8_t entry) const { String p="/pp-p1/active-";p+=String(session_);p+="-";p+=String(entry);p+=".tmp";return p; }
  static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
  static void put64(uint8_t*p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(8*i));}
  static uint32_t get32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
  static uint64_t get64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(p[i])<<(8*i);return v;}
  bool writeRecord(uint8_t entry,const LapRecord&r){String p=path(entry);File f=SD.open(p,FILE_APPEND);if(!f){f=SD.open(p,FILE_WRITE);if(!f)return false;uint8_t h[HeaderBytes]{};put32(h,0x50504431u);put32(h+4,session_);h[8]=entry;if(f.write(h,sizeof(h))!=sizeof(h)){f.close();return false;}}uint8_t b[RecordBytes]{};put32(b,r.lapNumber);put64(b+4,r.startTime);put64(b+12,r.finishTime);put64(b+20,r.lapTime);put64(b+28,r.racingFinishTime);b[36]=r.valid?1:0;const bool ok=f.write(b,sizeof(b))==sizeof(b);f.flush();f.close();return ok;}
  static bool readEncoded(File&f,LapRecord&r){uint8_t b[RecordBytes]{};if(f.read(b,sizeof(b))!=sizeof(b))return false;r.lapNumber=get32(b);r.startTime=get64(b+4);r.finishTime=get64(b+12);r.lapTime=get64(b+20);r.racingFinishTime=get64(b+28);r.valid=b[36]!=0;return true;}
};

} // namespace pp
