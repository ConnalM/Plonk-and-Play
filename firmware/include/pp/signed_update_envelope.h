#pragma once

// Versioned P&P firmware package boundary. The package carries a canonical
// manifest followed by the exact ESP image bytes. The manifest signature binds
// product, board, version, image size and image SHA-256 before OTA selection.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#if defined(PP_UPDATE_TEST_KEY)
#define PP_UPDATE_TRUSTED_PUBLIC_KEY_HEX "3059301306072a8648ce3d020106082a8648ce3d03010703420004ff655c2c04fe764d87be2bf00ad7ebb401f22f2811dbada7ccf65959276bf50d9787a66410608402c1cf45d0869321f8e4e56a84e3e098f87e6635bb6d485213"
#elif !defined(PP_UPDATE_PUBLIC_KEY_HEX)
#error "PP_UPDATE_PUBLIC_KEY_HEX must provide the production P&P verification key"
#else
#define PP_UPDATE_TRUSTED_PUBLIC_KEY_HEX PP_UPDATE_PUBLIC_KEY_HEX
#endif
#endif

namespace pp {

enum class SignedEnvelopeResult : uint8_t {
  Ok,
  UnsupportedVersion,
  Malformed,
  Truncated,
  SizeMismatch,
  ProductMismatch,
  BoardMismatch,
  DigestMismatch,
  Unsigned,
  InvalidSignature,
  VerificationUnavailable,
};

struct SignedEnvelopeIdentity {
  char product[33]{};
  char board[33]{};
  char buildId[65]{};
  uint32_t version = 0;
  size_t imageSize = 0;
};

class SignedFirmwareEnvelopeVerifier final {
 public:
  using BeginPayload = bool (*)(void*, size_t);
  using WritePayload = bool (*)(void*, const uint8_t*, size_t);

  SignedEnvelopeResult begin(size_t packageSize) {
    reset();
    packageSize_ = packageSize;
    if (packageSize < PrefixBytes || packageSize > MaxPackageBytes) return SignedEnvelopeResult::Malformed;
    active_ = true;
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    mbedtls_sha256_init(&payloadSha_);
    mbedtls_sha256_starts(&payloadSha_, 0);
#endif
    return SignedEnvelopeResult::Ok;
  }

  SignedEnvelopeResult consume(const uint8_t* data, size_t size, void* sink,
                               BeginPayload beginPayload, WritePayload writePayload) {
    if (!active_ || !data || !size || !beginPayload || !writePayload) return SignedEnvelopeResult::Malformed;
    size_t offset = 0;
    while (offset < size) {
      if (headerBytes_ < PrefixBytes) {
        const size_t n = (PrefixBytes - headerBytes_) < (size - offset) ? (PrefixBytes - headerBytes_) : (size - offset);
        memcpy(header_ + headerBytes_, data + offset, n);
        headerBytes_ += n;
        offset += n;
        if (headerBytes_ < PrefixBytes) return SignedEnvelopeResult::Ok;
        const SignedEnvelopeResult parsed = parsePrefix();
        if (parsed != SignedEnvelopeResult::Ok) return parsed;
      }
      if (!headerComplete_) {
        const size_t n = (headerSize_ - headerBytes_) < (size - offset) ? (headerSize_ - headerBytes_) : (size - offset);
        memcpy(header_ + headerBytes_, data + offset, n);
        headerBytes_ += n;
        offset += n;
        if (headerBytes_ < headerSize_) return SignedEnvelopeResult::Ok;
        const SignedEnvelopeResult parsed = parseHeader();
        if (parsed != SignedEnvelopeResult::Ok) return parsed;
        if (!beginPayload(sink, imageSize_)) return SignedEnvelopeResult::Malformed;
        headerComplete_ = true;
      }
      if (offset < size) {
        const size_t n = (size - offset) < payloadRemaining_ ? (size - offset) : payloadRemaining_;
        if (n && !writePayload(sink, data + offset, n)) return SignedEnvelopeResult::Malformed;
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
        if (n) mbedtls_sha256_update(&payloadSha_, data + offset, n);
#endif
        payloadRemaining_ -= n;
        payloadBytes_ += n;
        offset += n;
        if (offset < size && payloadRemaining_ == 0) return SignedEnvelopeResult::SizeMismatch;
      }
    }
    return SignedEnvelopeResult::Ok;
  }

  SignedEnvelopeResult finish(SignedEnvelopeIdentity& identity) {
    if (!active_ || !headerComplete_) return SignedEnvelopeResult::Truncated;
    if (payloadRemaining_ != 0 || headerSize_ + imageSize_ != packageSize_) return SignedEnvelopeResult::Truncated;
    if (!digestMatches()) return SignedEnvelopeResult::DigestMismatch;
    const SignedEnvelopeResult signature = verifySignature();
    if (signature != SignedEnvelopeResult::Ok) return signature;
    memcpy(identity.product, product_, sizeof(product_));
    memcpy(identity.board, board_, sizeof(board_));
    memcpy(identity.buildId, buildId_, sizeof(buildId_));
    identity.version = version_;
    identity.imageSize = imageSize_;
    active_ = false;
    return SignedEnvelopeResult::Ok;
  }

  size_t imageSize() const { return imageSize_; }
  void reset() {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    mbedtls_sha256_free(&payloadSha_);
#endif
    active_ = false; packageSize_ = headerBytes_ = headerSize_ = imageSize_ = payloadRemaining_ = payloadBytes_ = 0; headerComplete_ = false;
    memset(header_, 0, sizeof(header_)); memset(product_, 0, sizeof(product_)); memset(board_, 0, sizeof(board_)); memset(buildId_, 0, sizeof(buildId_));
  }

 private:
  static constexpr size_t PrefixBytes = 52;
  static constexpr size_t MaxHeaderBytes = 256;
  static constexpr size_t MaxPackageBytes = 0x640000 + MaxHeaderBytes;
  uint8_t header_[MaxHeaderBytes]{};
  char product_[33]{}, board_[33]{}, buildId_[65]{};
  size_t packageSize_ = 0, headerBytes_ = 0, headerSize_ = 0, imageSize_ = 0, payloadRemaining_ = 0, payloadBytes_ = 0;
  uint32_t version_ = 0;
  uint16_t signatureSize_ = 0;
  uint8_t imageDigest_[32]{};
  bool active_ = false, headerComplete_ = false;
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
  mbedtls_sha256_context payloadSha_{};
#endif

  static uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
  static uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
  static int hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
  static size_t decodeHex(const char* text, uint8_t* out, size_t cap) {
    const size_t length = strlen(text); if ((length & 1u) || length / 2 > cap) return 0;
    for (size_t i = 0; i < length / 2; ++i) { const int a = hex(text[i * 2]), b = hex(text[i * 2 + 1]); if (a < 0 || b < 0) return 0; out[i] = uint8_t((a << 4) | b); }
    return length / 2;
  }
  SignedEnvelopeResult parsePrefix() {
    if (memcmp(header_, "PPU1", 4) != 0) return SignedEnvelopeResult::Malformed;
    if (header_[4] != 1) return SignedEnvelopeResult::UnsupportedVersion;
    headerSize_ = u16(header_ + 5);
    const size_t productLength = header_[7], boardLength = header_[8], buildLength = header_[9];
    signatureSize_ = u16(header_ + 50);
    if (headerSize_ < PrefixBytes || headerSize_ > MaxHeaderBytes || !productLength || !boardLength || !buildLength || productLength > 32 || boardLength > 32 || buildLength > 64 || !signatureSize_ || signatureSize_ > 80) return SignedEnvelopeResult::Malformed;
    if (PrefixBytes + productLength + boardLength + buildLength + signatureSize_ != headerSize_) return SignedEnvelopeResult::Malformed;
    version_ = u32(header_ + 10); imageSize_ = u32(header_ + 14);
    if (!imageSize_ || headerSize_ + imageSize_ != packageSize_) return SignedEnvelopeResult::SizeMismatch;
    return SignedEnvelopeResult::Ok;
  }
  SignedEnvelopeResult parseHeader() {
    const size_t productLength = header_[7], boardLength = header_[8], buildLength = header_[9];
    size_t offset = PrefixBytes;
    memcpy(product_, header_ + offset, productLength); product_[productLength] = 0; offset += productLength;
    memcpy(board_, header_ + offset, boardLength); board_[boardLength] = 0; offset += boardLength;
    memcpy(buildId_, header_ + offset, buildLength); buildId_[buildLength] = 0; offset += buildLength;
    memcpy(imageDigest_, header_ + 18, sizeof(imageDigest_));
    payloadRemaining_ = imageSize_; payloadBytes_ = 0;
    return SignedEnvelopeResult::Ok;
  }
  bool digestMatches() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    mbedtls_sha256_context copy = payloadSha_;
    uint8_t digest[32]{};
    mbedtls_sha256_finish(&copy, digest);
    return memcmp(digest, imageDigest_, sizeof(digest)) == 0;
#else
    return false;
#endif
  }
  SignedEnvelopeResult verifySignature() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    if (!signatureSize_) return SignedEnvelopeResult::Unsigned;
    uint8_t key[256]{};
    const size_t keyBytes = decodeHex(PP_UPDATE_TRUSTED_PUBLIC_KEY_HEX, key, sizeof(key));
    if (!keyBytes) return SignedEnvelopeResult::VerificationUnavailable;
    const size_t signatureOffset = headerSize_ - signatureSize_;
    uint8_t manifestDigest[32]{};
    mbedtls_sha256_context manifestSha{};
    mbedtls_sha256_init(&manifestSha);
    mbedtls_sha256_starts(&manifestSha, 0);
    mbedtls_sha256_update(&manifestSha, header_, signatureOffset);
    mbedtls_sha256_finish(&manifestSha, manifestDigest);
    mbedtls_sha256_free(&manifestSha);
    mbedtls_pk_context pk{};
    mbedtls_pk_init(&pk);
    const int parsed = mbedtls_pk_parse_public_key(&pk, key, keyBytes);
    const int verified = parsed == 0 ? mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, manifestDigest, sizeof(manifestDigest), header_ + signatureOffset, signatureSize_) : -1;
    mbedtls_pk_free(&pk);
    return verified == 0 ? SignedEnvelopeResult::Ok : SignedEnvelopeResult::InvalidSignature;
#else
    return SignedEnvelopeResult::VerificationUnavailable;
#endif
  }
};

}  // namespace pp
