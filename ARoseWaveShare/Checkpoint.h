#pragma once
#include "RoseEngine.h"
#include <cstddef>
#include <cstring>

namespace arose {
struct Checkpoint {
  static constexpr uint32_t SaveEnabled = 1u << 8, PositionPresent = 1u << 9;
  uint32_t magic = 0x41524f53, version = 4;
  Engine::State state{};
  // Single supported format: backlight, flags, luminosity and glow.
  uint32_t settings = 100 | SaveEnabled | (50u << 16) | (80u << 24), crc = 0;
  uint8_t brightness() const { return uint8_t(settings & 255); }
  uint8_t luminous() const { return uint8_t(settings >> 16); }
  uint8_t glow() const { return uint8_t(settings >> 24); }
  bool savingEnabled() const { return settings & SaveEnabled; }
  bool hasPosition() const { return settings & PositionPresent; }
  static Checkpoint make(const Engine::State* position, uint8_t brightness, bool enabled, uint8_t luminous = 50, uint8_t glow = 80) {
    Checkpoint record;
    record.settings = brightness | (enabled ? SaveEnabled : 0) | (uint32_t(luminous) << 16) | (uint32_t(glow) << 24);
    if (enabled && position) { record.state = *position; record.settings |= PositionPresent; }
    record.seal();
    return record;
  }
  static uint32_t checksum(const void* data, size_t size) {
    uint32_t crc = 0xffffffff;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i=0; i<size; ++i) {
      crc ^= bytes[i];
      for (int bit=0; bit<8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u-(crc&1)));
    }
    return ~crc;
  }
  void seal() { crc = checksum(this, offsetof(Checkpoint, crc)); }
  bool valid() const {
    if (magic != 0x41524f53 || version != 4 || brightness() < 5 || brightness() > 100 ||
        crc != checksum(this, offsetof(Checkpoint, crc))) return false;
    if (luminous() > 100 || glow() > 100 || (settings & ~(255u | SaveEnabled | PositionPresent | 0xffff0000u))) return false;
    if (hasPosition() && !savingEnabled()) return false;
    const Engine::State empty{};
    return hasPosition() || std::memcmp(&state, &empty, sizeof(state)) == 0;
  }
};
static_assert(sizeof(Checkpoint) == 104);
}
