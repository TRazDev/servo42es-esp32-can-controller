#include "web.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include <lwip/sockets.h>
#include "config.h"
#include "control.h"
#include "index_html_gz.h"
#include "servo.h"
#include "settings.h"
#include "units.h"

namespace web {
namespace {

httpd_handle_t server = nullptr;
constexpr size_t MAX_CLIENTS = 8;

// Telemetry in joint units (see units.h). Keys are short because this goes out
// 20 times a second; ui/index.html reads them.
size_t telemetryJson(char *buf, size_t size) {
  const ServoState s = servo::snapshot();
  const control::Status c = control::status();
  const settings::Joint j = settings::joint();
  char fw[16] = "";
  if (s.versionKnown) snprintf(fw, sizeof(fw), "V%u.%u.%u", s.firmware[0], s.firmware[1], s.firmware[2]);
  char target[16] = "null";
  if (c.hasTarget) snprintf(target, sizeof(target), "%.3f", c.targetDeg);
  const int n = snprintf(buf, size,
      "{\"on\":%d,\"en\":%d,\"mode\":%d,\"run\":%u,\"alm\":%u,\"stall\":%d,\"homed\":%d,"
      "\"z\":%d,\"es\":%d,\"jog\":%d,\"tgt\":%s,\"cal\":%d,"
      "\"cnt\":%lld,\"pos\":%.3f,\"spd\":%.2f,\"err\":%.4f,\"gear\":%.4f,\"inv\":%d,\"maxrpm\":%u,"
      "\"fw\":\"%s\",\"hw\":%u,\"id\":%u,\"rssi\":%d,\"rx\":%lu,\"up\":%lu}",
      s.online, s.enabled, s.mode, s.runStatus, s.alarm, s.stalled, s.homed,
      c.zeroed, c.estop, c.jogging, target, c.calibrating,
      (long long)s.positionCounts,
      units::countsToDeg(s.positionCounts, j),
      units::rpmToDegPerS(s.speedRpm, j),
      units::errorToDeg(s.errorCounts, j),
      j.gear, j.invert, MAX_MOTOR_RPM, fw, s.hardware, MOTOR_ID, (int)WiFi.RSSI(),
      (unsigned long)s.rxFrames, (unsigned long)(millis() / 1000));
  return n > 0 && (size_t)n < size ? n : 0;
}

// ---- tiny parser for the flat command objects the UI sends ----
bool jsonString(const char *json, const char *key, char *out, size_t size) {
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char *p = strstr(json, pattern);
  if (!p) return false;
  p += strlen(pattern);
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < size) out[i++] = *p++;
  out[i] = '\0';
  return true;
}

float jsonNumber(const char *json, const char *key, float fallback) {
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char *p = strstr(json, pattern);
  if (!p) return fallback;
  char *end;
  const float v = strtof(p + strlen(pattern), &end);
  return end == p + strlen(pattern) || !isfinite(v) ? fallback : v;
}

void handleCommand(const char *json) {
  char name[16];
  if (!jsonString(json, "c", name, sizeof(name))) return;
  using control::Cmd;
  control::Command cmd{};
  if (!strcmp(name, "enable")) cmd.type = Cmd::Enable;
  else if (!strcmp(name, "disable")) cmd.type = Cmd::Disable;
  else if (!strcmp(name, "stop")) cmd.type = Cmd::Stop;
  else if (!strcmp(name, "estop")) cmd.type = Cmd::EStop;
  else if (!strcmp(name, "release")) cmd.type = Cmd::Release;
  else if (!strcmp(name, "zero")) cmd.type = Cmd::Zero;
  else if (!strcmp(name, "clearstall")) cmd.type = Cmd::ClearStall;
  else if (!strcmp(name, "getsettings")) cmd.type = Cmd::GetSettings;
  else if (!strcmp(name, "restart")) cmd.type = Cmd::RestartMotor;
  else if (!strcmp(name, "factoryreset")) cmd.type = Cmd::FactoryReset;
  else if (!strcmp(name, "calibrate")) cmd.type = Cmd::Calibrate;
  else if (!strcmp(name, "fixmode")) cmd.type = Cmd::FixMode;
  else if (!strcmp(name, "savesettings")) {
    cmd.type = Cmd::SaveSettings;
    cmd.gear = jsonNumber(json, "gear", NAN);
    cmd.invert = jsonNumber(json, "inv", 0) != 0;
    cmd.currentMa = (uint16_t)constrain(jsonNumber(json, "cur", 0), 0.0f, 65535.0f);
    cmd.stallOn = jsonNumber(json, "son", 1) != 0;
    cmd.stallTolDeg = jsonNumber(json, "stol", NAN);
    cmd.heartbeatMs = (uint32_t)constrain(jsonNumber(json, "hb", -1), -1.0f, 4.0e9f);
    if (isnan(cmd.gear) || isnan(cmd.stallTolDeg) || jsonNumber(json, "hb", -1) < 0) return;
  }
  else if (!strcmp(name, "jog")) {
    cmd.type = Cmd::Jog;
    const float dir = jsonNumber(json, "dir", 0);
    cmd.dir = dir > 0 ? 1 : dir < 0 ? -1 : 0;
    cmd.speed = jsonNumber(json, "spd", 0);
  } else if (!strcmp(name, "step") || !strcmp(name, "goto")) {
    cmd.type = name[0] == 's' ? Cmd::Step : Cmd::Goto;
    cmd.deg = jsonNumber(json, "deg", NAN);
    cmd.speed = jsonNumber(json, "spd", 0);
    cmd.accel = jsonNumber(json, "acc", 0);
    if (isnan(cmd.deg)) return;
  } else {
    return;
  }
  if (!control::enqueue(cmd)) publishEvent("COMMAND DROPPED / QUEUE FULL", "err");
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

esp_err_t handleWs(httpd_req_t *req) {
  if (req->method == HTTP_GET) return ESP_OK;  // handshake done
  httpd_ws_frame_t frame = {};
  esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
  if (err != ESP_OK || frame.len == 0) return err;
  if (frame.len > 256) return ESP_ERR_INVALID_SIZE;  // commands are tiny
  char payload[257];
  frame.payload = (uint8_t *)payload;
  err = httpd_ws_recv_frame(req, &frame, frame.len);
  if (err != ESP_OK) return err;
  payload[frame.len] = '\0';
  if (frame.type == HTTPD_WS_TYPE_TEXT) handleCommand(payload);
  return ESP_OK;
}

// A browser went away: stop any jog it may have been holding.
void onClose(httpd_handle_t hd, int sockfd) {
  if (httpd_ws_get_fd_info(hd, sockfd) == HTTPD_WS_CLIENT_WEBSOCKET) {
    control::Command cmd{};
    cmd.type = control::Cmd::ClientGone;
    control::enqueue(cmd);
  }
  close(sockfd);
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
  if (!server || !len) return;
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
  config.close_fn = onClose;
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
  char json[512];
  broadcast(json, telemetryJson(json, sizeof(json)));
}

void publishJson(const char *json) { broadcast(json, strlen(json)); }

void publishEvent(const char *text, const char *level) {
  char json[160];
  const int n = snprintf(json, sizeof(json), "{\"ev\":\"%s\",\"lvl\":\"%s\"}", text, level);
  if (n > 0 && (size_t)n < sizeof(json)) broadcast(json, n);
}

}  // namespace web
