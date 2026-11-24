#pragma once
#include "taster_link_transport.h"
#include "taster_link_display_projection.h"

namespace pp::taster_link::transport {

// Core-only bridge. No Taster renderer or network worker is given this
// object. Invoke Peer::poll after the existing Core owner services control,
// engine, and committed preferences. The preference publication revision
// is supplied by that same owner, not invented from network observations.
class RaceControlPort final : public CorePort {
public:
  RaceControlPort(RaceControlModule& control,const Noticeboard& notice,
                  const TasterPresentationPreferencesController* preferences=nullptr,
                  const uint64_t* preferencesRevision=nullptr)
    :control_(control),notice_(notice),preferences_(preferences),preferencesRevision_(preferencesRevision){}
  bool open(Session s) override {return control_.openRemoteTaster(s);}
  void close() override {control_.closeRemoteTaster();}
  Status submit(const Command& q,uint64_t now) override {return control_.submitRemoteTaster(q,now);}
  bool execute() override {return control_.executeRemoteTaster();}
  Status result(Session s,uint64_t id) const override {return control_.remoteTasterResult(s,id);}
  bool sample(uint64_t now,wire::DisplaySnapshot& out) const override {
    if((preferences_!=nullptr)!=(preferencesRevision_!=nullptr)) return false;
    TasterSnapshot sample{};
    sample.state=notice_.currentAt(now);
    sample.readiness=control_.readiness();
    if(const auto* proposal=control_.proposedRaceSetup()) sample.proposal=*proposal;
    sample.proposalRevision=control_.proposalRevision();
    return wire::projectDisplay(sample,preferences_?&preferences_->current():nullptr,
      preferencesRevision_?*preferencesRevision_:0,control_.remoteTasterContext(),now,out);
  }
private:
  RaceControlModule& control_;
  const Noticeboard& notice_;
  const TasterPresentationPreferencesController* preferences_;
  const uint64_t* preferencesRevision_;
};
} // namespace pp::taster_link::transport
