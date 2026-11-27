#pragma once
#include <stdint.h>

namespace Config {
constexpr char AP_SSID[] = "RobotCar";
constexpr char AP_PASSWORD[] = "natsuyasumi"; // Change before use: 8–63 characters.
static_assert(sizeof(AP_PASSWORD) >= 9 && sizeof(AP_PASSWORD) <= 64,
              "AP password must be 8–63 characters");

// Classic ESP32-WROOM DevKit only. GPIO numbers, NOT header positions.
// Keep false until the unpowered wiring and lifted-wheel checks in README.md.
constexpr bool MOTOR_WIRING_CONFIRMED = true;
// Existing layout: L293D pins 1/9 are pulled HIGH to 5 V through 10 kΩ.
// They have no ESP32 GPIO connection. PWM is applied to the four direction pins.
constexpr uint8_t LEFT_IN1 = 23; // L293D pin 2 (1A), reported wiring
constexpr uint8_t LEFT_IN2 = 22; // L293D pin 7 (2A), reported wiring
constexpr uint8_t RIGHT_IN1 = 14;// L293D pin 10 (3A), reported wiring; also JTAG MTMS
constexpr uint8_t RIGHT_IN2 = 27;// L293D pin 15 (4A), reported wiring
constexpr bool LEFT_INVERT = false;
constexpr bool RIGHT_INVERT = false;
constexpr uint32_t PWM_HZ = 5000;
constexpr uint8_t PWM_BITS = 8;
static_assert(PWM_BITS == 8, "Motor mixing and bridge duty use 8-bit PWM");
constexpr uint32_t COMMAND_TIMEOUT_MS = 300;
constexpr int JOYSTICK_MAX = 1000;
constexpr int DEADBAND = 60;

constexpr uint8_t MOTOR_PINS[] = {
    LEFT_IN1, LEFT_IN2, RIGHT_IN1, RIGHT_IN2};
constexpr bool validPins() {
  for (unsigned i = 0; i < sizeof(MOTOR_PINS); ++i) {
    const auto pin = MOTOR_PINS[i];
    // Conservative whitelist avoids boot straps, flash, UART and input-only pins.
    // GPIO14 is an output-capable JTAG pin; do not use JTAG while it drives a motor.
    if (!(pin == 4 || pin == 14 || (pin >= 16 && pin <= 19) || (pin >= 21 && pin <= 23) ||
          (pin >= 25 && pin <= 27) || pin == 32 || pin == 33)) return false;
    for (unsigned j = 0; j < i; ++j)
      if (pin == MOTOR_PINS[j]) return false;
  }
  return true;
}
static_assert(validPins(), "Use distinct, safe ESP32 output GPIOs");
} // namespace Config
