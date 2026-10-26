#include "web_dashboard.h"
#include "web_pages.h"

static const IPAddress kApIp(192, 168, 4, 1);
static const char *kSessionCookie = "CHRONOS_SID";
static const uint16_t kDurationOptions[] = ALARM_DURATION_OPTIONS_SEC;

// Appends s as a JSON string literal. Names/labels are printable ASCII only (sanitized on
// save), so only quote and backslash need escaping.
static void appendJsonString(String &out, const char *s) {
  out += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    out += *s;
  }
  out += '"';
}

// Copies printable ASCII from in to out (max outLen-1 chars), trimming surrounding spaces.
static void sanitizeText(const String &in, char *out, size_t outLen) {
  String t = in;
  t.trim();
  size_t n = 0;
  for (size_t i = 0; i < t.length() && n < outLen - 1; i++) {
    char c = t[i];
    if (c >= 0x20 && c < 0x7F) out[n++] = c;
  }
  out[n] = '\0';
}

void WebDashboard::begin(AlarmManager *alarms, TimeManager *time, LabNames *labNames,
                         BuzzerManager *buzzers, DisplayManager *display) {
  alarms_ = alarms;
  time_ = time;
  labNames_ = labNames;
  buzzers_ = buzzers;
  display_ = display;

  prefs_.begin(NVS_NAMESPACE, false);
  adminPassword_ = prefs_.getString("webpass", WEB_ADMIN_DEFAULT_PASSWORD);
  prefs_.end();

  // Log client join/leave/DHCP so a failed connection shows up on the serial monitor.
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("[Web] Client joined %02x:%02x:%02x:%02x:%02x:%02x\n",
                    info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                    info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                    info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println("[Web] Client left");
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      Serial.printf("[Web] Client got IP %s\n",
                    IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
      break;
    default:
      break;
    }
  });

  // Don't reuse Wi-Fi config cached in NVS by earlier firmware; start clean every boot.
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  bool apOk = WiFi.softAP(WIFI_AP_SSID, strlen(WIFI_AP_PASSWORD) ? WIFI_AP_PASSWORD : nullptr,
                          WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CLIENTS);
  // The AP must be up before its IP/DHCP range is changed, or clients can associate but
  // never get an address.
  delay(100);
  bool cfgOk = WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
  Serial.printf("[Web] softAPConfig %s, DMA free %u largest %u, internal free %u largest %u\n",
                cfgOk ? "ok" : "FAILED", heap_caps_get_free_size(MALLOC_CAP_DMA),
                heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  // Full TX power draws ~300 mA spikes; on the same 5 V rail as two HUB75 panels that sags
  // the 3.3 V supply and the radio stops working reliably.
  WiFi.setTxPower(WIFI_AP_TX_POWER);
  Serial.printf("[Web] Access point \"%s\" %s on ch %d, dashboard at http://%s\n", WIFI_AP_SSID,
                apOk ? "started" : "FAILED", WIFI_AP_CHANNEL, WiFi.softAPIP().toString().c_str());

  // Answer every DNS lookup with our own IP so phones pop up the captive-portal page.
  dns_.start(53, "*", kApIp);

  const char *headers[] = {"Cookie"};
  server_.collectHeaders(headers, 1);
  setupRoutes();
  server_.begin();

  // Core 0 alongside the Wi-Fi stack, below timeAlarmTask's priority (2) so alarm timing
  // always wins.
  xTaskCreatePinnedToCore(taskEntry, "WebTask", 8192, this, 1, nullptr, 0);
}

void WebDashboard::taskEntry(void *self) {
  WebDashboard *w = static_cast<WebDashboard *>(self);
  for (;;) {
    w->dns_.processNextRequest();
    w->server_.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void WebDashboard::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() { server_.send_P(200, "text/html", kWelcomeHtml); });
  server_.on("/style.css", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "max-age=86400");
    server_.send_P(200, "text/css", kStyleCss);
  });
  server_.on("/login", HTTP_GET, [this]() {
    if (isAuthed()) {
      server_.sendHeader("Location", "/dashboard");
      server_.send(302, "text/plain", "");
      return;
    }
    server_.send_P(200, "text/html", kLoginHtml);
  });
  server_.on("/login", HTTP_POST, [this]() { handleLoginPost(); });
  server_.on("/logout", HTTP_GET, [this]() { handleLogout(); });
  server_.on("/dashboard", HTTP_GET, [this]() {
    if (!isAuthed()) {
      server_.sendHeader("Location", "/login");
      server_.send(302, "text/plain", "");
      return;
    }
    server_.send_P(200, "text/html", kDashboardHtml);
  });

  server_.on("/api/info", HTTP_GET, [this]() { handlePublicInfo(); });
  server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
  server_.on("/api/alarm", HTTP_POST, [this]() { handleSaveAlarm(); });
  server_.on("/api/labname", HTTP_POST, [this]() { handleSaveLabName(); });
  server_.on("/api/password", HTTP_POST, [this]() { handleChangePassword(); });
  server_.on("/api/diag", HTTP_GET, [this]() { handleDiag(); });
  server_.on("/api/timecfg", HTTP_POST, [this]() { handleTimeConfig(); });
  server_.on("/api/settime", HTTP_POST, [this]() { handleSetTime(); });
  server_.on("/api/beep", HTTP_POST, [this]() { handleTestBeep(); });
  server_.on("/api/brightness", HTTP_POST, [this]() { handleBrightness(); });
  server_.on("/api/volume", HTTP_POST, [this]() { handleVolume(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

// ---------------------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------------------
bool WebDashboard::isAuthed() {
  if (!server_.hasHeader("Cookie")) return false;
  String cookie = server_.header("Cookie");
  String prefix = String(kSessionCookie) + "=";
  int start = cookie.indexOf(prefix);
  if (start < 0) return false;
  start += prefix.length();
  int end = cookie.indexOf(';', start);
  String token = cookie.substring(start, end < 0 ? cookie.length() : end);
  token.trim();
  if (token.length() == 0) return false;
  for (const String &s : sessions_) {
    if (s.length() && s == token) return true;
  }
  return false;
}

bool WebDashboard::requireAuthApi() {
  if (isAuthed()) return true;
  sendError(401, "Not logged in");
  return false;
}

void WebDashboard::handleLoginPost() {
  if (server_.arg("username") != WEB_ADMIN_USER || server_.arg("password") != adminPassword_) {
    server_.sendHeader("Location", "/login?e=1");
    server_.send(302, "text/plain", "");
    return;
  }
  char token[33];
  for (uint8_t i = 0; i < 4; i++) snprintf(token + i * 8, 9, "%08lx", (unsigned long)esp_random());
  // Oldest session is replaced once WEB_MAX_SESSIONS devices are logged in.
  sessions_[nextSession_] = token;
  nextSession_ = (nextSession_ + 1) % WEB_MAX_SESSIONS;

  server_.sendHeader("Set-Cookie", String(kSessionCookie) + "=" + token + "; Path=/; HttpOnly");
  server_.sendHeader("Location", "/dashboard");
  server_.send(302, "text/plain", "");
}

void WebDashboard::handleLogout() {
  if (isAuthed()) {
    String cookie = server_.header("Cookie");
    for (String &s : sessions_) {
      if (s.length() && cookie.indexOf(s) >= 0) s = "";
    }
  }
  server_.sendHeader("Set-Cookie", String(kSessionCookie) + "=; Path=/; Max-Age=0");
  server_.sendHeader("Location", "/");
  server_.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------------------
void WebDashboard::handlePublicInfo() {
  char timeBuf[16], dateBuf[24], lab0[LAB_NAME_MAX_LEN + 1], lab1[LAB_NAME_MAX_LEN + 1];
  time_->formatTime(timeBuf, sizeof(timeBuf));
  time_->formatDate(dateBuf, sizeof(dateBuf));
  labNames_->get(0, lab0, sizeof(lab0));
  labNames_->get(1, lab1, sizeof(lab1));

  String json = "{\"time\":";
  appendJsonString(json, timeBuf);
  json += ",\"date\":";
  appendJsonString(json, dateBuf);
  json += ",\"gps\":";
  json += time_->isGpsLocked() ? "true" : "false";
  json += ",\"rtc\":";
  json += time_->isRtcPresent() ? "true" : "false";
  json += ",\"labs\":[";
  appendJsonString(json, lab0);
  json += ',';
  appendJsonString(json, lab1);
  json += "]}";
  server_.send(200, "application/json", json);
}

void WebDashboard::handleState() {
  if (!requireAuthApi()) return;

  char lab0[LAB_NAME_MAX_LEN + 1], lab1[LAB_NAME_MAX_LEN + 1];
  labNames_->get(0, lab0, sizeof(lab0));
  labNames_->get(1, lab1, sizeof(lab1));

  String json;
  json.reserve(7000);
  json = "{\"labs\":[";
  appendJsonString(json, lab0);
  json += ',';
  appendJsonString(json, lab1);
  json += "],\"alarms\":[";
  for (uint8_t t = 0; t < 2; t++) {
    if (t) json += ',';
    json += '[';
    for (uint8_t s = 1; s <= MAX_ALARMS_PER_LAB; s++) {
      Alarm a = alarms_->getAlarm(alarms_->slotIndexForTarget((AlarmTarget)t, s));
      a.label[ALARM_LABEL_MAX_LEN - 1] = '\0';
      if (s > 1) json += ',';
      char buf[96];
      snprintf(buf, sizeof(buf), "{\"en\":%d,\"h\":%u,\"m\":%u,\"d\":%u,\"dur\":%u,\"c\":%u,\"l\":",
               a.enabled ? 1 : 0, a.hour, a.minute, a.dayMask, a.durationSec, a.chime);
      json += buf;
      appendJsonString(json, a.label);
      json += '}';
    }
    json += ']';
  }
  json += "]}";
  server_.send(200, "application/json", json);
}

void WebDashboard::handleSaveAlarm() {
  if (!requireAuthApi()) return;

  int target = server_.arg("t").toInt();
  int uiSlot = server_.arg("s").toInt();
  int hour = server_.arg("h").toInt();
  int minute = server_.arg("m").toInt();
  int dayMask = server_.arg("d").toInt();
  int duration = server_.arg("dur").toInt();
  int chime = server_.arg("c").toInt();

  if (target < 0 || target > 1) return sendError(400, "Invalid lab");
  int idx = alarms_->slotIndexForTarget((AlarmTarget)target, (uint8_t)uiSlot);
  if (idx < 0) return sendError(400, "Invalid slot");
  if (!server_.hasArg("h") || hour < 0 || hour > 23 || minute < 0 || minute > 59)
    return sendError(400, "Invalid time");
  if (dayMask < 0 || dayMask > DAYMASK_DAILY) return sendError(400, "Invalid days");
  if (chime < 0 || chime >= CHIME_STYLE_COUNT) return sendError(400, "Invalid chime");
  bool durOk = false;
  for (uint16_t d : kDurationOptions) durOk |= (d == duration);
  if (!durOk) return sendError(400, "Invalid duration");

  Alarm a;
  a.enabled = server_.arg("en") == "1";
  if (a.enabled && dayMask == 0) return sendError(400, "Pick at least one day");
  a.target = (AlarmTarget)target;
  a.hour = hour;
  a.minute = minute;
  a.dayMask = dayMask;
  a.durationSec = duration;
  a.chime = (ChimeStyle)chime;
  sanitizeText(server_.arg("l"), a.label, sizeof(a.label));
  if (a.label[0] == '\0') strlcpy(a.label, "ALARM", sizeof(a.label));

  alarms_->saveAlarm(idx, a);
  Serial.printf("[Web] Saved alarm target=%d slot=%d %02d:%02d en=%d \"%s\"\n", target, uiSlot,
                hour, minute, a.enabled, a.label);
  sendJsonOk();
}

void WebDashboard::handleSaveLabName() {
  if (!requireAuthApi()) return;
  int lab = server_.arg("lab").toInt();
  if (!server_.hasArg("lab") || lab < 0 || lab > 1) return sendError(400, "Invalid lab");
  char name[LAB_NAME_MAX_LEN + 1];
  sanitizeText(server_.arg("name"), name, sizeof(name));
  if (!labNames_->set(lab, name)) return sendError(400, "Name cannot be empty");
  Serial.printf("[Web] Lab %d renamed to \"%s\"\n", lab + 1, name);
  sendJsonOk();
}

void WebDashboard::handleChangePassword() {
  if (!requireAuthApi()) return;
  if (server_.arg("cur") != adminPassword_) return sendError(400, "Current password is wrong");
  String pw = server_.arg("new");
  if (pw.length() < 4 || pw.length() > 32) return sendError(400, "New password must be 4-32 chars");
  adminPassword_ = pw;
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putString("webpass", pw);
  prefs_.end();
  sendJsonOk();
}

// Live status for the Diagnostics and Time & Date pages (same data as the panel's GPS
// monitor / brightness / timezone screens, plus system health).
void WebDashboard::handleDiag() {
  if (!requireAuthApi()) return;

  DateTime now = time_->now();
  bool locked = time_->isGpsLocked();
  const char *fix = locked ? "LOCKED" : (time_->isGpsReceivingData() ? "NO FIX" : "NO DATA");
  float hdop = time_->gpsHdop();
  char hdopBuf[12];
  if (hdop >= 99.9f) strlcpy(hdopBuf, "null", sizeof(hdopBuf));
  else snprintf(hdopBuf, sizeof(hdopBuf), "%.1f", hdop);

  char buf[384];
  snprintf(buf, sizeof(buf),
           "{\"fix\":\"%s\",\"sats\":%u,\"hdop\":%s,\"rtc\":%s,\"bright\":%u,\"vol\":%u,\"tz\":%d,"
           "\"h12\":%s,\"y\":%u,\"mo\":%u,\"d\":%u,\"h\":%u,\"mi\":%u,\"s\":%u,\"up\":%lu,"
           "\"heap\":%u,\"dma\":%u,\"dmaMin\":%u,\"clients\":%u}",
           fix, time_->satelliteCount(), hdopBuf, time_->isRtcPresent() ? "true" : "false",
           display_->getBrightness(), buzzers_->volume(), time_->timezoneOffsetMinutes(),
           time_->use12HourFormat() ? "true" : "false", now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(), (unsigned long)(millis() / 1000),
           ESP.getFreeHeap(), heap_caps_get_free_size(MALLOC_CAP_DMA),
           heap_caps_get_minimum_free_size(MALLOC_CAP_DMA), WiFi.softAPgetStationNum());
  server_.send(200, "application/json", buf);
}

// Timezone offset and/or 12h/24h format; either field may be sent alone.
void WebDashboard::handleTimeConfig() {
  if (!requireAuthApi()) return;
  if (server_.hasArg("tz")) {
    int tz = server_.arg("tz").toInt();
    if (tz < -12 * 60 || tz > 14 * 60 || tz % 15 != 0) return sendError(400, "Invalid timezone");
    time_->setTimezoneOffsetMinutes(tz);
  }
  if (server_.hasArg("h12")) time_->setUse12HourFormat(server_.arg("h12") == "1");
  sendJsonOk();
}

// Same as the panel's Manual Set Time: local date/time, written to the RTC.
void WebDashboard::handleSetTime() {
  if (!requireAuthApi()) return;
  if (!time_->isRtcPresent()) return sendError(400, "No RTC found, time cannot be stored");
  int y = server_.arg("y").toInt(), mo = server_.arg("mo").toInt(), d = server_.arg("d").toInt();
  int h = server_.arg("h").toInt(), mi = server_.arg("mi").toInt();
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || h < 0 || h > 23 || mi < 0 || mi > 59 ||
      !server_.hasArg("h"))
    return sendError(400, "Invalid date/time");
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
  int maxDay = (mo == 2 && leap) ? 29 : kDays[mo - 1];
  if (d < 1 || d > maxDay) return sendError(400, "Invalid date/time");
  time_->setManualDateTime(DateTime(y, mo, d, h, mi, 0));
  Serial.printf("[Web] Time set to %04d-%02d-%02d %02d:%02d\n", y, mo, d, h, mi);
  sendJsonOk();
}

void WebDashboard::handleTestBeep() {
  if (!requireAuthApi()) return;
  int lab = server_.arg("lab").toInt();
  if (!server_.hasArg("lab") || lab < 0 || lab > 1) return sendError(400, "Invalid speaker");
  buzzers_->requestTestBeep(lab);
  sendJsonOk();
}

void WebDashboard::handleBrightness() {
  if (!requireAuthApi()) return;
  int b = server_.arg("b").toInt();
  if (b < 1 || b > 255) return sendError(400, "Brightness must be 1-255");
  display_->requestBrightness(b);
  sendJsonOk();
}

void WebDashboard::handleVolume() {
  if (!requireAuthApi()) return;
  int v = server_.arg("v").toInt();
  if (!server_.hasArg("v") || v < 0 || v > 100) return sendError(400, "Volume must be 0-100");
  buzzers_->setVolume(v);
  sendJsonOk();
}

void WebDashboard::handleNotFound() {
  // Captive portal: any request for another host (phone connectivity checks, typed URLs)
  // is redirected to the welcome page.
  String host = server_.hostHeader();
  if (host.length() && host != kApIp.toString()) {
    server_.sendHeader("Location", String("http://") + kApIp.toString() + "/");
    server_.send(302, "text/plain", "");
    return;
  }
  server_.send(404, "text/plain", "Not found");
}

void WebDashboard::sendJsonOk() { server_.send(200, "application/json", "{\"ok\":true}"); }

void WebDashboard::sendError(int code, const char *msg) { server_.send(code, "text/plain", msg); }
