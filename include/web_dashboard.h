#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config.h"
#include "alarm_manager.h"
#include "time_manager.h"
#include "lab_names.h"
#include "buzzer_manager.h"
#include "display_manager.h"

// "Chronos" Wi-Fi access point + web dashboard. Serves a welcome page, a login page and a
// password-protected dashboard whose sidebar mirrors the button menu: each channel's
// (Comlab 1 / Comlab 2) alarms, Time & Date, and Diagnostics (buzzer test, GPS
// monitor, brightness, speaker volume), plus lab names and the admin password. Runs in its own low-priority task on Core 0 (next to the
// Wi-Fi stack), so it never stalls the Core-1 display refresh.
class WebDashboard {
public:
  void begin(AlarmManager *alarms, TimeManager *time, LabNames *labNames, BuzzerManager *buzzers,
             DisplayManager *display);

private:
  AlarmManager *alarms_ = nullptr;
  TimeManager *time_ = nullptr;
  LabNames *labNames_ = nullptr;
  BuzzerManager *buzzers_ = nullptr;
  DisplayManager *display_ = nullptr;

  WebServer server_{80};
  DNSServer dns_;
  Preferences prefs_;

  String adminPassword_;
  String sessions_[WEB_MAX_SESSIONS];
  uint8_t nextSession_ = 0;

  static void taskEntry(void *self);
  void setupRoutes();

  bool isAuthed();
  bool requireAuthApi(); // sends 401 and returns false if not logged in

  void handleLoginPost();
  void handleLogout();
  void handlePublicInfo();
  void handleState();
  void handleSaveAlarm();
  void handleSaveLabName();
  void handleChangePassword();
  void handleDiag();
  void handleTimeConfig();
  void handleSetTime();
  void handleTestBeep();
  void handleBrightness();
  void handleVolume();
  void handleNotFound();

  void sendJsonOk();
  void sendError(int code, const char *msg);
};
