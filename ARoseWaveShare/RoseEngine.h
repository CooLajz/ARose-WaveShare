#pragma once
#include "RoseTables.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arose {
// Port of the ARose desktop engine. Preserve the sequence clock at 50 Hz;
// presentation frequency is independent of the mathematical simulation.
class Engine {
 public:
  static constexpr int PointCount = 96, MaxTrail = 20, Slots = MaxTrail + 1;
  struct Point { float x, y, hue; };
  struct Frame { Point points[PointCount]; };
  // Portable generator state only: no pointers, lookup tables or pixel history.
  struct State {
    uint32_t frame;
    int32_t angle, radiusA, targetA, deltaA, radiusB, targetB, deltaB;
    int32_t frequency, targetFrequency, deltaFrequency;
    int32_t rotation, mode, modeCounter, modeIndex, changed;
    int32_t aIndex, aCounter, bIndex, bCounter, fIndex, fCounter;
  };
  static_assert(sizeof(State) == 88);
  State state() const;
  bool restore(const State& state);
  void begin();
  void reset();
  void step();
  Point interpolated(int age, int count, int point, float alpha) const;
  int available() const { return filled < MaxTrail ? filled : MaxTrail; }
  uint32_t frame() const { return frameNumber; }

 private:
  struct Sequence {
    const int* entries;
    size_t size, index = 0;
    int counter = 0;
    void advance(int current, int& target, int& delta) {
      if (counter != entries[index]) { ++counter; return; }
      target = entries[index + 1];
      delta = entries[index + 2] * (target < current ? -1 : 1);
      index = (index + 3) % size;
      counter = 0;
    }
  };
  static int wrap(int value) { return (value % 1440 + 1440) % 1440; }
  float sine(float angle) const;
  float trig[1441]{};
  Frame history[Slots]{};
  int angles[PointCount]{};
  int head = -1, filled = 0;
  uint32_t frameNumber = 0;
  int radiusA = 0, targetA = 150, deltaA = 1;
  int radiusB = 0, targetB = 0, deltaB = 1;
  int frequency = 0, targetFrequency = 0, deltaFrequency = 1;
  int rotation = 0, mode = 1, modeCounter = 0;
  size_t modeIndex = 0;
  bool changed = true;
  Sequence a{tables::radiusA, sizeof(tables::radiusA) / sizeof(int)};
  Sequence b{tables::radiusB, sizeof(tables::radiusB) / sizeof(int)};
  Sequence f{tables::frequency, sizeof(tables::frequency) / sizeof(int)};
};
}
