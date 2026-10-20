#pragma once
// Wi-Fi (STA, falls back to the setup AP), the LittleFS page, a JSON API and OTA upload.
// Handlers run on the async_tcp task, so every one takes the shared lock that loop() holds.
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <mutex>

class AlarmScheduler;
class AudioEngine;
class LightEngine;
class SleepMonitor;

class WebUI {
 public:
  void begin(AlarmScheduler& sched, LightEngine& light, AudioEngine& audio, SleepMonitor& sleep, std::mutex& lock,
             const float& lux, bool clock);
  void loop();  // call every loop

 private:
  bool sameOrigin(AsyncWebServerRequest* r);
  bool passSet();
  bool authOk(AsyncWebServerRequest* r);  // checks only
  bool authed(AsyncWebServerRequest* r);  // checks, and answers 401 when it fails
  void post(const char* path, std::function<bool(JsonVariant&)> fn);
  void sendState(AsyncWebServerRequest* r);
  void startAp();

  AsyncWebServer server_{80};
  AlarmScheduler* sched_ = nullptr;
  LightEngine* light_ = nullptr;
  AudioEngine* audio_ = nullptr;
  SleepMonitor* sleep_ = nullptr;
  std::mutex* lock_ = nullptr;
  const float* lux_ = nullptr;
  uint32_t lastSta_ = 0, restartAt_ = 0;
  bool apOn_ = false, clock_ = false;
  AsyncWebServerRequest* otaReq_ = nullptr;  // upload that passed auth; cleared when its connection closes
};
