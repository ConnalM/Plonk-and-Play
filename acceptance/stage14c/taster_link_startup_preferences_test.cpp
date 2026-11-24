// Committed publication: remote owner never observes uncommitted or
// mixed preference fields while HTTP writer persists a change.
#include "pp/taster_presentation_preferences.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
using namespace pp;
class StoreFixture final:public Store{
public:
  bool online=true,fail=false;
  std::map<unsigned,std::vector<uint8_t>> slots;
  bool available()const override{return online;}
  bool read(unsigned s,uint8_t* p,size_t n)override{
    auto it=slots.find(s);
    if(!online || it==slots.end() || it->second.size()!=n)return false;
    std::memcpy(p,it->second.data(),n);return true;
  }
  bool write(unsigned s,const uint8_t* p,size_t n)override{
    if(!online || fail)return false;
    slots[s]=std::vector<uint8_t>(p,p+n);return true;
  }
  bool reclaim(unsigned s)override{slots.erase(s);return true;}
};
int main(){
  StoreFixture store;
  TasterPresentationPreferencesMemory memory(store);
  TasterPresentationPreferencesController controller(memory);
  assert(!controller.committed().loaded);
  store.online=false;assert(controller.load()==LoadStatus::DefaultsStorageError);
  assert(!controller.committed().loaded);
  store.online=true;assert(controller.load()==LoadStatus::DefaultsMissing);
  assert(controller.committed().loaded && controller.committed().revision==0);
  auto a=TasterPresentationPreferences::factoryDefaults();
  std::strcpy(a.lane1Label,"ALPHA");std::strcpy(a.lane2Label,"ALPHA");
  a.lane1Graphic=a.lane2Graphic=TasterCarGraphic::GreenMgb;
  auto b=a;std::strcpy(b.lane1Label,"BETA");std::strcpy(b.lane2Label,"BETA");
  b.lane1Graphic=b.lane2Graphic=TasterCarGraphic::RedEtype;
  using R=TasterPresentationPreferencesResult;
  assert(controller.update(true,true,a)==R::Accepted);
  auto first=controller.committed();assert(first.revision==1);
  assert(controller.update(false,true,b)==R::PermissionDenied);
  assert(controller.update(true,false,b)==R::LifecycleNotReady);
  store.fail=true;assert(controller.update(true,true,b)==R::StorageUnavailable);
  assert(controller.committed().revision==first.revision);
  assert(!std::strcmp(controller.committed().value.lane1Label,"ALPHA"));
  store.fail=false;
  std::atomic<bool> done{false};
  std::thread reader([&]{
    uint64_t previous=1;
    do{
      const auto s=controller.committed();
      assert(s.loaded && s.revision>=previous);previous=s.revision;
      const auto& want=s.revision%2?a:b;
      assert(!std::strcmp(s.value.lane1Label,want.lane1Label) &&
        !std::strcmp(s.value.lane2Label,want.lane2Label) &&
        s.value.lane1Graphic==want.lane1Graphic && s.value.lane2Graphic==want.lane2Graphic);
    }while(!done.load());
  });
  for(unsigned i=2;i<=201;++i)assert(controller.update(true,true,i%2?a:b)==R::Accepted);
  done=true;reader.join();
  assert(controller.committed().revision==201);
  assert(controller.restoreFactory(true,true)==R::Accepted);
  assert(controller.committed().revision==202);
  TasterPresentationPreferencesMemory reloaded(store);
  TasterPresentationPreferencesController after(reloaded);
  assert(after.load()==LoadStatus::Remembered);
  assert(after.committed().revision==202 && !std::strcmp(after.committed().value.lane1Label,"LANE 1"));
  std::cout<<"S3g2i coherent committed preference publication PASS\n";
}
