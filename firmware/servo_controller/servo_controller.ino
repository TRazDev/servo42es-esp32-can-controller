// SERVO42ES controller firmware (ESP32-S3).
// Joins the home WiFi as servo-control.local, polls the motor over CAN and serves
// the control UI. Motion commands come from the browser over a WebSocket (control.cpp).
//
// Before building: copy secrets.h.example to secrets.h and fill in your WiFi details.
// Board settings (Arduino IDE): ESP32S3 Dev Module, Flash Size 16MB,
// PSRAM "OPI PSRAM", Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)",
// USB CDC On Boot "Disabled". Upload and serial monitor (115200) via the COM port.

#include <WiFi.h>
#include <ESPmDNS.h>
#include "config.h"
#include "can_bus.h"
#include "control.h"
#include "servo.h"
#include "settings.h"
#include "web.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h: copy secrets.h.example to secrets.h and fill in your WiFi name and password"
#endif

bool wifiWasConnected = false;
bool mdnsStarted = false;
uint32_t lastTelemetryMs = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nservo-controller starting");

  settings::begin();
  if (!can_bus::begin()) Serial.println("CAN: TWAI driver failed to start");
  servo::begin();
  control::begin();

  WiFi.mode(WIFI_STA);
  // Also answer IPv6 (AAAA) mDNS queries. Without it, macOS waits ~5 s for an
  // AAAA reply before falling back to IPv4 for servo-control.local.
  WiFi.enableIPv6();
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi: connecting to \"%s\"...\n", WIFI_SSID);

  if (!web::begin()) Serial.println("Web server failed to start");
}

void loop() {
  servo::update();
  control::update();

  if (millis() - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = millis();
    web::publishTelemetry();
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiWasConnected) {
    wifiWasConnected = connected;
    if (connected) {
      if (!mdnsStarted && MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
      }
      Serial.printf("WiFi: connected, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      Serial.printf("Open http://%s.local  (or http://%s)\n", HOSTNAME, WiFi.localIP().toString().c_str());
    } else {
      Serial.println("WiFi: disconnected, retrying...");
    }
  }
  delay(1);
}
