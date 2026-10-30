#include "RoseRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#ifdef ARDUINO
#include <esp_timer.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define ROSE_HOT IRAM_ATTR
static uint32_t profileTime() { return uint32_t(esp_timer_get_time()); }
#else
#define ROSE_HOT
static uint32_t profileTime() { return 0; }
#endif

#if defined(AROSE_HOST_PARALLEL) && !defined(ARDUINO)
#include <thread>
#endif

namespace arose {
namespace {
inline __attribute__((always_inline)) uint16_t pack(uint32_t r, uint32_t g, uint32_t b) {
  return uint16_t((r << 11) | (g << 5) | b);
}
inline __attribute__((always_inline)) uint16_t add(uint16_t a, uint16_t b) {
  return pack(std::min<uint32_t>(31, (a >> 11) + (b >> 11)),
              std::min<uint32_t>(63, ((a >> 5) & 63) + ((b >> 5) & 63)),
              std::min<uint32_t>(31, (a & 31) + (b & 31)));
}
inline __attribute__((always_inline)) uint16_t scale(uint16_t value, unsigned opacity) {
  return pack(((value >> 11) * opacity) >> 8,
              (((value >> 5) & 63) * opacity) >> 8,
              ((value & 31) * opacity) >> 8);
}
float gradient(float r) {
  if (r >= 1) return 0;
  if (r < 0.25f) return 0.85f - 0.5f * r / 0.25f;
  if (r < 0.55f) return 0.35f - 0.28f * (r - 0.25f) / 0.3f;
  return 0.07f * (1 - r) / 0.45f;
}
uint32_t coverage(int x) {
  const int first = std::max(0, x - 8) / 16;
  const int last = std::min(479, x + 9) / 16;
  uint32_t result = 0;
  for (int col = first; col <= last; ++col) result |= uint32_t(1) << col;
  return result;
}
}

Renderer::~Renderer() {
#ifdef ARDUINO
  if (workerTask) vTaskDelete(static_cast<TaskHandle_t>(workerTask));
#endif
  for (auto* tile : tiles) std::free(tile);
  std::free(pointColors);
}

void Renderer::workerMain(void* context) {
#ifdef ARDUINO
  auto& renderer = *static_cast<Renderer*>(context);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    renderer.renderBands(1);
    xTaskNotifyGive(static_cast<TaskHandle_t>(renderer.callerTask));
  }
#endif
}

bool Renderer::begin(float framePeriodMs) {
  if (!std::isfinite(framePeriodMs) || framePeriodMs <= 0) return false;
  nominalFrameMs = framePeriodMs;
  for (auto& tile : tiles) {
#ifdef ARDUINO
    tile = static_cast<uint16_t*>(heap_caps_malloc(Width * TileRows * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
    tile = static_cast<uint16_t*>(std::malloc(Width * TileRows * sizeof(uint16_t)));
#endif
    if (!tile) return false;
  }
#ifdef ARDUINO
  pointColors=static_cast<uint32_t*>(heap_caps_malloc(512*sizeof(uint32_t)+256*sizeof(uint16_t),MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
  pointColors=static_cast<uint32_t*>(std::malloc(512*sizeof(uint32_t)+256*sizeof(uint16_t)));
#endif
  if (!pointColors) return false;
  headColors=pointColors+256;
  baseColors=reinterpret_cast<uint16_t*>(pointColors+512);
  setGlow(80);
  setLuminous(0);
  for (int phase = 0; phase < 16; ++phase) {
    const float dx = float(phase % 4) / 4, dy = float(phase / 4) / 4;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
      const float u = x - 7 - dx, v = y - 7 - dy;
      halo[phase][y * 16 + x] = uint8_t(63 * gradient(std::sqrt(u*u + v*v) / 7.5f) * 0.4f + 0.5f);
    }
    core[phase][0] = uint8_t((1-dx)*(1-dy)*255 + 0.5f);
    core[phase][1] = uint8_t(dx*(1-dy)*255 + 0.5f);
    core[phase][2] = uint8_t((1-dx)*dy*255 + 0.5f);
    core[phase][3] = uint8_t(dx*dy*255 + 0.5f);
  }
  // Keep the same continuous decay when the display refresh rate changes.
  // RGB565 quantization still limits the dimmest part of the trail.
  const unsigned attenuation = unsigned(std::clamp(std::lround(256.f * std::pow(204.f / 256.f, nominalFrameMs / 30.93f)), 1L, 255L));
  for (unsigned value = 0; value < 65536; ++value)
    fade[value] = pack(((value >> 11) * attenuation) >> 8, (((value >> 5) & 63) * attenuation) >> 8, ((value & 31) * attenuation) >> 8);
#ifdef ARDUINO
  TaskHandle_t task = nullptr;
  if (xTaskCreatePinnedToCore(workerMain, "rose-worker", 3072, this, 1, &task, 0) != pdPASS) return false;
  workerTask = task;
#endif
  return true;
}

void Renderer::setGlow(unsigned percent) {
  const float gain=std::min(percent,100u)/80.f;
  for (int h = 0; h < 256; ++h) {
    const float hue = float(h) * 6 / 256;
    const int sector = int(hue);
    const float f = hue - sector, p = 0.05f, q = 1 - 0.95f * f, t = 0.05f + 0.95f * f;
    float r = 1, g = t, b = p;
    switch (sector) {
      case 1: r = q; g = 1; break;
      case 2: r = p; g = 1; b = t; break;
      case 3: r = p; g = q; b = 1; break;
      case 4: r = t; g = p; b = 1; break;
      case 5: r = 1; g = p; b = q; break;
    }
    baseColors[h]=pack(uint32_t(r*31+.5f),uint32_t(g*63+.5f),uint32_t(b*31+.5f));
    for (int level = 0; level < 64; ++level)
      colors[h][level] = pack(std::min<uint32_t>(31u,uint32_t(r * level * 31 / 63 * gain + 0.5f)), std::min<uint32_t>(63u,uint32_t(g * level * gain + 0.5f)), std::min<uint32_t>(31u,uint32_t(b * level * 31 / 63 * gain + 0.5f)));
  }
}

void Renderer::setLuminous(unsigned percent) {
  const float light=std::min(percent,100u)/50.f;
  const float white=.75f*std::min(1.f,light), gain=std::max(1.f,light);
  for (unsigned hue=0; hue<256; ++hue) {
    const auto base=baseColors[hue];
    const auto point=scale(base,200), head=add(scale(base,90),pack(20,40,20));
    // Preserve the existing look exactly at zero. The ESP canvas is already
    // additive; lift point energy continuously and whiten the leading cores.
    const float pointGain=1.f+light*.5f;
    auto channel=[](unsigned value,unsigned maximum,float lift,float multiplier) {
      return unsigned((value+(maximum-value)*lift)*multiplier+.5f);
    };
    auto emission=[](unsigned r,unsigned g,unsigned b) { return (r<<16)|(g<<8)|b; };
    pointColors[hue]=emission(channel(point>>11,31,0,pointGain),channel((point>>5)&63,63,0,pointGain),channel(point&31,31,0,pointGain));
    headColors[hue]=emission(channel(head>>11,31,white,gain),channel((head>>5)&63,63,white,gain),channel(head&31,31,white,gain));
  }
}

Renderer::Sprite Renderer::prepare(const Engine::Point& point) const {
  const float x = 240 + point.x * 0.75f, y = 240 - point.y * 0.75f;
  const int ix = int(x), iy = int(y);
  return {int16_t(ix), int16_t(iy), uint8_t(point.hue * 256),
    uint8_t(std::clamp(int((y-iy)*4),0,3)*4 + std::clamp(int((x-ix)*4),0,3))};
}

void ROSE_HOT Renderer::drawHalo(const Sprite& s, int top, uint16_t* tile) {
  const int left = s.x - 7, start = s.y - 7;
  const int y0 = std::max(top, start), y1 = std::min(top + TileRows, start + 16);
  const int x0 = std::max(0, left), x1 = std::min(Width, left + 16);
  const uint16_t* palette = colors[s.hue];
  for (int y = y0; y < y1; ++y) {
    uint16_t* dst = tile + (y-top) * Width + x0;
    const uint8_t* mask = halo[s.phase] + (y-start)*16 + x0-left;
    for (int x = x0; x < x1; ++x, ++dst, ++mask)
      if (*mask) *dst = add(*dst, palette[*mask]);
  }
}

void ROSE_HOT Renderer::drawCore(const Sprite& s, int top, bool head, uint16_t* tile) {
  const uint32_t color = head ? headColors[s.hue] : pointColors[s.hue];
  for (int y = 0; y < 2; ++y) {
    const int row = s.y + y - top;
    if (row < 0 || row >= TileRows) continue;
    for (int x = 0; x < 2; ++x) {
      const int col = s.x + x;
      if (col < 0 || col >= Width) continue;
      const unsigned opacity = core[s.phase][y*2+x];
      if (opacity) {
        uint16_t& dst = tile[row*Width+col];
        // Keep emission above RGB565 white until coverage has been applied.
        // Clamping the palette first erased most of the 50..100% light gain.
        dst = pack(std::min<unsigned>(31,(dst>>11)+(((color>>16)*opacity)>>8)),
                   std::min<unsigned>(63,((dst>>5)&63)+((((color>>8)&255)*opacity)>>8)),
                   std::min<unsigned>(31,(dst&31)+(((color&255)*opacity)>>8)));
      }
    }
  }
}

void Renderer::resetCanvas() {
  canvas=nullptr; lightCount=0; freshCount=0; lastFrame=0; lastAlpha=0; decayRemainder=0;
  for (auto& mask : active) mask=0;
  invalidateOutputs();
}

void ROSE_HOT Renderer::render(uint16_t* output, const Engine& engine, float alpha, bool slowMotion) {
  uint32_t mark = profileTime();
  haloUs = coreUs = convertUs = 0;
  const float delta = std::max(0.0f, float(engine.frame() - lastFrame) + alpha - lastAlpha);
  int decayPasses;
  if (slowMotion) {
    decayRemainder += delta * 20 / nominalFrameMs;
    decayPasses=std::clamp(int(decayRemainder),0,12);
    decayRemainder -= decayPasses;
  } else {
    decayRemainder=0;
    decayPasses=delta > 0 ? std::clamp(int(delta * 20 / nominalFrameMs + 0.5f), 1, 12) : 0;
  }
  const int count = std::min<int>(engine.available(), engine.frame() - lastFrame);
  freshCount = 0;
  for (int age = 0; age < count; ++age) for (int i = 0; i < Engine::PointCount; ++i)
    fresh[freshCount++] = prepare(engine.interpolated(age, count, i, 1));
  for (int i = 0; i < Engine::PointCount; ++i) heads[i] = prepare(engine.interpolated(0, 1, i, alpha));
  lastFrame = engine.frame(); lastAlpha = alpha;
  Bounds* bounds = nullptr;
  for (auto& item : previous) if (item.buffer == output) bounds = &item;
  if (!bounds) {
    for (auto& item : previous) if (!item.buffer) { bounds = &item; break; }
    if (!bounds) bounds = &previous[0];
    bounds->buffer = output;
    for (auto& mask : bounds->occupied) mask = 0x3fffffff;
  }
  prepareUs = profileTime() - mark;
  job = {output, bounds, decayPasses};
  for (auto& patch : nextLight) patch = {-1, -1, 0};
#ifdef ARDUINO
  callerTask = xTaskGetCurrentTaskHandle();
  xTaskNotifyGive(static_cast<TaskHandle_t>(workerTask));
  renderBands(0);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#elif defined(AROSE_HOST_PARALLEL)
  std::thread worker([this] { renderBands(1); });
  renderBands(0);
  worker.join();
#else
  renderBands(0);
  renderBands(1);
#endif
  haloUs = workProfile[0].halo + workProfile[1].halo;
  coreUs = workProfile[0].core + workProfile[1].core;
  convertUs = workProfile[0].copy + workProfile[1].copy;
  workerUs[0] = workProfile[0].elapsed;
  workerUs[1] = workProfile[1].elapsed;
  std::memcpy(light, nextLight, sizeof(light));
  lightCount = Engine::PointCount * 4;
  canvas = output;
}

void ROSE_HOT Renderer::renderBands(int worker) {
  const uint32_t started = profileTime();
  uint16_t* tile = tiles[worker];
  const auto [output, bounds, decayPasses] = job;
  auto& profile = workProfile[worker];
  profile = {};
  uint32_t mark;
  for (int top = 0; top < Height; top += TileRows) {
    // Core 0 also services LCD interrupts; give it one third of the bands.
    if ((top / TileRows % 3 == 0) != (worker == 1)) continue;
    const int band = top / TileRows;
    std::memset(tile, 0, Width * TileRows * sizeof(uint16_t));
    uint32_t occupied = active[band];
    mark = profileTime();
    for (int col = 0; col < Width/16;) {
      if (!(active[band] & (uint32_t(1) << col))) { ++col; continue; }
      const int left = col*16;
      do { ++col; } while (col < Width/16 && (active[band] & (uint32_t(1) << col)));
      const int length = col*16-left;
      for (int row = 0; row < TileRows; ++row) {
        uint16_t* dst = tile + row*Width + left;
        const uint16_t* src = canvas + (top+row)*Width + left;
        if (decayPasses == 1) {
          for (int x = 0; x < length; ++x) dst[x] = fadePixel(src[x]);
        } else if (decayPasses == 0) {
          std::memcpy(dst, src, length*sizeof(uint16_t));
        } else {
          for (int x = 0; x < length; ++x) {
            uint16_t value = src[x];
            for (int n = 0; n < decayPasses; ++n) value = fadePixel(value);
            dst[x] = value;
          }
        }
      }
    }
    // Remove the previous display-only heads using their saved underlying
    // values, faded identically to the rest of the old light.
    for (int i = 0; i < lightCount; ++i) {
      const auto& patch = light[i];
      if (patch.y < top || patch.y >= top+TileRows) continue;
      uint16_t value = patch.value;
      for (int n = 0; n < decayPasses; ++n) value = fadePixel(value);
      tile[(patch.y-top)*Width+patch.x] = value;
    }
    for (int i = 0; i < freshCount; ++i) {
      if (fresh[i].y+9 <= top || fresh[i].y-7 >= top+TileRows) continue;
      occupied |= coverage(fresh[i].x);
      drawHalo(fresh[i], top, tile);
      drawCore(fresh[i], top, false, tile);
    }
    uint32_t nextActive = 0;
    // Track persistent light without writing a second full image to PSRAM.
    for (int col = 0; col < Width/16;) {
      if (!(occupied & (uint32_t(1) << col))) { ++col; continue; }
      const int first = col;
      do { ++col; } while (col < Width/16 && (occupied & (uint32_t(1) << col)));
      const int left = first*16;
      for (int row = 0; row < TileRows; ++row) {
        const uint16_t* src = tile + row*Width + left;
        for (int block = first; block < col; ++block) {
          const uint32_t bit = uint32_t(1) << block;
          if (nextActive & bit) continue;
          const uint16_t* pixels = src + (block-first)*16;
          uint16_t nonzero = 0;
          for (int x = 0; x < 16; ++x) nonzero |= pixels[x];
          if (nonzero) nextActive |= bit;
        }
      }
    }
    active[band] = nextActive;
    profile.halo += profileTime() - mark; mark = profileTime();
    // Capture every head footprint before drawing any head, including
    // overlapping ones. Otherwise overlapping highlights would enter history.
    for (int i = 0; i < Engine::PointCount; ++i) for (int y = 0; y < 2; ++y) {
      const auto& head = heads[i];
      const int row = head.y+y;
      if (row < top || row >= top+TileRows) continue;
      for (int x = 0; x < 2; ++x) {
        const int col = head.x+x;
        if (col < 0 || col >= Width) continue;
        nextLight[i*4+y*2+x] = {int16_t(col), int16_t(row), tile[(row-top)*Width+col]};
      }
    }
    for (const auto& head : heads) if (head.y+2 > top && head.y < top+TileRows) {
      drawCore(head, top, true, tile);
      occupied |= coverage(head.x);
    }
    profile.core += profileTime() - mark; mark = profileTime();
    const uint32_t dirty = occupied | bounds->occupied[band];
    bounds->occupied[band] = occupied;
    for (int col = 0; col < Width/16;) {
      if (!(dirty & (uint32_t(1) << col))) { ++col; continue; }
      const int left = col*16;
      do { ++col; } while (col < Width/16 && (dirty & (uint32_t(1) << col)));
      const int right = col*16;
      for (int row = 0; row < TileRows; ++row) {
        std::memcpy(output + (top+row)*Width + left, tile + row*Width + left, (right-left)*sizeof(uint16_t));
      }
    }
    profile.copy += profileTime() - mark;
  }
  profile.elapsed = profileTime() - started;
}
}
