# ESP32 RobotCar

PlatformIO / Arduino-ESP32 **3.3.9**, targeting a classic **ESP32-WROOM-32 DevKit**
(`esp32dev`). The ESP32 creates the password-protected `RobotCar` access point,
serves a self-contained responsive joystick page at **http://192.168.4.1**, and
receives control over a WebSocket on port 81. No router, cloud, CDN, filesystem
upload, or internet connection is needed to operate it.

**Motor commands are disabled by default.** Verify the reported wiring and the
connections below before setting `MOTOR_WIRING_CONFIRMED = true` in
[`include/config.h`](include/config.h). ESP32-S3/C3 and WROVER boards require a
different board definition and a reviewed pin map; do not reuse this map blindly.

## Files and configuration

- `platformio.ini`: pinned pioarduino platform and WebSockets library.
- `include/config.h`: AP password, all four GPIO assignments, direction inversion,
  PWM settings, and wiring confirmation switch.
- `include/control.h`: strict command parser, differential mixing, timeout state.
- `include/web_page.h`: embedded HTML/CSS/JavaScript; touch, mouse, and pen input.
- `src/main.cpp`: AP, HTTP/WebSocket servers, and independent motor safety task.
- `tests/control_test.cpp`: host tests for the parser, mixing, and failsafe state.
- `tests/web_test.cjs`: tests the embedded script with simulated input and sockets.

Change `AP_PASSWORD` from `ChangeMe123` to your own 8–63 character password before
use. The AP name is `RobotCar`; its IP and gateway are explicitly set to
`192.168.4.1/24`. Anyone with the AP password can access the controller; it uses
local unencrypted HTTP/WebSockets. One WebSocket controller holds control at a
time; additional controllers receive a busy message. Close the first controller's
page before switching devices.

## Exact wiring: bare L293D DIP-16

Disconnect **all power, including USB**, before wiring or checking continuity.
With the L293D notch facing upward and the chip viewed from above, pin 1 is at
the top left. Count down the left side to pin 8, then up the right side from pin
9 to pin 16. GPIO numbers below are the ESP32's GPIO labels, **not physical
header positions**. Identify the board's actual silkscreen and pinout first.

This version matches your reported layout: the four direction inputs go to
GPIO23, GPIO22, GPIO14 and GPIO27; L293D enable pins 1 and 9 are each pulled HIGH
to the 5 V logic rail through a 10 kΩ resistor. Leave those enable connections
in place. The firmware controls speed by applying PWM to the direction inputs;
**GPIO32 and GPIO33 are not used**. Do not connect any ESP32 GPIO to the 5 V
enable nodes. Verify each enable actually measures a valid HIGH with logic power
on. L293D pin 16 remains on the regulated 5 V logic supply.
`MOTOR_WIRING_CONFIRMED` stays false until the complete wiring has been checked.
GPIO14 is also JTAG MTMS; do not use a JTAG debugger on that pin while it is
connected to the motor driver.

| L293D pin | Signal | Connect to |
| --- | --- | --- |
| 1 | 1,2EN | **5 V through 10 kΩ** (always enabled; no GPIO) |
| 2 | 1A | ESP32 **GPIO23** (left IN1 / forward PWM) |
| 3 | 1Y | Left motor terminal A |
| 4 | GND | Common ground |
| 5 | GND | Common ground |
| 6 | 2Y | Left motor terminal B |
| 7 | 2A | ESP32 **GPIO22** (left IN2 / reverse PWM) |
| 8 | VCC2 | External motor supply positive |
| 9 | 3,4EN | **5 V through 10 kΩ** (always enabled; no GPIO) |
| 10 | 3A | ESP32 **GPIO14** (right IN1 / forward PWM) |
| 11 | 3Y | Right motor terminal A |
| 12 | GND | Common ground |
| 13 | GND | Common ground |
| 14 | 4Y | Right motor terminal B |
| 15 | 4A | ESP32 **GPIO27** (right IN2 / reverse PWM) |
| 16 | VCC1 | Regulated **5 V logic supply**, not ESP32 3V3 |

Common ground joins **ESP32 GND, all four L293D ground pins, logic-supply
negative, and motor-supply negative**. Power the ESP32 through its supported USB
input or a board-approved regulated supply. Do not connect motor-supply positive
to an ESP32 GPIO, 3V3, or USB rail, and do not backfeed USB with an external 5 V
supply. Use a suitable regulated 5 V source for L293D logic.

Place a **100 nF ceramic capacitor** from pin 16 to GND and another from pin 8 to
GND, close to the chip. Add a suitably voltage-rated **100–470 µF bulk capacitor**
across the motor supply near the driver, observing polarity. A 100 nF ceramic
capacitor across each motor's terminals helps suppress brush noise. Keep motor
wiring short and away from the ESP32 antenna.

The enable pullups keep the bridges enabled even while the ESP32 resets or is
disconnected. After setup, the firmware holds all four direction inputs LOW for
stop (electrical braking). It cannot guarantee those input levels before setup,
during reset, or when the ESP32 loses power. For defined idle inputs, add a
separate **10 kΩ pulldown to GND on each of the four direction inputs**; these are
different from the existing enable pullups. Keep motor power off during upload
and reset; for the first test, power the ESP32 and driver logic first, wait for
the controller to be ready, then apply motor power with the wheels lifted.
Turn motor power off before removing ESP32/logic power. A software stop does not
disconnect the motor supply or disable the bridges.

Use motors whose **stall current** fits the L293D's **600 mA per-channel** rating
and the package's thermal limits when both motors run. Do not treat its 1.2 A
short-duration peak rating as continuous capacity. VCC2 must be in the specified
4.5–36 V range and suit the motors; account for the driver's substantial voltage
drop and heat. The L293D contains inductive clamp diodes; the plain L293 is a
different part and requires external protection. At 5 V VCC1, the L293D's 2.3 V
minimum HIGH input threshold accepts ESP32 3.3 V logic; **never feed 5 V into an
ESP32 GPIO**. See the [TI L293/L293D datasheet](https://www.ti.com/lit/ds/symlink/l293.pdf)
for electrical, thermal, and decoupling requirements.

For a driver module, use continuity checks and its schematic to map its labels
to these IC pins. This firmware expects the enables held HIGH and applies PWM
only to direction inputs. Confirm whether the module already includes pullups,
pulldowns, capacitors, or a regulator; never assume its header order or power
jumpers match this table.

## Build and upload

To try the webpage on your computer **without an ESP32**, run:

```sh
python3 tools/preview.py
```

Open **http://127.0.0.1:8765** in your browser. This serves the embedded page with
a simulated connection so you can drag the joystick, adjust speed, and press
STOP. A preview banner identifies it; it sends no commands to hardware. Stop the
server with Ctrl+C. Use `--port 8766` if the default port is already occupied.

### Firmware build

Install VS Code with PlatformIO IDE, or install the PlatformIO CLI in a Python
virtual environment:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install platformio
pio run -e esp32dev
```

The first build downloads the pinned toolchain, Arduino framework, and library.
The [pinned pioarduino release](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.39)
provides Arduino-ESP32 3.3.9. PWM uses the 3.x pin-based
[`ledcAttach(pin, frequency, resolution)` and `ledcWrite(pin, duty)` APIs](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html).
There are no legacy `ledcSetup` or `ledcAttachPin` calls.

Build output is `.pio/build/esp32dev/firmware.bin`. In PlatformIO IDE select
**Build**, or use the CLI command above. Building does not upload anything.

**Only after you authorize flashing**, connect the ESP32 by USB with motor power
disconnected, identify its port, and run:

```sh
pio device list
# Replace the example port with the one from the list.
pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-XXXX
pio device monitor -p /dev/cu.usbserial-XXXX -b 115200
```

On Windows the port is typically `COM3` or similar; on Linux it may be
`/dev/ttyUSB0`. If automatic boot entry fails, follow your board's BOOT/EN
procedure. No hardware has been flashed as part of creating this project.

## Wiring verification and commissioning

1. With all power off, use a meter to verify each table connection, all grounds,
   no shorts between supply rails, chip orientation, and the enable pullups.
   Check direction-input pulldowns if fitted. Correct `config.h` if the actual
   GPIO wiring differs. The compiler
   rejects duplicate pins and pins outside a conservative output whitelist.
2. Keep motor power disconnected. Build and, once authorized, upload with
   `MOTOR_WIRING_CONFIRMED = false`. Join `RobotCar` with your configured password,
   stay connected despite the phone's “no internet” warning, and open
   **http://192.168.4.1** explicitly (HTTP). The page must report motor output
   disabled. It can be tested without energizing motors.
3. Check L293D logic voltage, enable HIGH levels, and all four direction inputs
   LOW at idle with a meter. With power
   removed again, identify and verify the left and right motor output pairs.
   Lift and secure the chassis so all wheels can spin freely. Set
   `MOTOR_WIRING_CONFIRMED = true`, rebuild, and upload only when authorized.
4. Apply motor power. Start with the speed slider at 10%. Hold the joystick
   slightly upward: both wheels should propel the chassis forward. If only one
   wheel is reversed, disconnect power and swap that motor's two leads **or**
   change its `LEFT_INVERT`/`RIGHT_INVERT` setting and rebuild/upload. If sides are
   exchanged, correct the motor pairs or pin assignments. No software can infer
   your actual motor polarity or wiring.
5. Verify downward reverses both wheels, right drives the left wheel forward
   and right wheel backward, and left does the opposite. Diagonals make curved
   turns. Check the full speed range gradually.
6. While driving with wheels lifted, release the joystick, press STOP, switch
   tabs/apps, and turn off the controlling device's Wi-Fi. Each must stop drive.
   To test the watchdog independently of disconnect detection, a test client can
   send `0,0`, send `0,500`, then keep the socket open without sending commands.
   Observe all four direction inputs with a scope/logic analyzer: all must become
   LOW at the 300 ms timeout, within the task's scheduling resolution. The enable
   pins remain HIGH. Repeat with
   the browser showing no responsive network, and confirm that restoring the
   connection does not restart motion until a new joystick press.

If Wi-Fi fails during motor starts, check supply sag, common ground, decoupling,
motor noise, and current limits. If one motor does not move, check its enable
HIGH level, GPIO-to-driver continuity, output pair, and supply voltage before
changing software. Low duty cycles may not overcome a motor's starting friction.

## Control and failsafe behavior

The page sends signed integer **`X,Y` in [-1000,1000]** every **50 ms**, including
neutral heartbeats. Positive Y is forward; positive X is right. The speed slider
scales the transmitted vector. Mixing uses `left = Y + X`, `right = Y - X`, with
joint normalization to the PWM range [-255,255] and a small central deadband.
PWM is 5 kHz, 8 bit on the direction inputs. For positive wheel power, IN1 gets
PWM and IN2 stays LOW; for negative power, IN2 gets PWM and IN1 stays LOW.
Direction inversion swaps these roles. Stop sets both inputs LOW. During the
PWM off phase both inputs are LOW, producing electrical braking rather than
coasting. The enables remain HIGH throughout.

Pointer Events cover phones and computers. Pointer capture, release, cancellation,
lost capture, window blur, hidden pages, and STOP all reset to neutral. Escape or
Space also stops. STOP clears the current gesture; a fresh press can drive again.
The page checks acknowledgment freshness and closes its connection if no reply
arrives for 250 ms, with at most one queued WebSocket frame.

Only valid commands from the active controller refresh the firmware watchdog.
Disconnect, malformed commands, or 300 ms without a valid command clear the
targets. A priority-3 FreeRTOS task checks the timeout and writes the motor GPIOs
every 5 ms, independently of synchronous HTTP/WebSocket handling. Timeout occurs
at elapsed **>=300 ms**, with the next output update normally within one task
period (plus scheduler latency); this is a software failsafe, not a hard real-time
physical stopping guarantee. PWM zero sets all four inputs LOW and electrically
**brakes** the motors; it does not disable the bridges. Mechanical stopping time
depends on inertia, and braking current must stay within motor/driver limits.
Use a physical motor-power switch for an emergency stop.

After timeout or reconnect, nonzero commands are rejected until a fresh `0,0`
neutral command is received. The page resets its gesture on reconnect or rejection.
PWM initialization/task/AP failures leave the initialized direction inputs LOW.
The software confirmation switch gates motion commands, not the externally
powered enable pins. `/status` reports this configured command gate. No position,
speed, or wiring sensors are present.

## Host checks (no hardware)

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/control_test.cpp -o /tmp/robot-control-test
/tmp/robot-control-test
node tests/web_test.cjs
```

Tests cover command validation, forward/reverse/turn mixing, proportional speed,
deadband, timeout boundaries, neutral rearming, and `millis()` rollover. Bridge-duty
tests cover zero/braking, forward/reverse pin selection, inversion, and bounded
PWM values with at most one active input per motor. The Node.js
checks exercise release/cancellation, multiple pointers, STOP, blur/hidden pages,
acknowledgment loss, reconnect, and congestion using the actual embedded script.
These simulated checks do not verify browser rendering or replace the lifted-wheel
and electrical checks above.
