#pragma once
#include "RoseEngine.h"
#include <cstdint>

namespace arose {
// Persistent full-resolution light canvas: decay old light, then add only new
// simulation samples. No low-resolution rendering and no image upscaling.
class Renderer {
 public:
  static constexpr int Width = 480, Height = 480, TileRows = 4;
  uint32_t prepareUs = 0, haloUs = 0, coreUs = 0, convertUs = 0;
  uint32_t workerUs[2]{};
  bool begin(float framePeriodMs = 30.93f);
  ~Renderer();
  Renderer() = default;
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  void render(uint16_t* output, const Engine& engine, float alpha, bool slowMotion = false);
  void setGlow(unsigned percent); // Fixed halo footprint; palette intensity only.
  void setLuminous(unsigned percent); // Between renders only; no geometry/halo changes.
  void resetCanvas(); // Call only between renders; retains workers and LUTs.
  // UI can overwrite the other buffers while the last light canvas is held.
  void invalidateOutputs() { for (auto& item : previous) for (auto& mask : item.occupied) mask = 0x3fffffff; }
 private:
  struct Sprite { int16_t x, y; uint8_t hue, phase; };
  const uint16_t* canvas = nullptr; // Previous output; caller keeps it alive.
  uint16_t* tiles[2]{}; // Internal SRAM, allocated after the LCD DMA buffers.
  uint16_t colors[256][64]{};
  uint32_t* pointColors = nullptr;
  uint32_t* headColors = nullptr;
  uint16_t* baseColors = nullptr; // Shared 2.5 KiB allocation, made after LCD DMA.
  uint8_t halo[16][16 * 16]{}, core[16][4]{};
  uint16_t fade[65536]{};
  inline __attribute__((always_inline)) uint16_t fadePixel(uint16_t value) const {
    return fade[value];
  }
  Sprite fresh[Engine::PointCount * Engine::MaxTrail]{}, heads[Engine::PointCount]{};
  // Preserve the light underneath display-only heads instead of copying an
  // entire separate canvas to PSRAM. Duplicated coordinates hold equal values.
  struct LightPatch { int16_t x, y; uint16_t value; };
  LightPatch light[Engine::PointCount * 4]{}, nextLight[Engine::PointCount * 4]{};
  int lightCount = 0;
  int freshCount = 0;
  uint32_t active[Height / TileRows]{};
  struct Bounds { uint16_t* buffer = nullptr; uint32_t occupied[Height / TileRows]{}; };
  Bounds previous[3]{};
  uint32_t lastFrame = 0;
  float lastAlpha = 0;
  float nominalFrameMs = 0;
  float decayRemainder = 0;
  struct Job { uint16_t* output; Bounds* bounds; int decayPasses; } job{};
  struct Profile { uint32_t halo, core, copy, elapsed; } workProfile[2]{};
  void* workerTask = nullptr;
  void* callerTask = nullptr;
  static void workerMain(void* context);
  void renderBands(int worker);
  Sprite prepare(const Engine::Point& point) const;
  void drawHalo(const Sprite& sprite, int top, uint16_t* tile);
  void drawCore(const Sprite& sprite, int top, bool head, uint16_t* tile);
};
}
