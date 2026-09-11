#pragma once

// HTTP + WebSocket server (ESP-IDF esp_http_server, built into the ESP32 core; no extra libraries).
//   /            the UI (ui/index.html, gzipped into index_html_gz.h by tools/build_ui.py)
//   /api/status  telemetry as JSON
//   /ws          WebSocket; the ESP32 pushes telemetry JSON at TELEMETRY_INTERVAL_MS
namespace web {

bool begin();
// Call from loop() every TELEMETRY_INTERVAL_MS: sends telemetry to all WebSocket clients.
void publishTelemetry();

}  // namespace web
