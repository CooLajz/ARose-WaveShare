#include "RoseEngine.h"
#include <algorithm>

namespace arose {
Engine::State Engine::state() const {
  return {frameNumber, angles[0], radiusA, targetA, deltaA, radiusB, targetB, deltaB,
    frequency, targetFrequency, deltaFrequency, rotation, mode, modeCounter, int32_t(modeIndex), int32_t(changed),
    int32_t(a.index), a.counter, int32_t(b.index), b.counter, int32_t(f.index), f.counter};
}

bool Engine::restore(const State& s) {
  auto sequenceValid = [](const Sequence& seq, int index, int counter) {
    return index >= 0 && size_t(index) < seq.size - 2 && index % 3 == 0 &&
      counter >= 0 && counter <= seq.entries[index];
  };
  if (s.angle < 0 || s.angle >= 1440 || s.rotation < 0 || s.rotation >= 1440 ||
      s.mode < 0 || s.mode > 2 || s.changed < 0 || s.changed > 1 ||
      s.modeIndex < 0 || s.modeIndex % 2 || size_t(s.modeIndex) >= sizeof(tables::rotation)/sizeof(int) - 1 ||
      s.modeCounter < 0 || s.modeCounter > tables::rotation[s.modeIndex] ||
      s.radiusA < -32 || s.radiusA > 232 || s.targetA < 0 || s.targetA > 200 || std::abs(int64_t(s.deltaA)) > 32 ||
      s.radiusB < -32 || s.radiusB > 132 || s.targetB < 0 || s.targetB > 100 || std::abs(int64_t(s.deltaB)) > 32 ||
      s.frequency < -64 || s.frequency > 20064 || s.targetFrequency < 0 || s.targetFrequency > 20000 || std::abs(int64_t(s.deltaFrequency)) > 64 ||
      !sequenceValid(a, s.aIndex, s.aCounter) || !sequenceValid(b, s.bIndex, s.bCounter) || !sequenceValid(f, s.fIndex, s.fCounter)) return false;
  frameNumber = s.frame;
  for (int i = 0; i < PointCount; ++i) angles[i] = wrap(s.angle + i*15);
  radiusA=s.radiusA; targetA=s.targetA; deltaA=s.deltaA;
  radiusB=s.radiusB; targetB=s.targetB; deltaB=s.deltaB;
  frequency=s.frequency; targetFrequency=s.targetFrequency; deltaFrequency=s.deltaFrequency;
  rotation=s.rotation; mode=s.mode; modeCounter=s.modeCounter; modeIndex=s.modeIndex; changed=s.changed;
  a.index=s.aIndex; a.counter=s.aCounter; b.index=s.bIndex; b.counter=s.bCounter; f.index=s.fIndex; f.counter=s.fCounter;
  head=-1; filled=0;
  return true;
}

void Engine::begin() {
  for (int i = 0; i <= 1440; ++i) trig[i] = std::sin(float(i) * (3.14159265358979323846f / 720));
  reset();
}

void Engine::reset() {
  for (int i = 0; i < PointCount; ++i) angles[i] = i * 15;
  head=-1; filled=0; frameNumber=0;
  radiusA=0; targetA=150; deltaA=1;
  radiusB=0; targetB=0; deltaB=1;
  frequency=0; targetFrequency=0; deltaFrequency=1;
  rotation=0; mode=1; modeCounter=0; modeIndex=0; changed=true;
  a.index=b.index=f.index=0; a.counter=b.counter=f.counter=0;
}

float Engine::sine(float angle) const {
  angle = std::fmod(angle, 1440.0f);
  if (angle < 0) angle += 1440;
  const int index = int(angle);
  return trig[index] + (trig[index + 1] - trig[index]) * (angle - index);
}

void Engine::step() {
  head = (head + 1) % Slots;
  filled = std::min(Slots, filled + 1);
  const float sr = sine(float(wrap(2 * rotation)));
  const float cr = sine(float(wrap(2 * rotation)) + 360);
  for (int i = 0; i < PointCount; ++i) {
    const float theta = float(angles[i]);
    const float phi = float(frequency) * float(i * 15) / 200;
    const float u = radiusA * sine(theta) + radiusB * sine(phi);
    const float v = radiusA * sine(theta + 360) + radiusB * sine(phi + 360);
    // Reduce the frame before converting to float to avoid long-run hue loss.
    const float hue = float(i) / PointCount + float(frameNumber % 1200) / 1200;
    history[head].points[i] = {v * cr - u * sr, v * sr + u * cr, hue - std::floor(hue)};
  }
  constexpr int speed = 3;
  if (mode == 1) rotation = wrap(rotation + speed);
  if (mode == 2 || changed)
    for (int& angle : angles) angle = wrap(angle - speed);
  changed = true;
  if (frequency != targetFrequency) { frequency += deltaFrequency; changed = false; }
  if (radiusA != targetA) { radiusA += deltaA; changed = false; }
  if (radiusB != targetB) { radiusB += deltaB; changed = false; }
  a.advance(radiusA, targetA, deltaA);
  b.advance(radiusB, targetB, deltaB);
  f.advance(frequency, targetFrequency, deltaFrequency);
  if (modeCounter == tables::rotation[modeIndex]) {
    mode = tables::rotation[modeIndex + 1];
    modeIndex = (modeIndex + 2) % (sizeof(tables::rotation) / sizeof(int));
    modeCounter = 0;
  } else ++modeCounter;
  ++frameNumber;
}

Engine::Point Engine::interpolated(int age, int count, int point, float alpha) const {
  const int slot = (head - (count - 1 - age) + Slots) % Slots;
  const Point& current = history[slot].points[point];
  if (filled <= count) return current;
  const Point& previous = history[(slot + Slots - 1) % Slots].points[point];
  float hueDelta = current.hue - previous.hue;
  if (hueDelta < -0.5f) hueDelta += 1;
  if (hueDelta > 0.5f) hueDelta -= 1;
  float hue = previous.hue + hueDelta * alpha;
  if (hue >= 1) hue -= 1;
  if (hue < 0) hue += 1;
  return {previous.x + (current.x - previous.x) * alpha,
          previous.y + (current.y - previous.y) * alpha, hue};
}
}
