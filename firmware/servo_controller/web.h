#pragma once

// HTTP + WebSocket server (ESP-IDF esp_http_server, built into the ESP32 core; no extra libraries).
//   /            the UI (ui/index.html, gzipped into index_html_gz.h by tools/build_ui.py)
//   /api/status  telemetry as JSON
//   /ws          WebSocket. ESP32 -> browser: telemetry JSON every TELEMETRY_INTERVAL_MS,
//                and events {"ev":"text","lvl":"ok|warn|err"}. Browser -> ESP32: commands,
//                e.g. {"c":"jog","dir":1,"spd":30} (see handleCommand in web.cpp).
namespace web {

bool begin();
// Call from loop() every TELEMETRY_INTERVAL_MS: sends telemetry to all WebSocket clients.
void publishTelemetry();
// Shows a short message (toast) in every connected browser.
void publishEvent(const char *text, const char *level);

}  // namespace web
