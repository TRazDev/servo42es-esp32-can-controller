#include "web.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include "servo.h"

namespace web {
namespace {

httpd_handle_t server = nullptr;

// Temporary page until the real UI from design/ is ported.
const char PAGE[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>servo-control</title>
<style>
body{font-family:ui-monospace,Menlo,Consolas,monospace;margin:24px;background:#fff;color:#111}
h1{font-size:18px}td{padding:4px 16px 4px 0}td:first-child{color:#888}
.off{color:#c8371c}
</style></head><body>
<h1>SERVO / J1 &middot; step 1 status page</h1>
<table id="t"></table>
<p id="err" class="off"></p>
<script>
async function tick(){
  try{
    const s = await (await fetch('/api/status',{cache:'no-store'})).json();
    const m = s.motor;
    const rows = [
      ['motor link', m.online ? 'OK' : '<span class="off">NO RESPONSE</span>'],
      ['firmware', m.firmware], ['hardware', m.hardware], ['mode', m.mode],
      ['enabled', m.enabled], ['position (motor)', m.position_deg.toFixed(2)+' deg  ('+m.position_counts+' counts)'],
      ['speed (motor)', m.speed_rpm+' rpm'], ['position error', m.error_deg.toFixed(3)+' deg'],
      ['run status', m.run_status], ['alarm', m.alarm], ['stalled', m.stalled],
      ['CAN frames received', m.rx_frames], ['WiFi RSSI', s.wifi_rssi+' dBm'], ['uptime', s.uptime_s+' s']];
    document.getElementById('t').innerHTML = rows.map(r=>'<tr><td>'+r[0]+'</td><td>'+r[1]+'</td></tr>').join('');
    document.getElementById('err').textContent = '';
  }catch(e){ document.getElementById('err').textContent = 'ESP32 not reachable'; }
}
setInterval(tick, 500); tick();
</script></body></html>)HTML";

esp_err_t handleRoot(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handleStatus(httpd_req_t *req) {
  const ServoState s = servo::snapshot();
  char fw[16] = "";
  if (s.versionKnown) snprintf(fw, sizeof(fw), "V%u.%u.%u", s.firmware[0], s.firmware[1], s.firmware[2]);
  char json[640];
  snprintf(json, sizeof(json),
           "{\"uptime_s\":%lu,\"wifi_rssi\":%d,\"motor\":{"
           "\"online\":%s,\"rx_frames\":%lu,\"firmware\":\"%s\",\"hardware\":%u,\"mode\":%d,"
           "\"enabled\":%s,\"position_counts\":%lld,\"position_deg\":%.3f,\"speed_rpm\":%d,"
           "\"error_deg\":%.4f,\"run_status\":%u,\"alarm\":%u,\"stalled\":%s}}",
           (unsigned long)(millis() / 1000), (int)WiFi.RSSI(),
           s.online ? "true" : "false", (unsigned long)s.rxFrames,
           fw, s.hardware, s.mode, s.enabled ? "true" : "false", (long long)s.positionCounts,
           s.positionCounts * 360.0 / 16384.0, s.speedRpm, s.errorCounts * 360.0 / 51200.0,
           s.runStatus, s.alarm, s.stalled ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

}  // namespace

bool begin() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&server, &config) != ESP_OK) return false;
  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = handleRoot, .user_ctx = nullptr};
  const httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = handleStatus, .user_ctx = nullptr};
  httpd_register_uri_handler(server, &root);
  httpd_register_uri_handler(server, &status);
  return true;
}

}  // namespace web
