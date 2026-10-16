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

  bool recover() {
    spi_.begin(SckPin, MisoPin, MosiPin, CsPin);
    mounted_ = SD.begin(CsPin, spi_, SpiFrequency);
    if (mounted_) SD.mkdir(root_);
    return mounted_;
  }

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
    abort(); session_=sessionId; entries_=entries; complete_=true; storagePending_=!mounted(); finalized_=false; dropped_=0; queueSize_=0; memset(counts_,0,sizeof(counts_));
    cleanupOrphans();
    return true;
  }
  bool append(uint8_t entry,const LapRecord& record) override {
    if(!complete_||finalized_||entry>=entries_)return false;
    if(queueSize_>=QueueCapacity){complete_=false;storagePending_=false;++dropped_;return false;}
    queue_[queueSize_].entry=entry; queue_[queueSize_].record=record; ++queueSize_; ++counts_[entry]; return true;
  }
  void drain() override {
    if(storagePending_)return;
    while(queueSize_){Item item=queue_[0];
      if(!writeRecord(item.entry,item.record)){storagePending_=true;return;}
      for(size_t i=1;i<queueSize_;++i)queue_[i-1]=queue_[i];--queueSize_;
      storagePending_=false;
    }
  }
  bool finalize() override {
    drain(); if(!complete_||queueSize_||storagePending_||!mounted())return false;
    for(uint8_t i=0;i<entries_;++i)if(!sealFile(i)){complete_=false;return false;}
    finalized_=true; return true;
  }
  void abort() override { if(mounted()){for(uint8_t i=0;i<PP_MAX_ENTRIES;++i){SD.remove(path(i));SD.remove(markerPath(i));}} queueSize_=0; finalized_=false; complete_=true; storagePending_=false; dropped_=0; session_=0; entries_=0; for(auto&c:counts_)c=0; }
  uint8_t detailEntryCount() const override { return entries_; }
  uint32_t detailRecordCount(uint8_t entry) const override { return entry<entries_?counts_[entry]:0; }
  bool readDetail(uint8_t entry,uint32_t index,LapRecord& out) const override {
    if(!finalized_||entry>=entries_||index>=counts_[entry])return false; File f=SD.open(path(entry),FILE_READ);if(!f)return false;const bool ok=f.seek(HeaderBytes+size_t(index)*RecordBytes)&&readEncoded(f,out);f.close();return ok;
  }
  bool detailComplete() const override { return complete_&&finalized_; }
  uint32_t dropped() const override { return dropped_; }
  bool storagePending() const override { return storagePending_||queueSize_!=0; }
  bool detailLost() const override { return !complete_||dropped_!=0; }
  bool recover() override { if(mounted()){storagePending_=false;return true;} storagePending_=!store_.recover(); return !storagePending_; }
  bool mounted() const { return store_.available(); }
private:
  struct Item { uint8_t entry=0; LapRecord record{}; };
  static constexpr size_t HeaderBytes=24,RecordBytes=40;
  Esp32SpiSdStore& store_; uint32_t session_=0,dropped_=0;uint8_t entries_=0;uint32_t counts_[PP_MAX_ENTRIES]{};Item queue_[QueueCapacity]{};size_t queueSize_=0;bool complete_=true,finalized_=false,storagePending_=false;
  String path(uint8_t entry) const { String p="/pp-p1/active-";p+=String(session_);p+="-";p+=String(entry);p+=".tmp";return p; }
  static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
  static void put64(uint8_t*p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(8*i));}
  static uint32_t get32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
  static uint64_t get64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(p[i])<<(8*i);return v;}
  static uint32_t hashRecord(uint32_t h,const uint8_t*b){for(size_t i=0;i<RecordBytes;++i){h^=b[i];h*=16777619u;}return h;}
  bool writeRecord(uint8_t entry,const LapRecord&r){String p=path(entry);File f=SD.open(p,FILE_APPEND);if(!f){f=SD.open(p,FILE_WRITE);if(!f)return false;uint8_t h[HeaderBytes]{};put32(h,0x50504432u);put32(h+4,session_);h[8]=entry;if(f.write(h,sizeof(h))!=sizeof(h)){f.close();return false;}}uint8_t b[RecordBytes]{};put32(b,r.lapNumber);put64(b+4,r.startTime);put64(b+12,r.finishTime);put64(b+20,r.lapTime);put64(b+28,r.racingFinishTime);b[36]=r.valid?1:0;const bool ok=f.write(b,sizeof(b))==sizeof(b);f.flush();f.close();return ok;}
  String markerPath(uint8_t entry) const { String p=path(entry);p.remove(p.length()-4);p+=".ok";return p; }
  bool sealFile(uint8_t entry){File f=SD.open(path(entry),FILE_READ);if(!f||f.size()!=HeaderBytes+size_t(counts_[entry])*RecordBytes){if(f)f.close();return false;}uint8_t h[HeaderBytes]{};if(f.read(h,sizeof(h))!=sizeof(h)||get32(h)!=0x50504432u||get32(h+4)!=session_||h[8]!=entry){f.close();return false;}uint32_t hash=2166136261u;uint8_t b[RecordBytes]{};for(uint32_t i=0;i<counts_[entry];++i){if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1){f.close();return false;}hash=hashRecord(hash,b);}f.close();File out=SD.open(markerPath(entry),FILE_WRITE);if(!out)return false;uint8_t marker[12]{};put32(marker,0x504f4b32u);put32(marker+4,counts_[entry]);put32(marker+8,hash);const bool ok=out.write(marker,sizeof(marker))==sizeof(marker);out.flush();out.close();return ok&&verifyFile(entry);}
  bool verifyFile(uint8_t entry) const {File f=SD.open(path(entry),FILE_READ);if(!f||f.size()!=HeaderBytes+size_t(counts_[entry])*RecordBytes){if(f)f.close();return false;}uint8_t h[HeaderBytes]{};if(f.read(h,sizeof(h))!=sizeof(h)||get32(h)!=0x50504432u||get32(h+4)!=session_||h[8]!=entry){f.close();return false;}uint32_t hash=2166136261u;uint8_t b[RecordBytes]{};for(uint32_t i=0;i<counts_[entry];++i){if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1){f.close();return false;}hash=hashRecord(hash,b);}f.close();File marker=SD.open(markerPath(entry),FILE_READ);uint8_t m[12]{};const bool ok=marker&&marker.read(m,sizeof(m))==sizeof(m)&&get32(m)==0x504f4b32u&&get32(m+4)==counts_[entry]&&get32(m+8)==hash;if(marker)marker.close();return ok;}
  void cleanupOrphans(){if(!mounted())return;File root=SD.open("/pp-p1");if(!root||!root.isDirectory()){if(root)root.close();return;}for(;;){File file=root.openNextFile();if(!file)break;String name=file.name();const bool orphan=name.indexOf("active-")>=0&&(name.endsWith(".tmp")||name.endsWith(".ok"));file.close();if(orphan)SD.remove(name);}root.close();}
  static bool readEncoded(File&f,LapRecord&r){uint8_t b[RecordBytes]{};if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1)return false;r.lapNumber=get32(b);r.startTime=get64(b+4);r.finishTime=get64(b+12);r.lapTime=get64(b+20);r.racingFinishTime=get64(b+28);r.valid=b[36]!=0;return true;}
};

} // namespace pp
