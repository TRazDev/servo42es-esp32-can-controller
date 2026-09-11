#include "web.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include "config.h"
#include "index_html_gz.h"
#include "servo.h"

namespace web {
namespace {

httpd_handle_t server = nullptr;
constexpr size_t MAX_CLIENTS = 8;

// Telemetry in joint units (motor units divided by GEAR_RATIO). Keys are short
// because this goes out 20 times a second; ui/index.html reads them.
size_t telemetryJson(char *buf, size_t size) {
  const ServoState s = servo::snapshot();
  char fw[16] = "";
  if (s.versionKnown) snprintf(fw, sizeof(fw), "V%u.%u.%u", s.firmware[0], s.firmware[1], s.firmware[2]);
  const int n = snprintf(buf, size,
      "{\"on\":%d,\"en\":%d,\"mode\":%d,\"run\":%u,\"alm\":%u,\"stall\":%d,\"homed\":%d,"
      "\"cnt\":%lld,\"pos\":%.3f,\"spd\":%.2f,\"err\":%.4f,\"gear\":%.3f,"
      "\"fw\":\"%s\",\"hw\":%u,\"id\":%u,\"rssi\":%d,\"rx\":%lu,\"up\":%lu}",
      s.online, s.enabled, s.mode, s.runStatus, s.alarm, s.stalled, s.homed,
      (long long)s.positionCounts,
      s.positionCounts * 360.0 / 16384.0 / GEAR_RATIO,
      s.speedRpm * 6.0 / GEAR_RATIO,
      s.errorCounts * 360.0 / 51200.0 / GEAR_RATIO,
      GEAR_RATIO, fw, s.hardware, MOTOR_ID, (int)WiFi.RSSI(),
      (unsigned long)s.rxFrames, (unsigned long)(millis() / 1000));
  return n > 0 && (size_t)n < size ? n : 0;
}

esp_err_t handleRoot(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  return httpd_resp_send(req, (const char *)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
}

esp_err_t handleStatus(httpd_req_t *req) {
  char json[512];
  const size_t len = telemetryJson(json, sizeof(json));
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, len);
}

// Incoming WebSocket frames. Step 2a has no commands yet, so payloads are read and dropped.
esp_err_t handleWs(httpd_req_t *req) {
  if (req->method == HTTP_GET) return ESP_OK;  // handshake done
  httpd_ws_frame_t frame = {};
  esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
  if (err != ESP_OK || frame.len == 0) return err;
  uint8_t *payload = (uint8_t *)malloc(frame.len + 1);
  if (!payload) return ESP_ERR_NO_MEM;
  frame.payload = payload;
  err = httpd_ws_recv_frame(req, &frame, frame.len);
  free(payload);
  return err;
}

// WebSocket sends run inside the server task (httpd_queue_work), never from loop() directly.
struct WsJob {
  int fd;
  size_t len;
  char data[];
};

void sendJob(void *arg) {
  WsJob *job = static_cast<WsJob *>(arg);
  httpd_ws_frame_t frame = {};
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = (uint8_t *)job->data;
  frame.len = job->len;
  httpd_ws_send_frame_async(server, job->fd, &frame);
  free(job);
}

void broadcast(const char *text, size_t len) {
  size_t count = MAX_CLIENTS;
  int fds[MAX_CLIENTS];
  if (httpd_get_client_list(server, &count, fds) != ESP_OK) return;
  for (size_t i = 0; i < count; i++) {
    if (httpd_ws_get_fd_info(server, fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
    WsJob *job = (WsJob *)malloc(sizeof(WsJob) + len);
    if (!job) continue;
    job->fd = fds[i];
    job->len = len;
    memcpy(job->data, text, len);
    if (httpd_queue_work(server, sendJob, job) != ESP_OK) free(job);
  }
}

}  // namespace

bool begin() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 7;
  config.lru_purge_enable = true;  // drop the oldest idle connection instead of refusing new ones
  config.send_wait_timeout = 2;    // a slow client can't stall the server for long
  if (httpd_start(&server, &config) != ESP_OK) return false;

  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = handleRoot, .user_ctx = nullptr};
  const httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = handleStatus, .user_ctx = nullptr};
  httpd_uri_t ws = {};
  ws.uri = "/ws";
  ws.method = HTTP_GET;
  ws.handler = handleWs;
  ws.is_websocket = true;
  httpd_register_uri_handler(server, &root);
  httpd_register_uri_handler(server, &status);
  httpd_register_uri_handler(server, &ws);
  return true;
}

void publishTelemetry() {
  if (!server) return;
  char json[512];
  const size_t len = telemetryJson(json, sizeof(json));
  if (len) broadcast(json, len);
}

}  // namespace web
