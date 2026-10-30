#include "RoseEngine.h"
#include "RoseRenderer.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
void testFeatures(const char* previewPath);

int main(int argc, char** argv) {
  assert(argc == 3);
  testFeatures(argv[2]);
  arose::Engine engine;
  engine.begin();
  std::ifstream reference(argv[1]);
  assert(reference.good());
  int frame, index, checks = 0;
  double x, y, hue;
  while (reference >> frame >> index >> x >> y >> hue) {
    while (engine.frame() < unsigned(frame)) engine.step();
    const auto point = engine.interpolated(0, 1, index, 1);
    assert(std::isfinite(point.x) && std::isfinite(point.y));
    assert(std::abs(point.x - x) < 0.02 && std::abs(point.y - y) < 0.02);
    assert(std::abs(point.hue - hue) < 0.00001);
    ++checks;
  }
  assert(checks == 54);
  arose::Renderer renderer, referenceRenderer;
  assert(renderer.begin() && referenceRenderer.begin());
  std::vector<uint16_t> pixels(480 * 480 + 2, 0xa55a);
  std::vector<uint16_t> other(480 * 480 + 2, 0xa55a), third(480 * 480 + 2, 0xa55a);
  std::vector<uint16_t> expected(480 * 480);
  for (int i = 0; i < 100; ++i) {
    engine.step(); if (i % 2) engine.step();
    const float alpha = float(i % 4) / 4;
    auto& target = i % 3 == 0 ? pixels : (i % 3 == 1 ? other : third);
    renderer.render(target.data() + 1, engine, alpha);
    referenceRenderer.render(expected.data(), engine, alpha);
    assert(std::equal(expected.begin(), expected.end(), target.begin()+1));
    assert(target.front() == 0xa55a && target.back() == 0xa55a);
    assert(target[1] == 0 && target[480 * 480] == 0);
  }
  // Rendering an unchanged simulation must not accumulate light again.
  renderer.render(pixels.data()+1, engine, 0.75f);
  assert(std::equal(expected.begin(), expected.end(), pixels.begin()+1));
  unsigned lit = 0;
  FILE* file = std::fopen(argv[2], "wb");
  assert(file);
  std::fprintf(file, "P6\n480 480\n255\n");
  for (int i = 1; i <= 480 * 480; ++i) {
    const unsigned p = pixels[i];
    if (p) ++lit;
    const unsigned char rgb[] = {static_cast<unsigned char>((p >> 11) * 255 / 31),
      static_cast<unsigned char>(((p >> 5) & 63) * 255 / 63), static_cast<unsigned char>((p & 31) * 255 / 31)};
    std::fwrite(rgb, 1, 3, file);
  }
  std::fclose(file);
  assert(lit > 1000 && lit < 180000);
  std::printf("OK: %d desktop reference points across 30000 steps; bounds, finite coordinates, interpolation and raster (%u lit pixels).\n", checks, lit);
  std::puts("OK: three rotating framebuffers match a single-buffer renderer over 100 temporal frames; repeated presentation is idempotent.");
}
