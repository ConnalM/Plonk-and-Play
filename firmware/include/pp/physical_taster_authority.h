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
};
} // namespace pp
