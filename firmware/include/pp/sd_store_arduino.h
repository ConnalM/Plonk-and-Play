#pragma once

// WP2A: a thin SD-backed implementation of the existing product Store seam.
// SlotHistoryStore owns the metadata commit/index boundary; each individual
// Store write is staged and verified before it is atomically published.

#include <Arduino.h>
#include <FS.h>

#include "core.h"
#include "race_engine.h"
#include "physical_sd_owner.h"

namespace pp {

class Esp32SpiSdStore final : public Store {
public:
  static constexpr int CsPin = PhysicalSdOwner::CsPin;
  static constexpr int MosiPin = PhysicalSdOwner::MosiPin;
  static constexpr int SckPin = PhysicalSdOwner::SckPin;
  static constexpr int MisoPin = PhysicalSdOwner::MisoPin;
  static constexpr uint32_t SpiFrequency = PhysicalSdOwner::SpiFrequency;
  // Engineering values for the current S3/P1 backend. They are deliberately
  // not product capacity promises; final thresholds remain a hardware task.
  static constexpr uint64_t ProvisionalLowFreeBytes = 128u * 1024u;
  static constexpr uint64_t TransactionOverheadBytes = 4096u;

  explicit Esp32SpiSdStore(PhysicalSdOwner& owner): owner_(owner) {}

  bool begin(const char* root = "/pp-p1") {
    root_ = root ? root : "/pp-p1";
    auto lease = owner_.lock();
    mounted_ = bool(lease) && owner_.available();
    if (!mounted_) {
      Serial.println("[P8-P1] SD mount FAILED");
      return false;
    }
    owner_.mkdir(root_);
    cleanupSlotTemps();
    Serial.printf("[P8-P1] SD mount OK card=%llu total=%llu used=%llu\n",
                  static_cast<unsigned long long>(owner_.cardSize()),
                  static_cast<unsigned long long>(owner_.totalBytes()),
                  static_cast<unsigned long long>(owner_.usedBytes()));
    return true;
  }

  bool available() const override { return mounted_ && owner_.available(); }
  PhysicalSdOwner::Lease storageLock() { return owner_.lock(); }
  File open(const char* path, const char* mode) { return owner_.open(path, mode); }
  File open(const String& path, const char* mode) { return owner_.open(path, mode); }
  bool remove(const char* path) { return owner_.remove(path); }
  bool remove(const String& path) { return owner_.remove(path); }
  void reportPhysicalIoFailure(PhysicalSdOwner::Lease& lease) {
    if (lease) lease.reportIoFailure();
    mounted_ = false;
    lastHealth_ = StoreHealth::Unavailable;
  }
  StoreHealth health() const override {
    if (!available()) return StoreHealth::Unavailable;
    if (lastHealth_ == StoreHealth::Corrupt || lastHealth_ == StoreHealth::Unsupported) return lastHealth_;
    if (!owner_.durable()) return StoreHealth::Unavailable;
    if (lastHealth_ == StoreHealth::Insufficient) return lastHealth_;
    const uint64_t free = freeBytes();
    if (!free) return StoreHealth::Insufficient;
    return free <= ProvisionalLowFreeBytes ? StoreHealth::Low : StoreHealth::Healthy;
  }
  uint64_t totalBytes() const override {
    auto lease = owner_.lock();
    return lease && available() ? owner_.totalBytes() : 0;
  }
  uint64_t freeBytes() const override {
    auto lease = owner_.lock();
    if (!lease || !available()) return 0;
    const uint64_t total = totalBytes();
    const uint64_t used = owner_.usedBytes();
    return used < total ? total - used : 0;
  }
  bool canWrite(size_t bytes) const override {
    if (!available()) return false;
    const uint64_t needed = static_cast<uint64_t>(bytes) + TransactionOverheadBytes;
    return freeBytes() >= needed;
  }
  bool reclaim(unsigned slot) override {
    auto lease = owner_.lock();
    if (!lease || !available()) return false;
    return owner_.remove(path(slot));
  }

  bool recover() {
    auto lease = owner_.lock();
    mounted_ = bool(lease) && owner_.available();
    if (lease && mounted_) { owner_.mkdir(root_); lastHealth_ = StoreHealth::Healthy; cleanupSlotTemps(); }
    return mounted_;
  }

  bool read(unsigned slot, uint8_t* data, size_t size) override {
    return readInternal(slot, data, size, false);
  }

  bool readRequired(unsigned slot, uint8_t* data, size_t size) override {
    return readInternal(slot, data, size, true);
  }

private:
  bool readInternal(unsigned slot, uint8_t* data, size_t size, bool required) {
    if (!available() || !data) return false;
    auto lease = owner_.lock();
    if (!lease || !available() || !data) return false;
    File file = owner_.open(path(slot), FILE_READ);
    if (!file) {
      if (required) reportPhysicalIoFailure(lease);
      return false;
    }
    const size_t actual = file.size();
    if (actual != size) {
      file.close();
      lastHealth_ = StoreHealth::Corrupt;
      const unsigned byte = slot >> 3;
      const uint8_t bit = uint8_t(1u << (slot & 7u));
      if (byte < sizeof(mismatchLogged_) && !(mismatchLogged_[byte] & bit)) {
        mismatchLogged_[byte] |= bit;
        Serial.printf("[PERSIST] history_payload_integrity slot=%u path=%s expected=%u observed=%u reason=size\n",
                      slot, path(slot).c_str(), static_cast<unsigned>(size), static_cast<unsigned>(actual));
      }
      return false;
    }
    const size_t got = file.read(data, size);
    file.close();
    if (got != size) { reportPhysicalIoFailure(lease); return false; }
    return true;
  }

public:
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    auto lease = owner_.lock();
    if (!lease || !available() || !data) return false;
    if (!canWrite(size)) { lastHealth_ = StoreHealth::Insufficient; return false; }
    const String target = path(slot);
    String temporary = target;
    temporary += ".tmp";
    owner_.remove(temporary);
    File file = owner_.open(temporary, FILE_WRITE);
    if (!file) { reportPhysicalIoFailure(lease); return false; }
    const size_t written = file.write(data, size);
    file.flush();
    file.close();
    if (written != size) {
      owner_.remove(temporary);
      reportPhysicalIoFailure(lease);
      return false;
    }

    // Read back in bounded chunks before publication. This keeps the Store
    // seam independent of the final SD streaming/recovery implementation and
    // prevents a short/corrupt payload from becoming visible at target.
    File verify = owner_.open(temporary, FILE_READ);
    if (!verify || verify.size() != size) {
      if (verify) verify.close();
      owner_.remove(temporary);
      if (!verify) reportPhysicalIoFailure(lease);
      return false;
    }
    uint8_t buffer[128];
    size_t offset = 0;
    bool matches = true;
    bool ioFailure = false;
    while (offset < size) {
      const size_t want = (size - offset) < sizeof(buffer) ? (size - offset) : sizeof(buffer);
      const size_t got = verify.read(buffer, want);
      if (got != want) {
        ioFailure = true;
        matches = false;
        break;
      }
      if (memcmp(buffer, data + offset, want) != 0) {
        matches = false;
        break;
      }
      offset += want;
    }
    verify.close();
    if (!matches) {
      owner_.remove(temporary);
      if (ioFailure) reportPhysicalIoFailure(lease);
      return false;
    }

    // ESP-IDF VFS rename is the publication boundary and leaves the prior
    // target intact if the rename cannot be completed.
    if (!owner_.rename(temporary, target)) {
      owner_.remove(temporary);
      return false;
    }
    lastHealth_ = healthAfterWrite();
    return true;
  }

private:
  String root_ = "/pp-p1";
  PhysicalSdOwner& owner_;
  bool mounted_ = false;
  mutable StoreHealth lastHealth_ = StoreHealth::Unavailable;
  uint8_t mismatchLogged_[26]{};

  StoreHealth healthAfterWrite() const {
    const uint64_t free = freeBytes();
    return free == 0 ? StoreHealth::Insufficient : (free <= ProvisionalLowFreeBytes ? StoreHealth::Low : StoreHealth::Healthy);
  }

  String path(unsigned slot) const {
    String value(root_);
    value += "/slot-";
    value += String(slot);
    value += ".bin";
    return value;
  }
  void cleanupSlotTemps() {
    if (!mounted_) return;
    File root = owner_.open(root_, FILE_READ);
    if (!root || !root.isDirectory()) { if (root) root.close(); return; }
    for (;;) {
      File file = root.openNextFile();
      if (!file) break;
      String name = file.name();
      const bool temporary = name.indexOf("slot-") >= 0 && name.endsWith(".bin.tmp");
      file.close();
      if (temporary) (void)owner_.remove(name);
    }
    root.close();
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
      for(size_t i=1;i<queueSize_;++i){
        queue_[i-1]=queue_[i];
      }
      --queueSize_;
      storagePending_=false;
    }
  }
  bool finalize() override {
    drain(); if(!complete_||queueSize_||storagePending_||!mounted())return false;
    for(uint8_t i=0;i<entries_;++i)if(!sealFile(i)){complete_=false;return false;}
    finalized_=true; return true;
  }
  void abort() override { auto lease=store_.storageLock(); if(lease&&mounted()){for(uint8_t i=0;i<PP_MAX_ENTRIES;++i){store_.remove(path(i));store_.remove(markerPath(i));}} queueSize_=0; finalized_=false; complete_=true; storagePending_=false; dropped_=0; session_=0; entries_=0; for(auto&c:counts_)c=0; }
  uint8_t detailEntryCount() const override { return entries_; }
  uint32_t detailRecordCount(uint8_t entry) const override { return entry<entries_?counts_[entry]:0; }
  bool readDetail(uint8_t entry,uint32_t index,LapRecord& out) const override {
    if(!finalized_||entry>=entries_||index>=counts_[entry])return false;
    auto lease=store_.storageLock(); if(!lease)return false;
    File f=store_.open(path(entry),FILE_READ);
    if(!f){store_.reportPhysicalIoFailure(lease);return false;}
    const bool ok=f.seek(HeaderBytes+size_t(index)*RecordBytes)&&readEncoded(f,out);
    f.close();
    return ok;
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
  bool writeRecord(uint8_t entry,const LapRecord&r){auto lease=store_.storageLock();if(!lease||!store_.canWrite(HeaderBytes+RecordBytes))return false;String p=path(entry);File f=store_.open(p,FILE_APPEND);if(!f){f=store_.open(p,FILE_WRITE);if(!f){store_.reportPhysicalIoFailure(lease);return false;}uint8_t h[HeaderBytes]{};put32(h,0x50504432u);put32(h+4,session_);h[8]=entry;if(f.write(h,sizeof(h))!=sizeof(h)){f.close();store_.reportPhysicalIoFailure(lease);return false;}}uint8_t b[RecordBytes]{};put32(b,r.lapNumber);put64(b+4,r.startTime);put64(b+12,r.finishTime);put64(b+20,r.lapTime);put64(b+28,r.racingFinishTime);b[36]=r.valid?1:0;const bool ok=f.write(b,sizeof(b))==sizeof(b);f.flush();f.close();if(!ok)store_.reportPhysicalIoFailure(lease);return ok;}
  String markerPath(uint8_t entry) const { String p=path(entry);p.remove(p.length()-4);p+=".ok";return p; }
  bool sealFile(uint8_t entry){auto lease=store_.storageLock();if(!lease||!store_.canWrite(12))return false;File f=store_.open(path(entry),FILE_READ);if(!f){store_.reportPhysicalIoFailure(lease);return false;}if(f.size()!=HeaderBytes+size_t(counts_[entry])*RecordBytes){f.close();return false;}uint8_t h[HeaderBytes]{};if(f.read(h,sizeof(h))!=sizeof(h)||get32(h)!=0x50504432u||get32(h+4)!=session_||h[8]!=entry){f.close();return false;}uint32_t hash=2166136261u;uint8_t b[RecordBytes]{};for(uint32_t i=0;i<counts_[entry];++i){if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1){f.close();return false;}hash=hashRecord(hash,b);}f.close();File out=store_.open(markerPath(entry),FILE_WRITE);if(!out){store_.reportPhysicalIoFailure(lease);return false;}uint8_t marker[12]{};put32(marker,0x504f4b32u);put32(marker+4,counts_[entry]);put32(marker+8,hash);const bool ok=out.write(marker,sizeof(marker))==sizeof(marker);out.flush();out.close();if(!ok)store_.reportPhysicalIoFailure(lease);return ok&&verifyFile(entry);}
  bool verifyFile(uint8_t entry) const {auto lease=store_.storageLock();if(!lease)return false;File f=store_.open(path(entry),FILE_READ);if(!f){store_.reportPhysicalIoFailure(lease);return false;}if(f.size()!=HeaderBytes+size_t(counts_[entry])*RecordBytes){f.close();return false;}uint8_t h[HeaderBytes]{};if(f.read(h,sizeof(h))!=sizeof(h)||get32(h)!=0x50504432u||get32(h+4)!=session_||h[8]!=entry){f.close();return false;}uint32_t hash=2166136261u;uint8_t b[RecordBytes]{};for(uint32_t i=0;i<counts_[entry];++i){if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1){f.close();return false;}hash=hashRecord(hash,b);}f.close();File marker=store_.open(markerPath(entry),FILE_READ);if(!marker){store_.reportPhysicalIoFailure(lease);return false;}uint8_t m[12]{};const bool ok=marker.read(m,sizeof(m))==sizeof(m)&&get32(m)==0x504f4b32u&&get32(m+4)==counts_[entry]&&get32(m+8)==hash;if(marker)marker.close();return ok;}
  void cleanupOrphans(){auto lease=store_.storageLock();if(!lease||!mounted())return;File root=store_.open("/pp-p1",FILE_READ);if(!root||!root.isDirectory()){if(root)root.close();return;}for(;;){File file=root.openNextFile();if(!file)break;String name=file.name();const bool orphan=name.indexOf("active-")>=0&&(name.endsWith(".tmp")||name.endsWith(".ok"));file.close();if(orphan)store_.remove(name);}root.close();}
  static bool readEncoded(File&f,LapRecord&r){uint8_t b[RecordBytes]{};if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1)return false;r.lapNumber=get32(b);r.startTime=get64(b+4);r.finishTime=get64(b+12);r.lapTime=get64(b+20);r.racingFinishTime=get64(b+28);r.valid=b[36]!=0;return true;}
};

} // namespace pp
