#include "WebUI.h"

#include <AsyncJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>

#include "AlarmScheduler.h"
#include "AudioEngine.h"
#include "LightEngine.h"
#include "SleepMonitor.h"

void WebUI::begin(AlarmScheduler& sched, LightEngine& light, AudioEngine& audio, SleepMonitor& sleep, std::mutex& lock,
                  const float& lux, bool clock) {
  sched_ = &sched;
  light_ = &light;
  audio_ = &audio;
  sleep_ = &sleep;
  lock_ = &lock;
  lux_ = &lux;
  clock_ = clock;

  const Settings& s = sched.settings;
  WiFi.persistent(false);  // credentials live in our NVS settings
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  if (s.ssid[0]) WiFi.begin(s.ssid, s.pass);
  else startAp();  // first boot: nothing to join
  lastSta_ = millis();
#ifndef WOKWI  // the simulator's RTC starts at the PC's local time; real UTC from SNTP would shift the test alarm by hours
  configTzTime(s.tz, "pool.ntp.org", "time.google.com");  // SNTP runs whenever the STA link is up
#endif
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  if (!LittleFS.begin(false)) log_e("LittleFS missing: run `pio run -t uploadfs`");

  server_.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* r) {
    if (authed(r)) sendState(r);  // occupancy is private once an admin password is set
  });

  server_.on("/api/sleep", HTTP_GET, [this](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    JsonDocument doc;
    {
      std::lock_guard<std::mutex> g(*lock_);
      sleep_->liveJson(doc["live"].to<JsonObject>());
      sleep_->nightsJson(doc["nights"].to<JsonArray>());
    }
    String out;
    serializeJson(doc, out);
    r->send(200, "application/json", out);
  });

  post("/api/sleep", [this](JsonVariant& b) {  // {"clear": true} wipes the history
    if (!(b["clear"] | false)) return false;
    sleep_->clear();
    return true;
  });

  post("/api/alarms", [this](JsonVariant& b) { return sched_->setAlarms(b.as<JsonArrayConst>()); });

  post("/api/settings", [this](JsonVariant& b) {
    bool wifi = false;
    if (!sched_->applySettings(b.as<JsonObjectConst>(), &wifi)) return false;
    const Settings& st = sched_->settings;
    light_->setNight(light_->nightOn(), st.nightPct, st.nightKelvin);
    if (wifi) restartAt_ = millis() + 1500;  // rejoin with the new credentials
    return true;
  });

  post("/api/light", [this](JsonVariant& b) {
    JsonObjectConst o = b.as<JsonObjectConst>();
    if (!o["on"].is<bool>() || !sched_->applySettings(o, nullptr)) return false;  // + nightPct / nightKelvin
    const Settings& st = sched_->settings;
    light_->setNight(o["on"].as<bool>(), st.nightPct, st.nightKelvin);
    return true;
  });

  post("/api/sound", [this](JsonVariant& b) {
    JsonObjectConst o = b.as<JsonObjectConst>();
    if (!sched_->applySettings(o, nullptr)) return false;  // soundVolume / alarmVolume
    if (o["stop"] | false) {
      audio_->stop(1000);
      audio_->setSleepTimer(0);
      return true;
    }
    JsonVariantConst play = o["play"], timer = o["timer"];
    if (!timer.isNull() && (!timer.is<int>() || timer.as<int>() < 0 || timer.as<int>() > 720)) return false;
    if (!play.isNull() && (!play.is<const char*>() || !audio_->play(play.as<String>()))) return false;
    if (!timer.isNull()) audio_->setSleepTimer((uint16_t)timer.as<int>());
    if (audio_->source() != AudioEngine::Source::Chime) audio_->setVolume(sched_->settings.soundVolume);
    return true;
  });

  post("/api/time", [this](JsonVariant& b) {
    JsonVariantConst e = b["epoch"];
    if (!e.is<int64_t>() || e.as<int64_t>() < 1704067200 || e.as<int64_t>() > 2145916800) return false;  // 2024-2037
    sched_->setTime((time_t)e.as<int64_t>());
    return true;
  });

  post("/api/alarm", [this](JsonVariant& b) {
    const char* action = b["action"] | "";
    if (!strcmp(action, "snooze")) sched_->snooze();
    else if (!strcmp(action, "dismiss")) sched_->dismiss();
    else return false;
    return true;
  });

  server_.on(
      "/update", HTTP_POST,
      [this](AsyncWebServerRequest* r) {
        if (!authed(r)) return;
        if (!passSet()) {
          r->send(403, "text/plain", "Set an admin password first");
          return;
        }
        const bool ok = Update.isFinished() && !Update.hasError();
        r->send(ok ? 200 : 500, "text/plain", ok ? "OK, rebooting" : Update.errorString());
        std::lock_guard<std::mutex> g(*lock_);
        if (ok) restartAt_ = millis() + 1000;
      },
      [this](AsyncWebServerRequest* r, const String&, size_t index, uint8_t* data, size_t len, bool final) {
        if (!index) {
          if (!passSet() || !authOk(r)) return;  // checked first: a stranger can't abort a running update
          if (Update.isRunning()) Update.abort();
          const bool fs = r->hasParam("fs");
          if (fs) LittleFS.end();
          Update.begin(UPDATE_SIZE_UNKNOWN, fs ? U_SPIFFS : U_FLASH);  // LittleFS uses the "spiffs" partition
          otaReq_ = r;
          r->onDisconnect([this, r]() {  // r is freed after this: its address must never stay a credential
            if (otaReq_ != r) return;
            otaReq_ = nullptr;
            if (Update.isRunning()) Update.abort();  // cut off mid-upload
          });
        }
        if (r != otaReq_ || !Update.isRunning()) return;  // only the request that passed auth may write
        Update.write(data, len);
        if (final) {
          Update.end(true);  // verifies the image; the old firmware stays on failure
          otaReq_ = nullptr;
        }
      });

  server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server_.onNotFound([](AsyncWebServerRequest* r) { r->send(404, "text/plain", "Not found (web UI missing? pio run -t uploadfs)"); });
  server_.begin();
}

void WebUI::loop() {
  if (restartAt_ && (int32_t)(millis() - restartAt_) >= 0) ESP.restart();
  if (WiFi.status() == WL_CONNECTED) {
    lastSta_ = millis();
    if (apOn_ && WiFi.softAPgetStationNum() == 0) {
      WiFi.softAPdisconnect(true);
      apOn_ = false;
    }
  } else if (!apOn_ && millis() - lastSta_ > 30000) {
    startAp();  // STA keeps retrying in the background
  }
}

void WebUI::startAp() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);  // http://192.168.4.1
  apOn_ = true;
}

// Host must name this clock (blocks DNS rebinding) and a browser's Origin must match it (blocks CSRF).
bool WebUI::sameOrigin(AsyncWebServerRequest* r) {
  String host = r->host();
  host.toLowerCase();
  const int colon = host.indexOf(':');
  const String name = colon < 0 ? host : host.substring(0, colon);
#ifdef WOKWI
  if (name == "localhost") return true;  // the simulator's port forward; never in a real build
#endif
  if (name != WiFi.localIP().toString() && name != WiFi.softAPIP().toString() && name != HOSTNAME &&
      name != String(HOSTNAME) + ".local")
    return false;
  return !r->hasHeader("Origin") || r->header("Origin").equalsIgnoreCase(String("http://") + host);
}

bool WebUI::passSet() {
  std::lock_guard<std::mutex> g(*lock_);
  return sched_->settings.adminPass[0] != 0;
}

bool WebUI::authOk(AsyncWebServerRequest* r) {
  char pass[sizeof(Settings::adminPass)];
  {
    std::lock_guard<std::mutex> g(*lock_);
    memcpy(pass, sched_->settings.adminPass, sizeof pass);
  }
  return sameOrigin(r) && (!pass[0] || r->authenticate("admin", pass));
}

bool WebUI::authed(AsyncWebServerRequest* r) {
  if (authOk(r)) return true;
  if (sameOrigin(r)) r->requestAuthentication();
  else r->send(403, "text/plain", "Forbidden");
  return false;
}

void WebUI::post(const char* path, std::function<bool(JsonVariant&)> fn) {
  server_.addHandler(new AsyncCallbackJsonWebHandler(path, [this, fn](AsyncWebServerRequest* r, JsonVariant& body) {
    if (!authed(r)) return;
    bool ok;
    {
      std::lock_guard<std::mutex> g(*lock_);
      ok = fn(body);
    }
    r->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"invalid\"}");
  }));
}

void WebUI::sendState(AsyncWebServerRequest* r) {
  static const char* const kPhase[] = {"idle", "sunrise", "ringing", "snoozed"};
  static const char* const kSource[] = {"off", "sound", "chime"};
  JsonDocument doc;
  {
    std::lock_guard<std::mutex> g(*lock_);
    const time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    char local[32];
    strftime(local, sizeof local, "%a %Y-%m-%d %H:%M:%S", &lt);
    doc["fw"] = FW_VERSION;
    doc["time"] = (int64_t)now;
    doc["local"] = local;
    doc["timeValid"] = sched_->timeValid();
    doc["rtc"] = sched_->rtcOk();
    doc["phase"] = kPhase[(int)sched_->phase()];
    int32_t away = 0;
    doc["next"]["index"] = sched_->next(away);
    doc["next"]["in"] = away;
    alarmsToJson(sched_->alarms, sched_->count, doc["alarms"].to<JsonArray>());
    sched_->settingsJson(doc["settings"].to<JsonObject>(), false);
    doc["auth"] = sched_->settings.adminPass[0] != 0;
    doc["light"]["on"] = light_->nightOn();
    doc["light"]["clock"] = clock_;
    sleep_->liveJson(doc["sleep"].to<JsonObject>());
    if (*lux_ >= 0) doc["light"]["lux"] = *lux_;
    JsonObject au = doc["audio"].to<JsonObject>();
    au["source"] = kSource[(int)audio_->source()];
    au["current"] = audio_->current();
    au["volume"] = audio_->volume();
    au["timer"] = audio_->sleepTimerLeft();
    au["sd"] = audio_->sdOk();
    JsonArray snd = au["sounds"].to<JsonArray>();
    for (const String& n : audio_->sounds()) snd.add(n);
  }
  JsonObject w = doc["wifi"].to<JsonObject>();
  const bool sta = WiFi.status() == WL_CONNECTED;
  w["connected"] = sta;
  w["ap"] = apOn_;
  w["ssid"] = sta ? WiFi.SSID() : String();
  w["ip"] = sta ? WiFi.localIP().toString() : String();
  if (sta) w["rssi"] = WiFi.RSSI();
  String out;
  serializeJson(doc, out);
  r->send(200, "application/json", out);
}
