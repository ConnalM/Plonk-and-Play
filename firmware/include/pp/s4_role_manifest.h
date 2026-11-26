#pragma once

// S4b board-independent role declarations.
//
// This header is deliberately a manifest, not a startup composition: it
// includes neither Arduino nor ESP-IDF headers and creates no service.  It
// documents the intended ownership boundary while the accepted integrated
// firmware remains the only active composition.  A manifest/host check does
// not prove linked-image isolation; that needs the later role builds.

#include <cstdint>

namespace pp::s4 {

enum class RuntimeRole : uint8_t {
  Integrated,
  Core,
  Taster,
};

enum class Service : uint32_t {
  DetectorInput       = 1u << 0,
  RaceAuthority       = 1u << 1,
  SharedStorage       = 1u << 2,
  SmuglandHttp        = 1u << 3,
  CustomerUpdateAuthority = 1u << 4,
  TasterLinkListener      = 1u << 5,
  TasterLinkClient        = 1u << 6,
  LocalDisplay            = 1u << 7,
  LocalTouch              = 1u << 8,
  ProtectedPairingStore   = 1u << 9,
  TasterLocatorStore      = 1u << 10,
  CommittedPreferences    = 1u << 11,
  LocalPresentationPrefs  = 1u << 12,
  GatedRemoteInteraction  = 1u << 13,
};

constexpr uint32_t serviceMask(Service service) {
  return static_cast<uint32_t>(service);
}

struct RoleManifest {
  RuntimeRole role;
  uint32_t instantiatedServices;
};

// The existing accepted image remains the integrated composition.  This is a
// declaration of ownership only: the present main.cpp is intentionally not
// changed by S4b.
inline constexpr RoleManifest kIntegratedManifest{
    RuntimeRole::Integrated,
    serviceMask(Service::DetectorInput) | serviceMask(Service::RaceAuthority) |
        serviceMask(Service::SharedStorage) | serviceMask(Service::SmuglandHttp) |
        serviceMask(Service::CustomerUpdateAuthority) |
        serviceMask(Service::CommittedPreferences) |
        serviceMask(Service::LocalPresentationPrefs) |
        serviceMask(Service::LocalDisplay) | serviceMask(Service::LocalTouch),
};

// Core owns authoritative racing, physical detectors, persistent records and
// SMUGLAND. It may host the Taster link but has no display or touch service.
inline constexpr RoleManifest kCoreManifest{
    RuntimeRole::Core,
    serviceMask(Service::DetectorInput) | serviceMask(Service::RaceAuthority) |
        serviceMask(Service::SharedStorage) | serviceMask(Service::SmuglandHttp) |
        serviceMask(Service::CustomerUpdateAuthority) |
        serviceMask(Service::CommittedPreferences) |
        serviceMask(Service::ProtectedPairingStore) |
        serviceMask(Service::TasterLinkListener),
};

// Taster owns presentation, a gated client link, and local protected pairing
// material plus its locator. Neither local item is Core-owned race/record/SD
// storage. It must not create Core sporting or HTTP services; it has no
// customer-facing update authority. That does not prohibit future device OTA.
inline constexpr RoleManifest kTasterManifest{
    RuntimeRole::Taster,
    serviceMask(Service::TasterLinkClient) | serviceMask(Service::LocalDisplay) |
        serviceMask(Service::LocalTouch) |
        serviceMask(Service::LocalPresentationPrefs) |
        serviceMask(Service::GatedRemoteInteraction) |
        serviceMask(Service::ProtectedPairingStore) |
        serviceMask(Service::TasterLocatorStore),
};

constexpr bool instantiates(const RoleManifest& manifest, Service service) {
  return (manifest.instantiatedServices & serviceMask(service)) != 0;
}

// Design minima, not a measured capacity claim. Four SSE subscribers are
// included within the seven HTTP-client allowance. Twelve is the architectural
// floor: the existing ten sockets plus listener and admitted peer. The actual
// connection-admission check needs one additional transient accept/reject
// descriptor, so thirteen is the bare runtime-admission floor. No unmeasured
// capacity margin is represented here.
struct SocketBudget {
  uint8_t httpClients;
  uint8_t sseClients;
  uint8_t existingBrowserSockets;
  uint8_t tasterListenerSockets;
  uint8_t tasterPeerSockets;
  uint8_t transientAdmissionSockets;
  uint8_t architecturalFloor;
  uint8_t runtimeAdmissionFloor;
};

inline constexpr SocketBudget kCoreSocketBudget{
    7u, 4u, 10u, 1u, 1u, 1u, 12u, 13u,
};

constexpr bool validArchitecture(const SocketBudget& budget) {
  const uint16_t architecturalRequirement =
      static_cast<uint16_t>(budget.existingBrowserSockets) +
      budget.tasterListenerSockets + budget.tasterPeerSockets;
  return budget.httpClients == 7u && budget.sseClients == 4u &&
         budget.sseClients <= budget.httpClients &&
         budget.tasterListenerSockets == 1u && budget.tasterPeerSockets == 1u &&
         budget.architecturalFloor == architecturalRequirement &&
         budget.architecturalFloor == 12u;
}

constexpr bool permitsRuntimeAdmission(const SocketBudget& budget,
                                       uint8_t availableSockets) {
  const uint16_t admissionRequirement =
      static_cast<uint16_t>(budget.architecturalFloor) +
      budget.transientAdmissionSockets;
  return validArchitecture(budget) && budget.transientAdmissionSockets == 1u &&
         budget.runtimeAdmissionFloor == admissionRequirement &&
         availableSockets >= budget.runtimeAdmissionFloor;
}

static_assert(validArchitecture(kCoreSocketBudget),
              "S4 Core architectural socket floor changed");
static_assert(!permitsRuntimeAdmission(kCoreSocketBudget, 12u),
              "twelve sockets cannot admit a Taster connection");
static_assert(permitsRuntimeAdmission(kCoreSocketBudget, 13u),
              "thirteen sockets is the bare Taster-admission floor");

}  // namespace pp::s4
