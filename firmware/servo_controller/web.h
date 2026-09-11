#pragma once

// HTTP server (ESP-IDF esp_http_server, built into the ESP32 core; no extra libraries).
// Step 1: a plain status page at / and JSON at /api/status.
namespace web {

bool begin();

}  // namespace web
