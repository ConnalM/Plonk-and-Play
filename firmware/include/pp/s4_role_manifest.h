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
  FirmwareUpdate      = 1u << 4,
  TasterLinkListener  = 1u << 5,
  TasterLinkClient    = 1u << 6,
  LocalDisplay        = 1u << 7,
  LocalTouch          = 1u << 8,
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
        serviceMask(Service::FirmwareUpdate) |
        serviceMask(Service::TasterLinkListener) |
        serviceMask(Service::TasterLinkClient) | serviceMask(Service::LocalDisplay) |
        serviceMask(Service::LocalTouch),
};

// Core owns authoritative racing, physical detectors, persistent records and
// SMUGLAND. It may host the Taster link but has no display or touch service.
inline constexpr RoleManifest kCoreManifest{
    RuntimeRole::Core,
    serviceMask(Service::DetectorInput) | serviceMask(Service::RaceAuthority) |
        serviceMask(Service::SharedStorage) | serviceMask(Service::SmuglandHttp) |
        serviceMask(Service::FirmwareUpdate) |
        serviceMask(Service::TasterLinkListener),
};

// Taster owns presentation and the gated client link only. It must not create
// Core sporting, HTTP or record/storage services.
inline constexpr RoleManifest kTasterManifest{
    RuntimeRole::Taster,
    serviceMask(Service::TasterLinkClient) | serviceMask(Service::LocalDisplay) |
        serviceMask(Service::LocalTouch),
};

constexpr bool instantiates(const RoleManifest& manifest, Service service) {
  return (manifest.instantiatedServices & serviceMask(service)) != 0;
}

// Design minima, not a measured capacity claim. Four SSE subscribers are
// included within the seven HTTP-client allowance. The listener and one peer
// are separate link sockets. Twelve is the approved minimum total socket
// budget; no unmeasured margin is represented here.
struct SocketBudget {
  uint8_t httpClients;
  uint8_t sseClients;
  uint8_t tasterListenerSockets;
  uint8_t tasterPeerSockets;
  uint8_t minimumTotalSockets;
};

inline constexpr SocketBudget kCoreSocketBudget{
    7u, 4u, 1u, 1u, 12u,
};

constexpr bool valid(const SocketBudget& budget) {
  const uint16_t directRequirement = static_cast<uint16_t>(budget.httpClients) +
                                     budget.tasterListenerSockets +
                                     budget.tasterPeerSockets;
  return budget.httpClients == 7u && budget.sseClients == 4u &&
         budget.sseClients <= budget.httpClients &&
         budget.tasterListenerSockets == 1u && budget.tasterPeerSockets == 1u &&
         budget.minimumTotalSockets >= directRequirement &&
         budget.minimumTotalSockets >= 12u;
}

static_assert(valid(kCoreSocketBudget), "S4 Core socket design minimum changed");

}  // namespace pp::s4
