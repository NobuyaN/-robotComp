#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Control {
struct Drive { int left; int right; };

struct BridgeDuty { uint32_t in1; uint32_t in2; };
// EN is permanently HIGH. PWM one input, hold the other LOW; both LOW brake.
inline BridgeDuty bridgeDuty(int speed, bool invert) {
  if (speed > 255) speed = 255;
  if (speed < -255) speed = -255;
  if (invert) speed = -speed;
  return {static_cast<uint32_t>(speed > 0 ? speed : 0),
          static_cast<uint32_t>(speed < 0 ? -speed : 0)};
}

// Strict bounded wire format: signed decimal X,Y, each in [-1000,1000].
inline bool parse(const uint8_t* data, size_t length, int& x, int& y) {
  if (length < 3 || length > 11) return false;
  size_t pos = 0;
  auto number = [&](int& value) {
    bool negative = pos < length && data[pos] == '-';
    if (negative) ++pos;
    size_t start = pos;
    value = 0;
    while (pos < length && data[pos] >= '0' && data[pos] <= '9') {
      value = value * 10 + data[pos++] - '0';
      if (value > 1000) return false;
    }
    if (pos == start) return false;
    if (negative) value = -value;
    return true;
  };
  return number(x) && pos < length && data[pos++] == ',' &&
         number(y) && pos == length;
}

inline Drive mix(int x, int y, int deadband = 60) {
  // Positive Y = forward, positive X = right. Normalize to preserve curvature.
  if (x * x + y * y <= deadband * deadband) return {0, 0};
  int left = y + x, right = y - x;
  int peak = left < 0 ? -left : left;
  int magnitude = right < 0 ? -right : right;
  if (magnitude > peak) peak = magnitude;
  if (peak < 1000) peak = 1000;
  return {left * 255 / peak, right * 255 / peak};
}

struct State {
  int x = 0, y = 0;
  uint32_t lastCommand = 0;
  bool armed = false;
  void stop() { x = y = 0; armed = false; }
  void expire(uint32_t now, uint32_t timeout) {
    if (static_cast<uint32_t>(now - lastCommand) >= timeout) stop();
  }
  bool accept(int nextX, int nextY, uint32_t now, uint32_t timeout) {
    expire(now, timeout);
    if (nextX == 0 && nextY == 0) armed = true;
    if (!armed) return false; // Must release after reconnect/timeout.
    x = nextX; y = nextY; lastCommand = now;
    return true;
  }
};
} // namespace Control
