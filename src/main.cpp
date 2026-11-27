#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_arduino_version.h>
#include "config.h"
#include "control.h"
#include "web_page.h"

#if ESP_ARDUINO_VERSION_MAJOR != 3
#error "This project requires Arduino-ESP32 3.x"
#endif

WebServer http(80);
WebSocketsServer socketServer(81);
portMUX_TYPE controlMux = portMUX_INITIALIZER_UNLOCKED;
Control::State command;
int owner = -1; // Accessed only by the Arduino networking loop.
bool pwmReady = false;

void stopCommand() {
  portENTER_CRITICAL(&controlMux);
  command.stop();
  portEXIT_CRITICAL(&controlMux);
}

void motor(uint8_t in1, uint8_t in2, int speed, bool invert) {
  const auto duty = Control::bridgeDuty(speed, invert);
  // Return both inputs LOW before selecting the active PWM input.
  // EN is pulled HIGH externally: both LOW electrically brake, not coast.
  ledcWrite(in1, 0);
  ledcWrite(in2, 0);
  if (duty.in1) ledcWrite(in1, duty.in1);
  if (duty.in2) ledcWrite(in2, duty.in2);
}

void motorTask(void*) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    portENTER_CRITICAL(&controlMux);
    command.expire(millis(), Config::COMMAND_TIMEOUT_MS);
    const auto drive = Control::mix(command.x, command.y, Config::DEADBAND);
    portEXIT_CRITICAL(&controlMux);
    // This task alone writes the driver after setup; it never waits on networking.
    motor(Config::LEFT_IN1, Config::LEFT_IN2,
          Config::MOTOR_WIRING_CONFIRMED ? drive.left : 0, Config::LEFT_INVERT);
    motor(Config::RIGHT_IN1, Config::RIGHT_IN2,
          Config::MOTOR_WIRING_CONFIRMED ? drive.right : 0, Config::RIGHT_INVERT);
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
  }
}

void onSocket(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    if (owner >= 0) {
      socketServer.sendTXT(client, "busy");
      socketServer.disconnect(client);
      return;
    }
    owner = client;
    stopCommand();
    socketServer.sendTXT(client, Config::MOTOR_WIRING_CONFIRMED ? "ready" : "disabled");
  } else if (type == WStype_DISCONNECTED && owner == client) {
    stopCommand();
    owner = -1;
  } else if (owner == client && type == WStype_TEXT) {
    int x, y;
    if (!Control::parse(payload, length, x, y)) {
      stopCommand(); // Invalid frames never refresh the watchdog.
      socketServer.sendTXT(client, "release");
      return;
    }
    portENTER_CRITICAL(&controlMux);
    bool accepted = command.accept(x, y, millis(), Config::COMMAND_TIMEOUT_MS);
    portEXIT_CRITICAL(&controlMux);
    socketServer.sendTXT(client, accepted ? "ok" : "release");
  } else if (owner == client && (type == WStype_BIN || type == WStype_FRAGMENT_TEXT_START ||
             type == WStype_FRAGMENT_BIN_START || type == WStype_FRAGMENT ||
             type == WStype_FRAGMENT_FIN)) {
    stopCommand();
    socketServer.sendTXT(client, "release");
  }
}

void setup() {
  Serial.begin(115200);
  // EN is fixed HIGH; initialize all four direction inputs LOW (electrical brake).
  // Firmware cannot control these pins before setup or while the ESP32 is off.
  for (auto pin : Config::MOTOR_PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  bool attached[sizeof(Config::MOTOR_PINS)] = {};
  pwmReady = true;
  for (unsigned i = 0; i < sizeof(Config::MOTOR_PINS); ++i) {
    const auto pin = Config::MOTOR_PINS[i];
    attached[i] = ledcAttach(pin, Config::PWM_HZ, Config::PWM_BITS);
    if (attached[i]) ledcWrite(pin, 0);
    else pwmReady = false;
  }
  if (!pwmReady) {
    for (unsigned i = 0; i < sizeof(Config::MOTOR_PINS); ++i) {
      const auto pin = Config::MOTOR_PINS[i];
      if (attached[i]) ledcDetach(pin);
      digitalWrite(pin, LOW);
      pinMode(pin, OUTPUT);
    }
    Serial.println("PWM setup failed; motor inputs LOW (brake). Reset to retry.");
    return;
  }
  if (xTaskCreate(motorTask, "motor-safety", 3072, nullptr, 3, nullptr) != pdPASS) {
    pwmReady = false;
    Serial.println("Motor task failed; motors disabled. Reset to retry.");
    return;
  }
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  const IPAddress ip(192, 168, 4, 1);
  if (!WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0)) ||
      !WiFi.softAP(Config::AP_SSID, Config::AP_PASSWORD)) {
    pwmReady = false;
    Serial.println("AP setup failed; motors disabled. Reset to retry.");
    return;
  }
  http.on("/", HTTP_GET, [] {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });
  http.on("/status", HTTP_GET, [] {
    http.sendHeader("Cache-Control", "no-store");
    http.send(200, "application/json", Config::MOTOR_WIRING_CONFIRMED ?
              "{\"motorsEnabled\":true}" : "{\"motorsEnabled\":false}");
  });
  http.onNotFound([] { http.send(404, "text/plain", "Not found"); });
  http.begin();
  socketServer.begin();
  socketServer.onEvent(onSocket);
  socketServer.enableHeartbeat(1000, 500, 2);
  Serial.printf("RobotCar: http://192.168.4.1 — motor output %s\n",
                Config::MOTOR_WIRING_CONFIRMED ? "ENABLED" : "DISABLED (verify config.h)");
}

void loop() {
  if (pwmReady) {
    socketServer.loop();
    http.handleClient();
  }
  delay(1);
}
