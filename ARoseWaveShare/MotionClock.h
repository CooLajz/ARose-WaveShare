#pragma once
#include <algorithm>
#include <cstdint>

namespace arose {
// Integrate a two-second smoothstep speed ramp exactly, independent of FPS.
class MotionClock {
 public:
  static constexpr double DurationUs = 2000000.0;
  void reset(double speed = 1) { from_ = target_ = std::clamp(speed, 0.0, 1.0); elapsed_ = DurationUs; }
  double speed() const {
    const double t = std::min(1.0, elapsed_ / DurationUs);
    return from_ + (target_ - from_) * t*t*(3-2*t);
  }
  bool transitioning() const { return elapsed_ < DurationUs; }
  bool stopped() const { return !transitioning() && target_ == 0; }
  void setPaused(bool paused) {
    const double target = paused ? 0 : 1;
    if (target == target_) return;
    from_ = speed(); target_ = target; elapsed_ = 0;
  }
  double advance(int64_t deltaUs) {
    double remaining = std::max<int64_t>(0, deltaUs), result = 0;
    if (transitioning()) {
      const double ramp = std::min(remaining, DurationUs-elapsed_);
      const double a = elapsed_/DurationUs, b = (elapsed_+ramp)/DurationUs;
      result = from_*ramp + (target_-from_)*DurationUs*(integral(b)-integral(a));
      elapsed_ += ramp; remaining -= ramp;
    }
    // Avoid a negative sub-nanosecond remainder from cancellation at rest.
    return std::max(0.0, result + target_*remaining);
  }
 private:
  static double integral(double t) { return t*t*t - .5*t*t*t*t; }
  double from_ = 1, target_ = 1, elapsed_ = DurationUs;
};
}
