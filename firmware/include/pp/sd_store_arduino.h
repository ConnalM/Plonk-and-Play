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
    Serial.printf("[P8-P1] History store begin owner_lease=%u owner_mounted=%u owner_health=%u\n",
                  lease ? 1u : 0u, owner_.available() ? 1u : 0u,
                  static_cast<unsigned>(owner_.health()));
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

  bool available() const override { return mounted_; }
  PhysicalSdOwner::Lease storageLock() { return owner_.lock(); }
  File open(const char* path, const char* mode) { return owner_.open(path, mode); }
  File open(const String& path, const char* mode) { return owner_.open(path, mode); }
  bool remove(const char* path) { return owner_.remove(path); }
  bool remove(const String& path) { return owner_.remove(path); }
  StoreHealth health() const override {
    if (!mounted_) return StoreHealth::Unavailable;
    if (lastHealth_ == StoreHealth::Insufficient) return lastHealth_;
    const uint64_t free = freeBytes();
    if (!free) return StoreHealth::Insufficient;
    return free <= ProvisionalLowFreeBytes ? StoreHealth::Low : StoreHealth::Healthy;
  }
  uint64_t totalBytes() const override {
    auto lease = owner_.lock();
    return lease && mounted_ ? owner_.totalBytes() : 0;
  }
  uint64_t freeBytes() const override {
    auto lease = owner_.lock();
    if (!lease || !mounted_) return 0;
    const uint64_t total = totalBytes();
    const uint64_t used = owner_.usedBytes();
    return used < total ? total - used : 0;
  }
  bool canWrite(size_t bytes) const override {
    if (!mounted_) return false;
    const uint64_t needed = static_cast<uint64_t>(bytes) + TransactionOverheadBytes;
    return freeBytes() >= needed;
  }
  bool reclaim(unsigned slot) override {
    auto lease = owner_.lock();
    if (!lease || !mounted_) return false;
    return owner_.remove(path(slot));
  }

  bool recover() {
    auto lease = owner_.lock();
    mounted_ = bool(lease) && owner_.available();
    Serial.printf("[P8-P1] History store recover owner_lease=%u owner_mounted=%u owner_health=%u\n",
                  lease ? 1u : 0u, owner_.available() ? 1u : 0u,
                  static_cast<unsigned>(owner_.health()));
    if (lease && mounted_) { owner_.mkdir(root_); lastHealth_ = StoreHealth::Healthy; cleanupSlotTemps(); }
    Serial.printf("[P8-P1] History store recover result mounted=%u health=%u\n",
                  mounted_ ? 1u : 0u, static_cast<unsigned>(health()));
    return mounted_;
  }

  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (!mounted_ || !data) return false;
    auto lease = owner_.lock();
    if (!lease || !mounted_ || !data) return false;
    File file = owner_.open(path(slot), FILE_READ);
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
    auto lease = owner_.lock();
    if (!lease || !mounted_ || !data) return false;
    if (!canWrite(size)) { lastHealth_ = StoreHealth::Insufficient; return false; }
    const String target = path(slot);
    String temporary = target;
    temporary += ".tmp";
    owner_.remove(temporary);
    File file = owner_.open(temporary, FILE_WRITE);
    if (!file) return false;
    const size_t written = file.write(data, size);
    file.flush();
    file.close();
    if (written != size) {
      owner_.remove(temporary);
      return false;
    }

    // Read back in bounded chunks before publication. This keeps the Store
    // seam independent of the final SD streaming/recovery implementation and
    // prevents a short/corrupt payload from becoming visible at target.
    File verify = owner_.open(temporary, FILE_READ);
    if (!verify || verify.size() != size) {
      if (verify) verify.close();
      owner_.remove(temporary);
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
      owner_.remove(temporary);
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
    Serial.printf("[P8-P1] detail_begin session=%lu entries=%u mounted=%u storage_pending=%u\n",
                  static_cast<unsigned long>(session_), static_cast<unsigned>(entries_),
                  mounted() ? 1u : 0u, storagePending_ ? 1u : 0u);
    cleanupOrphans();
    return true;
  }
  bool append(uint8_t entry,const LapRecord& record) override {
    if(!complete_||finalized_||entry>=entries_)return false;
    if(queueSize_>=QueueCapacity){complete_=false;storagePending_=false;++dropped_;return false;}
    queue_[queueSize_].entry=entry; queue_[queueSize_].record=record; ++queueSize_; ++counts_[entry]; return true;
  }
  void drain() override {
    if(storagePending_){
      detailTrace("drain", "precondition", "storage_pending");
      return;
    }
    while(queueSize_){Item item=queue_[0];
      if(!writeRecord(item.entry,item.record)){
        storagePending_=true;
        Serial.printf("[P8-P1] detail_drain write_failed entry=%u reason=%s queue=%u\n",
                      static_cast<unsigned>(item.entry), lastWriteFailure_,
                      static_cast<unsigned>(queueSize_));
        detailTrace("drain", "write", lastWriteFailure_);
        return;
      }
      for(size_t i=1;i<queueSize_;++i){
        queue_[i-1]=queue_[i];
      }
      --queueSize_;
      storagePending_=false;
    }
  }
  bool finalize() override {
    Serial.printf("[P8-P1] detail_finalize begin session=%lu entries=%u complete=%u queue=%u pending=%u mounted=%u dropped=%lu\n",
                  static_cast<unsigned long>(session_), static_cast<unsigned>(entries_),
                  complete_ ? 1u : 0u, static_cast<unsigned>(queueSize_),
                  storagePending_ ? 1u : 0u, mounted() ? 1u : 0u,
                  static_cast<unsigned long>(dropped_));
    detailTrace("finalize", "drain_begin", queueSize_ ? "queued" : "empty");
    drain();
    detailTrace("finalize", "drain_end", storagePending_ ? "storage_pending" : (queueSize_ ? "queue_pending" : "complete"));
    if(!complete_){Serial.println("[P8-P1] detail_finalize failed stage=precondition reason=detail_incomplete");detailTrace("finalize", "precondition", "detail_incomplete");return false;}
    if(queueSize_){Serial.printf("[P8-P1] detail_finalize failed stage=drain reason=queue_pending queue=%u\n",static_cast<unsigned>(queueSize_));detailTrace("finalize", "drain", "queue_pending");return false;}
    if(storagePending_){Serial.println("[P8-P1] detail_finalize failed stage=drain reason=storage_pending");detailTrace("finalize", "drain", "storage_pending");return false;}
    if(!mounted()){Serial.println("[P8-P1] detail_finalize failed stage=mount reason=unmounted");detailTrace("finalize", "mount", "unmounted");return false;}
    for(uint8_t i=0;i<entries_;++i){
      if(!sealFile(i)){
        complete_=false;
        Serial.printf("[P8-P1] detail_finalize failed stage=seal entry=%u reason=%s verify=%s\n",
                      static_cast<unsigned>(i), lastSealFailure_, lastVerifyFailure_);
        detailTrace("finalize", "seal", lastSealFailure_);
        return false;
      }
    }
    finalized_=true;
    Serial.printf("[P8-P1] detail_finalize success session=%lu entries=%u\n",
                  static_cast<unsigned long>(session_), static_cast<unsigned>(entries_));
    detailTrace("finalize", "success", "none");
    return true;
  }
  void abort() override { auto lease=store_.storageLock(); if(lease&&mounted()){for(uint8_t i=0;i<PP_MAX_ENTRIES;++i){store_.remove(path(i));store_.remove(markerPath(i));}} queueSize_=0; finalized_=false; complete_=true; storagePending_=false; dropped_=0; session_=0; entries_=0; for(auto&c:counts_)c=0; }
  uint8_t detailEntryCount() const override { return entries_; }
  uint32_t detailRecordCount(uint8_t entry) const override { return entry<entries_?counts_[entry]:0; }
  bool readDetail(uint8_t entry,uint32_t index,LapRecord& out) const override {
    if(!finalized_||entry>=entries_||index>=counts_[entry])return false;
    auto lease=store_.storageLock(); if(!lease)return false;
    File f=store_.open(path(entry),FILE_READ);
    if(!f)return false;
    const bool ok=f.seek(HeaderBytes+size_t(index)*RecordBytes)&&readEncoded(f,out);
    f.close();
    return ok;
  }
  bool detailComplete() const override { return complete_&&finalized_; }
  uint32_t dropped() const override { return dropped_; }
  bool storagePending() const override { return storagePending_||queueSize_!=0; }
  bool detailLost() const override { return !complete_||dropped_!=0; }
  bool recover() override {
    Serial.printf("[P8-P1] detail_recover before mounted=%u pending=%u finalized=%u\n",
                  mounted() ? 1u : 0u, storagePending_ ? 1u : 0u, finalized_ ? 1u : 0u);
    if(mounted()){storagePending_=false;return true;}
    storagePending_=!store_.recover();
    const bool recovered = !storagePending_;
    Serial.printf("[P8-P1] detail_recover result mounted=%u pending=%u recovered=%u\n",
                  mounted() ? 1u : 0u, storagePending_ ? 1u : 0u, recovered ? 1u : 0u);
    return recovered;
  }
  bool mounted() const { return store_.available(); }
private:
  struct Item { uint8_t entry=0; LapRecord record{}; };
  static constexpr size_t HeaderBytes=24,RecordBytes=40;
  Esp32SpiSdStore& store_; uint32_t session_=0,dropped_=0;uint8_t entries_=0;uint32_t counts_[PP_MAX_ENTRIES]{};Item queue_[QueueCapacity]{};size_t queueSize_=0;bool complete_=true,finalized_=false,storagePending_=false;
  const char* lastWriteFailure_="none";
  const char* lastSealFailure_="none";
  mutable const char* lastVerifyFailure_="none";
  String path(uint8_t entry) const { String p="/pp-p1/active-";p+=String(session_);p+="-";p+=String(entry);p+=".tmp";return p; }
  static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
  static void put64(uint8_t*p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(8*i));}
  static uint32_t get32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
  static uint64_t get64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(p[i])<<(8*i);return v;}
  static uint32_t hashRecord(uint32_t h,const uint8_t*b){for(size_t i=0;i<RecordBytes;++i){h^=b[i];h*=16777619u;}return h;}
  void detailTrace(const char* stage,const char* substage,const char* reason) const {
    // Direct Serial evidence deliberately bypasses the bounded diagnostic queue.
    // It is observation-only and carries no payload data.
    Serial.printf("[P8-P1] detail_trace stage=%s substage=%s reason=%s session=%lu entry_count=%u queue=%u pending=%u mounted=%u dropped=%lu\n",
                  stage, substage, reason, static_cast<unsigned long>(session_),
                  static_cast<unsigned>(entries_), static_cast<unsigned>(queueSize_),
                  storagePending_ ? 1u : 0u, mounted() ? 1u : 0u,
                  static_cast<unsigned long>(dropped_));
  }
  bool writeRecord(uint8_t entry,const LapRecord&r){lastWriteFailure_="none";auto lease=store_.storageLock();if(!lease){lastWriteFailure_="storage_lease";detailTrace("write","lease","storage_lease");return false;}if(!store_.canWrite(HeaderBytes+RecordBytes)){lastWriteFailure_="capacity_or_mount";detailTrace("write","capacity","capacity_or_mount");return false;}String p=path(entry);File existing=store_.open(p,FILE_READ);const bool exists=bool(existing);if(existing)existing.close();File f=exists?store_.open(p,FILE_APPEND):store_.open(p,FILE_WRITE);if(!f){lastWriteFailure_=exists?"append_open":"create_open";detailTrace("write", "open", lastWriteFailure_);return false;}if(!exists){uint8_t h[HeaderBytes]{};put32(h,0x50504432u);put32(h+4,session_);h[8]=entry;if(f.write(h,sizeof(h))!=sizeof(h)){lastWriteFailure_="header_write";f.close();detailTrace("write", "header", lastWriteFailure_);return false;}}uint8_t b[RecordBytes]{};put32(b,r.lapNumber);put64(b+4,r.startTime);put64(b+12,r.finishTime);put64(b+20,r.lapTime);put64(b+28,r.racingFinishTime);b[36]=r.valid?1:0;const bool ok=f.write(b,sizeof(b))==sizeof(b);f.flush();f.close();if(!ok){lastWriteFailure_="record_write";detailTrace("write", "record", lastWriteFailure_);}return ok;}
  String markerPath(uint8_t entry) const { String p=path(entry);p.remove(p.length()-4);p+=".ok";return p; }
  bool sealFile(uint8_t entry){lastSealFailure_="none";lastVerifyFailure_="none";auto lease=store_.storageLock();if(!lease){lastSealFailure_="storage_lease";detailTrace("seal","lease",lastSealFailure_);return false;}if(!store_.canWrite(12)){lastSealFailure_="marker_capacity_or_mount";detailTrace("seal","capacity",lastSealFailure_);return false;}File f=store_.open(path(entry),FILE_READ);if(!f){lastSealFailure_="active_open";detailTrace("seal","active_open",lastSealFailure_);return false;}if(f.size()!=HeaderBytes+size_t(counts_[entry])*RecordBytes){f.close();lastSealFailure_="active_size";detailTrace("seal","active_size",lastSealFailure_);return false;}uint8_t h[HeaderBytes]{};if(f.read(h,sizeof(h))!=sizeof(h)||get32(h)!=0x50504432u||get32(h+4)!=session_||h[8]!=entry){f.close();lastSealFailure_="active_header";detailTrace("seal","active_header",lastSealFailure_);return false;}uint32_t hash=2166136261u;uint8_t b[RecordBytes]{};for(uint32_t i=0;i<counts_[entry];++i){if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1){f.close();lastSealFailure_="active_record";detailTrace("seal","active_record",lastSealFailure_);return false;}hash=hashRecord(hash,b);}f.close();File out=store_.open(markerPath(entry),FILE_WRITE);if(!out){lastSealFailure_="marker_open";detailTrace("seal","marker_open",lastSealFailure_);return false;}uint8_t marker[12]{};put32(marker,0x504f4b32u);put32(marker+4,counts_[entry]);put32(marker+8,hash);const bool ok=out.write(marker,sizeof(marker))==sizeof(marker);out.flush();out.close();if(!ok){lastSealFailure_="marker_write";detailTrace("seal","marker_write",lastSealFailure_);return false;}if(!verifyFile(entry)){lastSealFailure_="verify";detailTrace("seal","verify",lastVerifyFailure_);return false;}return true;}
  bool verifyFile(uint8_t entry) const {lastVerifyFailure_="none";auto lease=store_.storageLock();if(!lease){lastVerifyFailure_="storage_lease";detailTrace("verify","lease",lastVerifyFailure_);return false;}File f=store_.open(path(entry),FILE_READ);if(!f){lastVerifyFailure_="active_open";detailTrace("verify","active_open",lastVerifyFailure_);return false;}if(f.size()!=HeaderBytes+size_t(counts_[entry])*RecordBytes){f.close();lastVerifyFailure_="active_size";detailTrace("verify","active_size",lastVerifyFailure_);return false;}uint8_t h[HeaderBytes]{};if(f.read(h,sizeof(h))!=sizeof(h)||get32(h)!=0x50504432u||get32(h+4)!=session_||h[8]!=entry){f.close();lastVerifyFailure_="active_header";detailTrace("verify","active_header",lastVerifyFailure_);return false;}uint32_t hash=2166136261u;uint8_t b[RecordBytes]{};for(uint32_t i=0;i<counts_[entry];++i){if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1){f.close();lastVerifyFailure_="active_record";detailTrace("verify","active_record",lastVerifyFailure_);return false;}hash=hashRecord(hash,b);}f.close();File marker=store_.open(markerPath(entry),FILE_READ);if(!marker){lastVerifyFailure_="marker_open";detailTrace("verify","marker_open",lastVerifyFailure_);return false;}uint8_t m[12]{};const bool ok=marker.read(m,sizeof(m))==sizeof(m)&&get32(m)==0x504f4b32u&&get32(m+4)==counts_[entry]&&get32(m+8)==hash;if(marker)marker.close();if(!ok)lastVerifyFailure_="marker_invalid";if(!ok)detailTrace("verify","marker",lastVerifyFailure_);return ok;}
  void cleanupOrphans(){auto lease=store_.storageLock();if(!lease||!mounted())return;File root=store_.open("/pp-p1",FILE_READ);if(!root||!root.isDirectory()){if(root)root.close();return;}for(;;){File file=root.openNextFile();if(!file)break;String name=file.name();const bool orphan=name.indexOf("active-")>=0&&(name.endsWith(".tmp")||name.endsWith(".ok"));file.close();if(orphan)store_.remove(name);}root.close();}
  static bool readEncoded(File&f,LapRecord&r){uint8_t b[RecordBytes]{};if(f.read(b,sizeof(b))!=sizeof(b)||b[36]>1)return false;r.lapNumber=get32(b);r.startTime=get64(b+4);r.finishTime=get64(b+12);r.lapTime=get64(b+20);r.racingFinishTime=get64(b+28);r.valid=b[36]!=0;return true;}
};

} // namespace pp
