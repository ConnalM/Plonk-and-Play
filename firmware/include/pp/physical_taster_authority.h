#pragma once
#include "taster_surface.h"
#include "taster_presentation_preferences.h"

namespace pp {
// Presentation-only contract. Time supplied here is in the authority's
// clock domain in the integrated build; a future remote adapter must map it.
class PhysicalTasterAuthority : public TasterAuthority {
public:
  virtual NoticeboardState stateAt(Time now) const = 0;
  virtual SessionLifecycle lifecycle() const = 0;
  virtual const TasterPresentationPreferences* committedPreferences() const = 0;
  virtual void pollResults() = 0;

  // The integrated authority needs no link-level touch gate.  A wireless
  // Taster overrides these hooks so a physical gesture can never become a
  // command without a fresh authoritative remote snapshot.
  virtual bool beginTouch() { return true; }
  virtual void observeReleased() {}
  virtual void cancelTouch() {}
  // A remote owner increments this whenever its authority or fresh gesture
  // context is invalidated.  The integrated authority remains stable.
  virtual uint64_t interactionRevision() const { return 0; }
};
} // namespace pp
