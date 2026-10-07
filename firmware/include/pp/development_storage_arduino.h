#pragma once

#include <FS.h>
#include <SPIFFS.h>
#include "core.h"

namespace pp {

// Development/Wokwi storage backend. Production hardware uses the separate
// SD/FATFS backend; this class exists to exercise the same Store seam without
// making Wokwi depend on host-PC files.
class SpiffsSlotStore final : public Store {
public:
  bool begin(const char* root="/pp-dev") { root_=root?root:"/pp-dev"; ready_=SPIFFS.begin(false); return ready_; }
  bool available() const override { return ready_; }
  bool read(unsigned slot,uint8_t* data,size_t size) override {
    if(!ready_||!data)return false;File f=SPIFFS.open(path(slot),"r");if(!f)return false;const size_t actual=f.size();if(actual!=size){f.close();memset(data,0,size);return true;}const size_t got=f.read(data,size);f.close();return got==size;
  }
  bool write(unsigned slot,const uint8_t* data,size_t size) override {
    if(!ready_||!data)return false;const String target=path(slot);const String temporary=target+".tmp";File f=SPIFFS.open(temporary,"w");if(!f)return false;const size_t written=f.write(data,size);f.flush();f.close();if(written!=size){SPIFFS.remove(temporary);return false;}SPIFFS.remove(target);if(!SPIFFS.rename(temporary,target)){SPIFFS.remove(temporary);return false;}return true;
  }
  void clear() { if(!ready_)return;for(unsigned i=0;i<32;++i){SPIFFS.remove(path(i));SPIFFS.remove(path(i)+".tmp");} }
private:
  const char* root_="/pp-dev";bool ready_=false;
  String path(unsigned slot) const {String p(root_);p+="-slot-";p+=String(slot);p+=".bin";return p;}
};

}
