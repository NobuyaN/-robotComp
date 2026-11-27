#include <cassert>
#include <cstring>
#include <initializer_list>
#include "control.h"

int main() {
  // With fixed-HIGH enables, stop must be LOW/LOW and only one input may have PWM.
  for (bool invert : {false, true}) {
    for (int speed = -300; speed <= 300; ++speed) {
      const auto duty = Control::bridgeDuty(speed, invert);
      assert(duty.in1 <= 255 && duty.in2 <= 255);
      assert(duty.in1 == 0 || duty.in2 == 0);
      if (speed == 0) assert(duty.in1 == 0 && duty.in2 == 0);
      else if ((speed > 0) != invert) assert(duty.in1 > 0 && duty.in2 == 0);
      else assert(duty.in1 == 0 && duty.in2 > 0);
      int magnitude = speed < 0 ? -speed : speed;
      if (magnitude > 255) magnitude = 255;
      assert(duty.in1 + duty.in2 == static_cast<unsigned>(magnitude));
    }
  }
  int x = 0, y = 0;
  auto parse = [&](const char* text) {
    return Control::parse(reinterpret_cast<const uint8_t*>(text), strlen(text), x, y);
  };
  assert(parse("-1000,1000") && x == -1000 && y == 1000);
  assert(parse("0,0") && x == 0 && y == 0);
  for (auto text : {"", "0", "0,", ",0", "1,2,3", "1001,0", "0,-1001",
                    "nan,0", "+1,0", " 0,0", "0,0\n", "999999999,0"})
    assert(!parse(text));
  const uint8_t embedded[] = {'0', ',', '0', 0, '1'};
  assert(!Control::parse(embedded, sizeof(embedded), x, y));
  auto forward = Control::mix(0, 1000); assert(forward.left == 255 && forward.right == 255);
  auto reverse = Control::mix(0, -1000); assert(reverse.left == -255 && reverse.right == -255);
  auto right = Control::mix(1000, 0); assert(right.left == 255 && right.right == -255);
  auto left = Control::mix(-1000, 0); assert(left.left == -255 && left.right == 255);
  auto diagonal = Control::mix(1000, 1000); assert(diagonal.left == 255 && diagonal.right == 0);
  auto half = Control::mix(0, 500); assert(half.left == 127 && half.right == 127);
  auto center = Control::mix(20, 20); assert(center.left == 0 && center.right == 0);
  Control::State state;
  assert(!state.accept(0, 1000, 0, 300)); // Neutral required at startup.
  assert(state.accept(0, 0, 10, 300));
  assert(state.accept(0, 1000, 20, 300));
  state.expire(319, 300); assert(state.y == 1000);
  state.expire(320, 300); assert(state.y == 0 && !state.armed);
  assert(!state.accept(1000, 0, 321, 300)); // Late traffic cannot restart motors.
  assert(state.accept(0, 0, 322, 300));
  assert(state.accept(0, -1000, 323, 300));
  state.stop(); assert(!state.armed && state.y == 0);
  assert(state.accept(0, 0, UINT32_MAX - 100, 300));
  assert(state.accept(0, 1000, UINT32_MAX - 100, 300));
  state.expire(198, 300); assert(state.y == 1000);
  state.expire(199, 300); assert(state.y == 0); // millis() rollover.
}
